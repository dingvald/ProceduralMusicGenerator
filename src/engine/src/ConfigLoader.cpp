#include "engine/ConfigLoader.h"

#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>

#include "engine/MelodyGenerator.h"
#include "engine/NoteStringParser.h"
#include "engine/RandomSource.h"
#include "engine/RhythmGenerator.h"
#include "engine/Theory.h"
#include "nlohmann/json.hpp"

namespace pmg {

namespace {

using nlohmann::json;

const json& RequireField(const json& obj, const std::string& key, const std::string& context) {
    auto it = obj.find(key);
    if (it == obj.end()) {
        throw std::runtime_error("ConfigLoader: missing required field '" + key + "' in " + context);
    }
    return *it;
}

Waveform ParseWaveform(const std::string& name) {
    if (name == "sine") return Waveform::Sine;
    if (name == "saw") return Waveform::Saw;
    if (name == "square") return Waveform::Square;
    if (name == "triangle") return Waveform::Triangle;
    if (name == "noise") return Waveform::Noise;
    throw std::runtime_error("ConfigLoader: unknown waveform '" + name + "'");
}

VariationOptionType ParseVariationOptionType(const std::string& name) {
    if (name == "noOp") return VariationOptionType::NoOp;
    if (name == "swapPattern") return VariationOptionType::SwapPattern;
    if (name == "setTrackMuted") return VariationOptionType::SetTrackMuted;
    if (name == "addLayer") return VariationOptionType::AddLayer;
    if (name == "removeLayer") return VariationOptionType::RemoveLayer;
    throw std::runtime_error("ConfigLoader: unknown variation option type '" + name + "'");
}

VariationStrategyKind ParseVariationStrategyKind(const std::string& name) {
    if (name == "ruleBased") return VariationStrategyKind::RuleBased;
    if (name == "markovChain") return VariationStrategyKind::MarkovChain;
    throw std::runtime_error("ConfigLoader: unknown variationStrategy '" + name + "'");
}

InstrumentConfig ParseInstrument(const json& j) {
    InstrumentConfig inst;
    inst.id = RequireField(j, "id", "instrument").get<std::string>();
    std::string typeStr = RequireField(j, "type", "instrument '" + inst.id + "'").get<std::string>();
    inst.gain = j.value("gain", 1.0f);

    if (typeStr == "synth") {
        inst.type = InstrumentType::Synth;
        inst.waveform = ParseWaveform(
            RequireField(j, "waveform", "synth instrument '" + inst.id + "'").get<std::string>());
        inst.dutyCycle = j.value("dutyCycle", 0.5f);

        auto arpIt = j.find("arpeggio");
        if (arpIt != j.end()) {
            json semitones = RequireField(*arpIt, "semitones", "arpeggio for instrument '" + inst.id + "'");
            for (const auto& semitoneJson : semitones) {
                inst.arpeggio.semitoneOffsets.push_back(semitoneJson.get<int>());
            }
            inst.arpeggio.rateHz = arpIt->value("rateHz", 20.0f);
        }

        auto vibIt = j.find("vibrato");
        if (vibIt != j.end()) {
            inst.vibrato.rateHz = vibIt->value("rateHz", 5.0f);
            inst.vibrato.depthCents = vibIt->value("depthCents", 0.0f);
        }

        auto fmIt = j.find("fm");
        if (fmIt != j.end()) {
            inst.fm.ratio = fmIt->value("ratio", 1.0f);
            inst.fm.amount = fmIt->value("amount", 0.0f);
        }

        json env = RequireField(j, "envelope", "synth instrument '" + inst.id + "'");
        inst.envelope.attackSec = env.value("attack", 0.01f);
        inst.envelope.decaySec = env.value("decay", 0.1f);
        inst.envelope.sustainLevel = env.value("sustain", 0.7f);
        inst.envelope.releaseSec = env.value("release", 0.2f);
    } else if (typeStr == "sample") {
        inst.type = InstrumentType::Sample;
        inst.file = RequireField(j, "file", "sample instrument '" + inst.id + "'").get<std::string>();
    } else {
        throw std::runtime_error(
            "ConfigLoader: unknown instrument type '" + typeStr + "' for instrument '" + inst.id + "'");
    }

    return inst;
}

StepConfig ParseStep(const json& j, const std::string& patternId) {
    StepConfig step;
    step.beat = RequireField(j, "beat", "step in pattern '" + patternId + "'").get<double>();
    step.instrument = RequireField(j, "instrument", "step in pattern '" + patternId + "'").get<std::string>();
    step.velocity = j.value("velocity", 1.0f);
    step.gate = j.value("gate", 0.25f);

    auto noteIt = j.find("note");
    if (noteIt != j.end()) {
        step.hasNote = true;
        step.note = Theory::ParseNoteName(noteIt->get<std::string>());
        step.octave = j.value("octave", 4);
    }
    return step;
}

MelodyConfig ParseMelody(const json& j, const std::string& patternId) {
    MelodyConfig melody;
    melody.instrument = RequireField(j, "instrument", "melody in pattern '" + patternId + "'").get<std::string>();
    melody.notes = RequireField(j, "notes", "melody in pattern '" + patternId + "'").get<std::string>();
    melody.startBeat = j.value("startBeat", 0.0);
    melody.defaultOctave = j.value("defaultOctave", 4);
    melody.noteLengthBeats = j.value("noteLengthBeats", 1.0);
    melody.gateFraction = j.value("gateFraction", 0.8f);
    melody.velocity = j.value("velocity", 0.8f);
    return melody;
}

// A generated block's "seed" is consumed immediately (the RandomSource it
// seeds is only ever used right here, to expand the block into StepConfigs),
// so it's read straight from JSON rather than round-tripped through
// GeneratedMelodyConfig/GeneratedRhythmConfig. Omitting it falls back to
// std::random_device, matching main.cpp's existing non-reproducible-by-default
// seeding for the rest of the engine; an explicit seed makes just this one
// block's generated content reproducible run-to-run.
uint64_t ReadSeed(const json& j) {
    return j.value<uint64_t>("seed", static_cast<uint64_t>(std::random_device{}()));
}

GeneratedMelodyConfig ParseGeneratedMelody(const json& j, const std::string& patternId) {
    GeneratedMelodyConfig gen;
    gen.instrument =
        RequireField(j, "instrument", "generatedMelody in pattern '" + patternId + "'").get<std::string>();
    gen.key = Theory::ParseNoteName(j.value("key", std::string("C")));
    gen.scale = Theory::ParseScale(j.value("scale", std::string("majorPentatonic")));
    gen.baseOctave = j.value("baseOctave", 4);
    gen.octaveRange = j.value("octaveRange", 1);
    gen.lengthBeats = j.value("lengthBeats", 8.0);
    gen.noteLengthBeats = j.value("noteLengthBeats", 1.0);
    gen.restProbability = j.value("restProbability", 0.15f);
    gen.gateFraction = j.value("gateFraction", 0.8f);
    gen.velocity = j.value("velocity", 0.8f);
    return gen;
}

GeneratedRhythmConfig ParseGeneratedRhythm(const json& j, const std::string& patternId) {
    GeneratedRhythmConfig gen;
    gen.instrument =
        RequireField(j, "instrument", "generatedRhythm in pattern '" + patternId + "'").get<std::string>();
    gen.steps = j.value("steps", 16);
    gen.pulses = j.value("pulses", 5);
    gen.lengthBeats = j.value("lengthBeats", 4.0);
    gen.velocity = j.value("velocity", 0.6f);
    gen.velocityJitter = j.value("velocityJitter", 0.0f);
    gen.gate = j.value("gate", 0.1f);

    auto noteIt = j.find("note");
    if (noteIt != j.end()) {
        gen.hasNote = true;
        gen.note = Theory::ParseNoteName(noteIt->get<std::string>());
        gen.octave = j.value("octave", 8);
    }
    return gen;
}

PatternConfig ParsePattern(const json& j) {
    PatternConfig pattern;
    pattern.id = RequireField(j, "id", "pattern").get<std::string>();
    pattern.lengthBars = j.value("lengthBars", 1);

    json steps = j.value("steps", json::array());
    for (const auto& stepJson : steps) {
        pattern.steps.push_back(ParseStep(stepJson, pattern.id));
    }

    if (j.contains("melodies")) {
        for (const auto& melodyJson : j["melodies"]) {
            MelodyConfig melody = ParseMelody(melodyJson, pattern.id);
            try {
                std::vector<StepConfig> melodySteps = ParseNoteString(melody);
                pattern.steps.insert(pattern.steps.end(), melodySteps.begin(), melodySteps.end());
            } catch (const std::exception& e) {
                throw std::runtime_error("ConfigLoader: pattern '" + pattern.id + "' melody for instrument '" +
                                          melody.instrument + "': " + e.what());
            }
        }
    }

    if (j.contains("generatedMelodies")) {
        for (const auto& genJson : j["generatedMelodies"]) {
            GeneratedMelodyConfig genConfig = ParseGeneratedMelody(genJson, pattern.id);
            try {
                RandomSource rng(ReadSeed(genJson));
                std::vector<StepConfig> genSteps = GenerateMelody(genConfig, rng);
                pattern.steps.insert(pattern.steps.end(), genSteps.begin(), genSteps.end());
            } catch (const std::exception& e) {
                throw std::runtime_error("ConfigLoader: pattern '" + pattern.id +
                                          "' generatedMelody for instrument '" + genConfig.instrument +
                                          "': " + e.what());
            }
        }
    }

    if (j.contains("generatedRhythms")) {
        for (const auto& genJson : j["generatedRhythms"]) {
            GeneratedRhythmConfig genConfig = ParseGeneratedRhythm(genJson, pattern.id);
            try {
                RandomSource rng(ReadSeed(genJson));
                std::vector<StepConfig> genSteps = GenerateRhythm(genConfig, rng);
                pattern.steps.insert(pattern.steps.end(), genSteps.begin(), genSteps.end());
            } catch (const std::exception& e) {
                throw std::runtime_error("ConfigLoader: pattern '" + pattern.id +
                                          "' generatedRhythm for instrument '" + genConfig.instrument +
                                          "': " + e.what());
            }
        }
    }

    return pattern;
}

VariationOptionConfig ParseVariationOption(const json& j, const std::string& ruleId) {
    VariationOptionConfig option;
    std::string typeStr =
        RequireField(j, "type", "variation option in rule '" + ruleId + "'").get<std::string>();
    option.type = ParseVariationOptionType(typeStr);
    option.weight = j.value("weight", 1.0f);

    if (option.type == VariationOptionType::SwapPattern) {
        option.targetId = RequireField(j, "pattern", "swapPattern option in rule '" + ruleId + "'").get<std::string>();
    } else if (option.type == VariationOptionType::SetTrackMuted) {
        option.targetId = RequireField(j, "track", "setTrackMuted option in rule '" + ruleId + "'").get<std::string>();
        option.boolValue = j.value("muted", false);
    } else if (option.type == VariationOptionType::AddLayer) {
        option.targetId = RequireField(j, "pattern", "addLayer option in rule '" + ruleId + "'").get<std::string>();
    } else if (option.type == VariationOptionType::RemoveLayer) {
        option.targetId = RequireField(j, "pattern", "removeLayer option in rule '" + ruleId + "'").get<std::string>();
    }

    return option;
}

VariationRuleConfig ParseVariationRule(const json& j) {
    VariationRuleConfig rule;
    rule.id = RequireField(j, "id", "variation rule").get<std::string>();
    rule.scope = j.value("scope", std::string("perBar"));

    json options = RequireField(j, "options", "variation rule '" + rule.id + "'");
    for (const auto& optJson : options) {
        rule.options.push_back(ParseVariationOption(optJson, rule.id));
    }
    return rule;
}

MarkovChainConfig ParseMarkovChain(const json& j) {
    MarkovChainConfig chain;
    json transitions = RequireField(j, "patternTransitions", "markovChain");

    for (auto it = transitions.begin(); it != transitions.end(); ++it) {
        const std::string& fromPattern = it.key();
        std::vector<MarkovTransitionConfig> outgoing;
        for (const auto& transitionJson : it.value()) {
            MarkovTransitionConfig transition;
            transition.toPattern =
                RequireField(transitionJson, "pattern", "markovChain transition from '" + fromPattern + "'")
                    .get<std::string>();
            transition.weight = transitionJson.value("weight", 1.0f);
            outgoing.push_back(transition);
        }
        chain[fromPattern] = std::move(outgoing);
    }

    return chain;
}

} // namespace

CompositionConfig ConfigLoader::LoadFromString(const std::string& jsonText) {
    json root = json::parse(jsonText); // throws nlohmann::json::parse_error on malformed JSON

    CompositionConfig config;
    config.sampleRate = root.value("sampleRate", 48000u);

    json tempo = RequireField(root, "tempo", "composition");
    config.tempo.bpm = RequireField(tempo, "bpm", "tempo").get<double>();
    config.tempo.beatsPerBar = tempo.value("beatsPerBar", 4);

    config.startPattern = RequireField(root, "startPattern", "composition").get<std::string>();

    json instruments = RequireField(root, "instruments", "composition");
    for (const auto& instJson : instruments) {
        config.instruments.push_back(ParseInstrument(instJson));
    }

    json patterns = RequireField(root, "patterns", "composition");
    for (const auto& patternJson : patterns) {
        config.patterns.push_back(ParsePattern(patternJson));
    }

    if (root.contains("variationRules")) {
        for (const auto& ruleJson : root["variationRules"]) {
            config.variationRules.push_back(ParseVariationRule(ruleJson));
        }
    }

    if (root.contains("variationStrategy")) {
        config.variationStrategy = ParseVariationStrategyKind(root["variationStrategy"].get<std::string>());
    }

    if (root.contains("markovChain")) {
        config.markovChain = ParseMarkovChain(root["markovChain"]);
    }

    if (config.variationStrategy == VariationStrategyKind::MarkovChain && config.markovChain.empty()) {
        throw std::runtime_error(
            "ConfigLoader: variationStrategy is 'markovChain' but no 'markovChain.patternTransitions' were provided");
    }

    if (root.contains("loFi")) {
        json loFi = root["loFi"];
        config.loFi.bitDepth = loFi.value("bitDepth", 16);
        config.loFi.holdFactor = loFi.value("holdFactor", 1);
    }

    if (root.contains("delay")) {
        json delay = root["delay"];
        config.delay.delayTimeSeconds = delay.value("delayTimeSeconds", 0.0f);
        config.delay.feedback = delay.value("feedback", 0.0f);
        config.delay.mix = delay.value("mix", 0.0f);
    }

    return config;
}

CompositionConfig ConfigLoader::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("ConfigLoader: could not open file '" + path + "'");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return LoadFromString(buffer.str());
}

} // namespace pmg
