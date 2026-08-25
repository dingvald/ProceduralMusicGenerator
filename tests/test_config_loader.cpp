#include <stdexcept>

#include "doctest/doctest.h"
#include "engine/ConfigLoader.h"

using namespace pmg;

namespace {

const char* kValidJson = R"JSON(
{
  "sampleRate": 44100,
  "tempo": { "bpm": 100, "beatsPerBar": 4 },
  "key": { "root": "A", "scale": "natural_minor" },
  "startPattern": "p1",
  "instruments": [
    { "id": "syn", "type": "synth", "waveform": "square", "gain": 0.5,
      "envelope": { "attack": 0.02, "decay": 0.1, "sustain": 0.4, "release": 0.3 } },
    { "id": "snare", "type": "sample", "file": "samples/snare.wav", "gain": 0.8 }
  ],
  "patterns": [
    { "id": "p1", "lengthBars": 1, "steps": [
      { "beat": 0.0, "instrument": "snare", "velocity": 1.0 },
      { "beat": 1.0, "instrument": "syn", "degree": 2, "velocity": 0.6, "gate": 0.25 }
    ] }
  ],
  "variationRules": [
    { "id": "r1", "scope": "perBar", "options": [
      { "type": "swapPattern", "pattern": "p1", "weight": 1.0 },
      { "type": "noOp", "weight": 1.0 }
    ] }
  ]
}
)JSON";

} // namespace

TEST_CASE("ConfigLoader parses a full valid composition") {
    CompositionConfig config = ConfigLoader::LoadFromString(kValidJson);

    CHECK(config.sampleRate == 44100);
    CHECK(config.tempo.bpm == doctest::Approx(100.0));
    CHECK(config.tempo.beatsPerBar == 4);
    CHECK(config.key.root == "A");
    CHECK(config.key.scale == "natural_minor");
    CHECK(config.startPattern == "p1");

    REQUIRE(config.instruments.size() == 2);
    CHECK(config.instruments[0].id == "syn");
    CHECK(config.instruments[0].type == InstrumentType::Synth);
    CHECK(config.instruments[0].waveform == Waveform::Square);
    CHECK(config.instruments[1].type == InstrumentType::Sample);
    CHECK(config.instruments[1].file == "samples/snare.wav");

    REQUIRE(config.patterns.size() == 1);
    REQUIRE(config.patterns[0].steps.size() == 2);
    CHECK_FALSE(config.patterns[0].steps[0].hasDegree);
    CHECK(config.patterns[0].steps[1].hasDegree);
    CHECK(config.patterns[0].steps[1].degree == 2);

    REQUIRE(config.variationRules.size() == 1);
    REQUIRE(config.variationRules[0].options.size() == 2);
    CHECK(config.variationStrategy == VariationStrategyKind::RuleBased);
    CHECK(config.markovChain.empty());
}

TEST_CASE("ConfigLoader throws on missing required field") {
    const char* missingTempo = R"JSON(
    {
      "key": { "root": "C", "scale": "major" },
      "startPattern": "p1",
      "instruments": [],
      "patterns": []
    }
    )JSON";
    CHECK_THROWS_AS(ConfigLoader::LoadFromString(missingTempo), std::runtime_error);
}

TEST_CASE("ConfigLoader throws on malformed JSON") {
    CHECK_THROWS(ConfigLoader::LoadFromString("{ not valid json"));
}

TEST_CASE("ConfigLoader parses markovChain and variationStrategy") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "key": { "root": "C", "scale": "major" },
      "startPattern": "a",
      "variationStrategy": "markovChain",
      "instruments": [],
      "patterns": [
        { "id": "a", "steps": [] },
        { "id": "b", "steps": [] }
      ],
      "markovChain": {
        "patternTransitions": {
          "a": [
            { "pattern": "a", "weight": 0.3 },
            { "pattern": "b", "weight": 0.7 }
          ]
        }
      }
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    CHECK(config.variationStrategy == VariationStrategyKind::MarkovChain);

    REQUIRE(config.markovChain.count("a") == 1);
    REQUIRE(config.markovChain.at("a").size() == 2);
    CHECK(config.markovChain.at("a")[0].toPattern == "a");
    CHECK(config.markovChain.at("a")[0].weight == doctest::Approx(0.3));
    CHECK(config.markovChain.at("a")[1].toPattern == "b");
    CHECK(config.markovChain.at("a")[1].weight == doctest::Approx(0.7));
}

TEST_CASE("ConfigLoader throws when variationStrategy is markovChain but markovChain is missing") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "key": { "root": "C", "scale": "major" },
      "startPattern": "a",
      "variationStrategy": "markovChain",
      "instruments": [],
      "patterns": [ { "id": "a", "steps": [] } ]
    }
    )JSON";
    CHECK_THROWS_AS(ConfigLoader::LoadFromString(json), std::runtime_error);
}
