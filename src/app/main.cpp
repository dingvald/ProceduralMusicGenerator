#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "engine/AudioEngine.h"
#include "engine/ConfigLoader.h"
#include "engine/Mixer.h"
#include "engine/Pattern.h"
#include "engine/RandomSource.h"
#include "engine/SampleLoader.h"
#include "engine/Sequencer.h"
#include "engine/Theory.h"
#include "engine/VariationEngine.h"

int main(int argc, char** argv) {
    using namespace pmg;

    bool useNullBackend = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--null-audio") {
            useNullBackend = true;
        }
    }

    std::cout.setf(std::ios::unitbuf); // flush eagerly so redirected/piped output stays live during the playback loop

    std::cout << "Procedural Music Generator - demo\n";

    CompositionConfig config;
    try {
        config = ConfigLoader::LoadFromFile("composition_demo.json");
    } catch (const std::exception& e) {
        std::cerr << "Failed to load composition_demo.json: " << e.what() << "\n";
        return 1;
    }

    NoteName root = Theory::ParseNoteName(config.key.root);
    ScaleType scale = Theory::ParseScaleType(config.key.scale);

    std::vector<Pattern> patterns;
    patterns.reserve(config.patterns.size());
    for (const PatternConfig& patternConfig : config.patterns) {
        patterns.push_back(ResolvePattern(patternConfig, root, scale));
    }

    AudioEngine audioEngine;
    AudioEngineConfig audioConfig;
    audioConfig.sampleRate = config.sampleRate;
    audioConfig.channels = 2;
    audioConfig.useNullBackend = useNullBackend;
    if (!audioEngine.Initialize(audioConfig)) {
        std::cerr << "Failed to initialize audio device.\n";
        return 1;
    }

    Mixer& mixer = audioEngine.GetMixer();
    for (const InstrumentConfig& instrument : config.instruments) {
        if (instrument.type == InstrumentType::Synth) {
            SynthInstrumentDef def;
            def.waveform = instrument.waveform;
            def.envelope = instrument.envelope;
            def.gain = instrument.gain;
            mixer.AddSynthInstrument(instrument.id, def);
        } else {
            try {
                SampleInstrumentDef def;
                def.asset = LoadSampleAsset(instrument.file, config.sampleRate);
                def.gain = instrument.gain;
                mixer.AddSampleInstrument(instrument.id, def);
            } catch (const std::exception& e) {
                std::cerr << "Failed to load sample instrument '" << instrument.id << "': " << e.what() << "\n";
                return 1;
            }
        }
    }

    RandomSource randomSource(std::random_device{}());
    VariationEngine variationEngine(config.variationRules);
    Sequencer sequencer(audioEngine, variationEngine, randomSource, config, std::move(patterns));
    sequencer.SetVariationLogCallback([](const std::string& message) { std::cout << message << "\n"; });

    audioEngine.Start();
    std::cout << "Playing composition_demo.json. Press Ctrl+C to stop.\n";

    while (true) {
        sequencer.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
