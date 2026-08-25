#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "engine/AudioEngine.h"
#include "engine/ConfigLoader.h"
#include "engine/MarkovChainVariationStrategy.h"
#include "engine/Mixer.h"
#include "engine/Pattern.h"
#include "engine/RandomSource.h"
#include "engine/SampleLoader.h"
#include "engine/Sequencer.h"
#include "engine/VariationEngine.h"

int main(int argc, char** argv) {
    using namespace pmg;

    bool useNullBackend = false;
    std::string configPath = "composition_demo.json";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--null-audio") {
            useNullBackend = true;
        } else {
            configPath = arg;
        }
    }

    std::cout.setf(std::ios::unitbuf); // flush eagerly so redirected/piped output stays live during the playback loop

    std::cout << "Procedural Music Generator - demo\n";

    CompositionConfig config;
    try {
        config = ConfigLoader::LoadFromFile(configPath);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load " << configPath << ": " << e.what() << "\n";
        return 1;
    }

    std::vector<Pattern> patterns;
    patterns.reserve(config.patterns.size());
    for (const PatternConfig& patternConfig : config.patterns) {
        patterns.push_back(ResolvePattern(patternConfig));
    }

    AudioEngine audioEngine;
    AudioEngineConfig audioConfig;
    audioConfig.sampleRate = config.sampleRate;
    audioConfig.channels = 2;
    audioConfig.useNullBackend = useNullBackend;
    audioConfig.loFi = config.loFi;
    if (!audioEngine.Initialize(audioConfig)) {
        std::cerr << "Failed to initialize audio device.\n";
        return 1;
    }

    Mixer& mixer = audioEngine.GetMixer();
    for (const InstrumentConfig& instrument : config.instruments) {
        if (instrument.type == InstrumentType::Synth) {
            SynthInstrumentDef def;
            def.waveform = instrument.waveform;
            def.dutyCycle = instrument.dutyCycle;
            def.arpeggio = instrument.arpeggio;
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

    std::unique_ptr<IVariationStrategy> variationStrategy;
    if (config.variationStrategy == VariationStrategyKind::MarkovChain) {
        variationStrategy = std::make_unique<MarkovChainVariationStrategy>(config.markovChain);
    }

    RandomSource randomSource(std::random_device{}());
    VariationEngine variationEngine(config.variationRules, std::move(variationStrategy));
    Sequencer sequencer(audioEngine, variationEngine, randomSource, config, std::move(patterns));
    sequencer.SetVariationLogCallback([](const std::string& message) { std::cout << message << "\n"; });

    audioEngine.Start();
    std::cout << "Playing " << configPath << ". Press Ctrl+C to stop.\n";

    while (true) {
        sequencer.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
