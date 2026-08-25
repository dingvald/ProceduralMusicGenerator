#include "engine/ConfigLoader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

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
    throw std::runtime_error("ConfigLoader: unknown waveform '" + name + "'");
}

VariationOptionType ParseVariationOptionType(const std::string& name) {
    if (name == "noOp") return VariationOptionType::NoOp;
    if (name == "swapPattern") return VariationOptionType::SwapPattern;
    if (name == "setTrackMuted") return VariationOptionType::SetTrackMuted;
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

    auto degreeIt = j.find("degree");
    if (degreeIt != j.end()) {
        step.hasDegree = true;
        step.degree = degreeIt->get<int>();
    }
    return step;
}

PatternConfig ParsePattern(const json& j) {
    PatternConfig pattern;
    pattern.id = RequireField(j, "id", "pattern").get<std::string>();
    pattern.lengthBars = j.value("lengthBars", 1);

    json steps = RequireField(j, "steps", "pattern '" + pattern.id + "'");
    for (const auto& stepJson : steps) {
        pattern.steps.push_back(ParseStep(stepJson, pattern.id));
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

    json key = RequireField(root, "key", "composition");
    config.key.root = RequireField(key, "root", "key").get<std::string>();
    config.key.scale = RequireField(key, "scale", "key").get<std::string>();

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
