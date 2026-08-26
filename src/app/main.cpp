#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "engine/AudioEngine.h"
#include "engine/ConfigLoader.h"
#include "engine/GameParameters.h"
#include "engine/MarkovChainVariationStrategy.h"
#include "engine/Mixer.h"
#include "engine/Pattern.h"
#include "engine/RandomSource.h"
#include "engine/SampleLoader.h"
#include "engine/Sequencer.h"
#include "engine/VariationEngine.h"

// Only the encoder half of miniaudio.h is used here (ma_encoder_*); the
// implementation itself is compiled exactly once, in
// src/engine/src/miniaudio_impl.cpp, so this translation unit just needs the
// declarations.
#include "miniaudio/miniaudio.h"

namespace {

using namespace pmg;

// A composition JSON file, parsed and pattern-resolved -- the unit that
// gets swapped in as "the currently playing track" both when streaming
// through a playlist and when hot-reloading a changed file. Kept as one
// bundle (rather than just a CompositionConfig) since Sequencer needs the
// already-ResolvePattern()'d Pattern list, not the raw PatternConfigs.
struct LoadedTrack {
    std::string path;
    CompositionConfig config;
    std::vector<Pattern> patterns;
};

bool LoadTrack(const std::string& path, LoadedTrack& out, std::string& errorOut) {
    CompositionConfig config;
    try {
        config = ConfigLoader::LoadFromFile(path);
    } catch (const std::exception& e) {
        errorOut = e.what();
        return false;
    }

    std::vector<Pattern> patterns;
    patterns.reserve(config.patterns.size());
    for (const PatternConfig& patternConfig : config.patterns) {
        patterns.push_back(ResolvePattern(patternConfig));
    }

    out.path = path;
    out.config = std::move(config);
    out.patterns = std::move(patterns);
    return true;
}

// Never throws: a file that's momentarily missing or unreadable (e.g. an
// editor doing an unlink-and-rewrite save) reports file_time_type::min()
// instead, which the hot-reload poll below just treats as "no change yet"
// rather than crashing the whole streaming loop.
std::filesystem::file_time_type SafeMtime(const std::string& path) {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(path, ec);
    return ec ? std::filesystem::file_time_type::min() : t;
}

// Every Mixer instrument a track needs, fully resolved (sample WAVs
// decoded) but not yet applied to a live Mixer -- see BuildInstrumentDefs.
// Keeping this as a staging step means a bad reload (e.g. a typo'd sample
// path) is caught before the previous, working track's instruments are
// torn out of the Mixer.
struct InstrumentDefs {
    std::vector<std::pair<std::string, SynthInstrumentDef>> synths;
    std::vector<std::pair<std::string, SampleInstrumentDef>> samples;
};

bool BuildInstrumentDefs(const CompositionConfig& config, uint32_t deviceSampleRate, InstrumentDefs& out,
                          std::string& errorOut) {
    out.synths.clear();
    out.samples.clear();
    for (const InstrumentConfig& instrument : config.instruments) {
        if (instrument.type == InstrumentType::Synth) {
            SynthInstrumentDef def;
            def.waveform = instrument.waveform;
            def.dutyCycle = instrument.dutyCycle;
            def.arpeggio = instrument.arpeggio;
            def.envelope = instrument.envelope;
            def.vibrato = instrument.vibrato;
            def.fm = instrument.fm;
            def.gain = instrument.gain;
            def.pan = instrument.pan;
            out.synths.emplace_back(instrument.id, def);
        } else {
            try {
                SampleInstrumentDef def;
                // Decoded at the audio device's actual sample rate, not
                // this track's own "sampleRate" field -- the device is
                // opened once, from the first track loaded, and every
                // later track (playlist advance or hot reload) keeps
                // playing at that same rate regardless of what it asks for.
                def.asset = LoadSampleAsset(instrument.file, deviceSampleRate);
                def.gain = instrument.gain;
                def.pan = instrument.pan;
                out.samples.emplace_back(instrument.id, def);
            } catch (const std::exception& e) {
                errorOut = "Failed to load sample instrument '" + instrument.id + "': " + e.what();
                return false;
            }
        }
    }
    return true;
}

void ApplyInstrumentDefs(Mixer& mixer, const InstrumentDefs& defs) {
    mixer.Reset();
    for (const auto& [id, def] : defs.synths) {
        mixer.AddSynthInstrument(id, def);
    }
    for (const auto& [id, def] : defs.samples) {
        mixer.AddSampleInstrument(id, def);
    }
}

std::unique_ptr<IVariationStrategy> MakeVariationStrategy(const CompositionConfig& config) {
    if (config.variationStrategy == VariationStrategyKind::MarkovChain) {
        return std::make_unique<MarkovChainVariationStrategy>(config.markovChain);
    }
    return nullptr; // VariationEngine defaults this to RuleBasedVariationStrategy
}

// Renders the composition offline (no audio device at all -- see
// AudioEngine::InitializeOffline) and writes it straight to a WAV file via
// miniaudio's encoder, chunk by chunk, so there's no need to hold the whole
// render in memory. Frames are counted the same way real-time playback would
// count them (AudioEngine::GetFramesProcessed()), just advanced by this
// loop's own RenderFrames() calls instead of a real-time device callback --
// so the exact same Sequencer/VariationEngine logic that drives normal
// playback drives the render, just as fast as the CPU can go rather than
// paced to a wall clock.
bool RenderCompositionToWav(pmg::AudioEngine& audioEngine, pmg::Sequencer& sequencer, const std::string& outputPath,
                             double seconds, uint32_t sampleRate, uint32_t channels) {
    // s16 (not f32) for maximum player compatibility -- some players/browsers
    // don't handle WAVE_FORMAT_IEEE_FLOAT. ma_encoder_write_pcm_frames writes
    // whatever bytes it's handed as-is (no implicit format conversion), so
    // this loop converts RenderFrames()'s f32 output (matching the real-time
    // device path) to s16 itself before each write.
    ma_encoder_config encoderConfig =
        ma_encoder_config_init(ma_encoding_format_wav, ma_format_s16, channels, sampleRate);
    ma_encoder encoder;
    if (ma_encoder_init_file(outputPath.c_str(), &encoderConfig, &encoder) != MA_SUCCESS) {
        std::cerr << "Failed to open '" << outputPath << "' for WAV output.\n";
        return false;
    }

    constexpr uint32_t kChunkFrames = 256;
    std::vector<float> floatBuffer(static_cast<size_t>(kChunkFrames) * channels);
    std::vector<int16_t> intBuffer(static_cast<size_t>(kChunkFrames) * channels);
    uint64_t totalFrames = static_cast<uint64_t>(seconds * sampleRate);

    for (uint64_t rendered = 0; rendered < totalFrames; rendered += kChunkFrames) {
        uint32_t framesThisChunk = static_cast<uint32_t>(std::min<uint64_t>(kChunkFrames, totalFrames - rendered));
        sequencer.Update();
        audioEngine.RenderFrames(floatBuffer.data(), framesThisChunk);

        size_t sampleCount = static_cast<size_t>(framesThisChunk) * channels;
        for (size_t i = 0; i < sampleCount; ++i) {
            float clamped = std::clamp(floatBuffer[i], -1.0f, 1.0f);
            intBuffer[i] = static_cast<int16_t>(clamped * 32767.0f);
        }

        ma_uint64 framesWritten = 0;
        ma_encoder_write_pcm_frames(&encoder, intBuffer.data(), framesThisChunk, &framesWritten);
    }

    ma_encoder_uninit(&encoder);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    using namespace pmg;

    bool useNullBackend = false;
    std::string renderWavPath;
    double renderSeconds = 30.0;
    double trackSeconds = 60.0; // how long each track streams before the playlist advances (only matters with >1 track)
    std::vector<std::string> trackPaths;
    // Initial runtime-parameter values (--param name=value), applied once
    // GameParameters exists and before anything reads from it -- the only
    // way to give a parameter a non-default starting value for a
    // --render-wav run, since that mode never reaches the interactive
    // stdin reader below.
    std::vector<std::pair<std::string, float>> initialParams;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--null-audio") {
            useNullBackend = true;
        } else if (arg == "--render-wav" && i + 1 < argc) {
            renderWavPath = argv[++i];
        } else if (arg == "--seconds" && i + 1 < argc) {
            renderSeconds = std::stod(argv[++i]);
        } else if (arg == "--track-seconds" && i + 1 < argc) {
            trackSeconds = std::stod(argv[++i]);
        } else if (arg == "--param" && i + 1 < argc) {
            std::string assignment = argv[++i];
            size_t eq = assignment.find('=');
            if (eq != std::string::npos) {
                initialParams.emplace_back(assignment.substr(0, eq), std::stof(assignment.substr(eq + 1)));
            } else {
                std::cerr << "Ignoring malformed --param '" << assignment << "' (expected name=value).\n";
            }
        } else {
            trackPaths.push_back(arg);
        }
    }
    if (trackPaths.empty()) {
        trackPaths.push_back("composition_demo.json");
    }

    std::cout.setf(std::ios::unitbuf); // flush eagerly so redirected/piped output stays live during the playback loop

    std::cout << "Procedural Music Generator - demo\n";

    LoadedTrack firstTrack;
    std::string loadErr;
    if (!LoadTrack(trackPaths[0], firstTrack, loadErr)) {
        std::cerr << "Failed to load " << trackPaths[0] << ": " << loadErr << "\n";
        return 1;
    }

    AudioEngine audioEngine;
    AudioEngineConfig audioConfig;
    audioConfig.sampleRate = firstTrack.config.sampleRate;
    audioConfig.channels = 2;
    audioConfig.useNullBackend = useNullBackend;
    audioConfig.loFi = firstTrack.config.loFi;
    audioConfig.delay = firstTrack.config.delay;
    bool renderingToWav = !renderWavPath.empty();
    bool initOk = renderingToWav ? audioEngine.InitializeOffline(audioConfig) : audioEngine.Initialize(audioConfig);
    if (!initOk) {
        std::cerr << "Failed to initialize audio device.\n";
        return 1;
    }

    Mixer& mixer = audioEngine.GetMixer();
    RandomSource randomSource(std::random_device{}());
    auto logCallback = [](const std::string& message) { std::cout << message << "\n"; };

    // Declared once from the first-loaded track and never rebuilt across a
    // track switch (playlist advance or hot reload), matching how the
    // device's sample rate/channel count are also fixed from the first
    // track -- a host game's "danger"/"intensity" state shouldn't reset
    // just because the underlying composition changed. A later track's own
    // "gameParameters" declarations are therefore only consulted this once.
    GameParameters gameParameters(firstTrack.config.gameParameters);
    for (const auto& [name, value] : initialParams) {
        if (!gameParameters.Has(name)) {
            std::cerr << "--param '" << name << "' isn't declared in " << trackPaths[0]
                       << "'s \"gameParameters\" -- ignoring.\n";
            continue;
        }
        gameParameters.Set(name, value);
    }

    LoadedTrack currentTrack;
    std::unique_ptr<VariationEngine> variationEngine;
    std::unique_ptr<Sequencer> sequencer;

    // Validates + applies newTrack as the currently playing track: decodes
    // its sample instruments first (so a broken track never tears down a
    // working one -- see BuildInstrumentDefs), then briefly stops the
    // device (Mixer/post-mix processors are audio-thread-owned and aren't
    // otherwise safe to mutate while the callback could fire concurrently
    // -- see AudioEngine::Reconfigure), swaps in the new instruments/
    // loFi/delay settings, rebuilds the VariationEngine+Sequencer for the
    // new composition, and restarts. Returns false (and logs why, keeping
    // whatever was playing before) if the new track doesn't load cleanly.
    auto ActivateTrack = [&](LoadedTrack newTrack) -> bool {
        InstrumentDefs defs;
        std::string instrErr;
        if (!BuildInstrumentDefs(newTrack.config, audioEngine.GetSampleRate(), defs, instrErr)) {
            std::cerr << "Failed to activate '" << newTrack.path << "': " << instrErr
                       << " -- keeping current track.\n";
            return false;
        }

        audioEngine.Stop();
        audioEngine.Reconfigure(newTrack.config.loFi, newTrack.config.delay);
        ApplyInstrumentDefs(mixer, defs);

        currentTrack = std::move(newTrack);
        variationEngine = std::make_unique<VariationEngine>(
            currentTrack.config.variationRules, gameParameters, MakeVariationStrategy(currentTrack.config));
        sequencer = std::make_unique<Sequencer>(audioEngine, *variationEngine, randomSource, gameParameters,
                                                 currentTrack.config, currentTrack.patterns);
        sequencer->SetVariationLogCallback(logCallback);
        audioEngine.Start();
        return true;
    };

    if (!ActivateTrack(std::move(firstTrack))) {
        std::cerr << "Failed to load initial track '" << trackPaths[0] << "'.\n";
        return 1;
    }

    if (renderingToWav) {
        std::cout << "Rendering " << trackPaths[0] << " -> " << renderWavPath << " (" << renderSeconds << "s)...\n";
        if (!RenderCompositionToWav(audioEngine, *sequencer, renderWavPath, renderSeconds, audioEngine.GetSampleRate(),
                                     audioConfig.channels)) {
            return 1;
        }
        std::cout << "Wrote " << renderWavPath << "\n";
        return 0;
    }

    if (trackPaths.size() > 1) {
        std::cout << "Streaming " << trackPaths.size() << " tracks (looping the playlist, " << trackSeconds
                   << "s each). Press Ctrl+C to stop.\n";
    } else {
        std::cout << "Playing " << trackPaths[0] << ". Press Ctrl+C to stop.\n";
    }
    std::cout << "Edit and save any track's JSON file to hot-reload it live.\n";

    // A minimal stand-in for a real game host: any thread can call
    // GameParameters::Set at any time (see GameParameters.h), so this just
    // reads "<name> <value>" lines from stdin on its own thread and calls
    // it directly -- exactly the API contract a real embedding game would
    // use, just typed by a human instead of computed from gameplay state.
    // Detached (not joined) since this process only ever exits via Ctrl+C;
    // harmless on a non-interactive stdin (piped/closed, e.g. under
    // --null-audio in a smoke test) since std::getline just returns false
    // immediately and the thread exits.
    if (!firstTrack.config.gameParameters.empty()) {
        std::cout << "Type '<parameter> <value>' + Enter to set a runtime parameter live (e.g. 'danger 0.8').\n";
    }
    std::thread paramInputThread([&gameParameters]() {
        std::string line;
        while (std::getline(std::cin, line)) {
            std::istringstream iss(line);
            std::string name;
            float value;
            if (!(iss >> name >> value)) {
                if (!line.empty()) {
                    std::cerr << "Malformed parameter input '" << line << "' (expected: <name> <value>).\n";
                }
                continue;
            }
            if (!gameParameters.Has(name)) {
                std::cerr << "Unknown runtime parameter '" << name << "' -- ignoring.\n";
                continue;
            }
            gameParameters.Set(name, value);
            std::cout << "Set " << name << " = " << value << "\n";
        }
    });
    paramInputThread.detach();

    size_t currentIndex = 0;
    std::filesystem::file_time_type currentMtime = SafeMtime(trackPaths[currentIndex]);
    std::filesystem::file_time_type lastFailedMtime = std::filesystem::file_time_type::min();
    auto trackStartTime = std::chrono::steady_clock::now();
    auto lastPollTime = trackStartTime;

    while (true) {
        sequencer->Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        auto now = std::chrono::steady_clock::now();

        // Hot reload: poll roughly once a second rather than every 5ms loop
        // iteration -- filesystem stats aren't free, and a track file
        // changes on the order of human edit-and-save, not audio-rate.
        if (now - lastPollTime >= std::chrono::seconds(1)) {
            lastPollTime = now;
            const std::string& path = trackPaths[currentIndex];
            std::filesystem::file_time_type mtime = SafeMtime(path);
            if (mtime != currentMtime && mtime != lastFailedMtime) {
                LoadedTrack reloaded;
                std::string err;
                if (LoadTrack(path, reloaded, err)) {
                    std::cout << "Detected change in " << path << ", hot-reloading...\n";
                    if (ActivateTrack(std::move(reloaded))) {
                        currentMtime = mtime;
                    } else {
                        lastFailedMtime = mtime; // don't retry until the file changes again
                    }
                } else {
                    std::cerr << "Hot reload failed for " << path << ": " << err
                               << " -- keeping current version, will retry if the file changes again.\n";
                    lastFailedMtime = mtime;
                }
            }
        }

        // Streaming: advance through the playlist, looping back to the
        // start after the last track, once the current one has played for
        // trackSeconds. A single-track "playlist" never advances -- it just
        // loops forever (as it always has), with hot reload still active.
        if (trackPaths.size() > 1 &&
            std::chrono::duration<double>(now - trackStartTime).count() >= trackSeconds) {
            currentIndex = (currentIndex + 1) % trackPaths.size();
            trackStartTime = now;
            lastFailedMtime = std::filesystem::file_time_type::min();

            LoadedTrack nextTrack;
            std::string err;
            if (LoadTrack(trackPaths[currentIndex], nextTrack, err)) {
                std::cout << "Advancing to " << trackPaths[currentIndex] << "\n";
                if (ActivateTrack(std::move(nextTrack))) {
                    currentMtime = SafeMtime(trackPaths[currentIndex]);
                }
            } else {
                std::cerr << "Failed to advance to " << trackPaths[currentIndex] << ": " << err
                           << " -- will retry this slot in " << trackSeconds << "s.\n";
                // currentTrack/sequencer are left exactly as they were (this
                // slot's ActivateTrack never ran), so playback just
                // continues uninterrupted on whatever was already active
                // until the next scheduled advance retries this same slot.
            }
        }
    }
}
