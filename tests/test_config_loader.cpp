#include <stdexcept>
#include <vector>

#include "doctest/doctest.h"
#include "engine/ConfigLoader.h"

using namespace pmg;

namespace {

const char* kValidJson = R"JSON(
{
  "sampleRate": 44100,
  "tempo": { "bpm": 100, "beatsPerBar": 4 },
  "startPattern": "p1",
  "instruments": [
    { "id": "syn", "type": "synth", "waveform": "square", "gain": 0.5,
      "envelope": { "attack": 0.02, "decay": 0.1, "sustain": 0.4, "release": 0.3 } },
    { "id": "snare", "type": "sample", "file": "samples/snare.wav", "gain": 0.8 }
  ],
  "patterns": [
    { "id": "p1", "lengthBars": 1, "steps": [
      { "beat": 0.0, "instrument": "snare", "velocity": 1.0 },
      { "beat": 1.0, "instrument": "syn", "note": "D", "octave": 5, "velocity": 0.6, "gate": 0.25 }
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
    CHECK(config.startPattern == "p1");

    REQUIRE(config.instruments.size() == 2);
    CHECK(config.instruments[0].id == "syn");
    CHECK(config.instruments[0].type == InstrumentType::Synth);
    CHECK(config.instruments[0].waveform == Waveform::Square);
    CHECK(config.instruments[0].dutyCycle == doctest::Approx(0.5)); // not set in kValidJson -> default
    CHECK(config.instruments[0].arpeggio.semitoneOffsets.empty()); // not set -> disabled
    CHECK(config.instruments[0].vibrato.depthCents == doctest::Approx(0.0f)); // not set -> disabled
    CHECK(config.instruments[0].fm.amount == doctest::Approx(0.0f));          // not set -> disabled
    CHECK(config.instruments[1].type == InstrumentType::Sample);
    CHECK(config.instruments[1].file == "samples/snare.wav");

    REQUIRE(config.patterns.size() == 1);
    REQUIRE(config.patterns[0].steps.size() == 2);
    CHECK_FALSE(config.patterns[0].steps[0].hasNote);
    CHECK(config.patterns[0].steps[1].hasNote);
    CHECK(config.patterns[0].steps[1].note == NoteName::D);
    CHECK(config.patterns[0].steps[1].octave == 5);

    REQUIRE(config.variationRules.size() == 1);
    REQUIRE(config.variationRules[0].options.size() == 2);
    CHECK(config.variationStrategy == VariationStrategyKind::RuleBased);
    CHECK(config.markovChain.empty());
    CHECK(config.loFi.bitDepth == 16); // not set in kValidJson -> default (no quantization)
    CHECK(config.loFi.holdFactor == 1);
    CHECK(config.delay.mix == doctest::Approx(0.0f)); // not set in kValidJson -> disabled
}

TEST_CASE("ConfigLoader defaults a note step's octave to 4 when omitted") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "steps": [
        { "beat": 0.0, "instrument": "syn", "note": "A" }
      ] } ]
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    REQUIRE(config.patterns[0].steps.size() == 1);
    CHECK(config.patterns[0].steps[0].hasNote);
    CHECK(config.patterns[0].steps[0].note == NoteName::A);
    CHECK(config.patterns[0].steps[0].octave == 4);
}

TEST_CASE("ConfigLoader parses a noise-waveform synth instrument") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [
        { "id": "hihat", "type": "synth", "waveform": "noise",
          "envelope": { "attack": 0.001, "decay": 0.04, "sustain": 0.0, "release": 0.02 } }
      ],
      "patterns": [ { "id": "p1", "steps": [] } ]
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    REQUIRE(config.instruments.size() == 1);
    CHECK(config.instruments[0].waveform == Waveform::Noise);
}

TEST_CASE("ConfigLoader throws on missing required field") {
    const char* missingTempo = R"JSON(
    {
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

TEST_CASE("ConfigLoader parses an explicit dutyCycle for a synth instrument") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [
        { "id": "pulse", "type": "synth", "waveform": "square", "dutyCycle": 0.25,
          "envelope": { "attack": 0.01, "decay": 0.1, "sustain": 0.7, "release": 0.2 } }
      ],
      "patterns": [ { "id": "p1", "steps": [] } ]
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    REQUIRE(config.instruments.size() == 1);
    CHECK(config.instruments[0].dutyCycle == doctest::Approx(0.25));
}

TEST_CASE("ConfigLoader parses an explicit arpeggio section for a synth instrument") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [
        { "id": "arp_pad", "type": "synth", "waveform": "square",
          "arpeggio": { "semitones": [0, 4, 7], "rateHz": 16 },
          "envelope": { "attack": 0.01, "decay": 0.1, "sustain": 0.7, "release": 0.2 } }
      ],
      "patterns": [ { "id": "p1", "steps": [] } ]
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    REQUIRE(config.instruments.size() == 1);
    const std::vector<int>& offsets = config.instruments[0].arpeggio.semitoneOffsets;
    REQUIRE(offsets.size() == 3);
    CHECK(offsets[0] == 0);
    CHECK(offsets[1] == 4);
    CHECK(offsets[2] == 7);
    CHECK(config.instruments[0].arpeggio.rateHz == doctest::Approx(16.0f));
}

TEST_CASE("ConfigLoader parses an explicit loFi section") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "steps": [] } ],
      "loFi": { "bitDepth": 4, "holdFactor": 4 }
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    CHECK(config.loFi.bitDepth == 4);
    CHECK(config.loFi.holdFactor == 4);
}

TEST_CASE("ConfigLoader parses an explicit vibrato section for a synth instrument") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [
        { "id": "lead", "type": "synth", "waveform": "sine",
          "vibrato": { "rateHz": 6.5, "depthCents": 30 },
          "envelope": { "attack": 0.01, "decay": 0.1, "sustain": 0.7, "release": 0.2 } }
      ],
      "patterns": [ { "id": "p1", "steps": [] } ]
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    REQUIRE(config.instruments.size() == 1);
    CHECK(config.instruments[0].vibrato.rateHz == doctest::Approx(6.5f));
    CHECK(config.instruments[0].vibrato.depthCents == doctest::Approx(30.0f));
}

TEST_CASE("ConfigLoader parses an explicit fm section for a synth instrument") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [
        { "id": "bell", "type": "synth", "waveform": "sine",
          "fm": { "ratio": 3.5, "amount": 0.6 },
          "envelope": { "attack": 0.01, "decay": 0.1, "sustain": 0.7, "release": 0.2 } }
      ],
      "patterns": [ { "id": "p1", "steps": [] } ]
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    REQUIRE(config.instruments.size() == 1);
    CHECK(config.instruments[0].fm.ratio == doctest::Approx(3.5f));
    CHECK(config.instruments[0].fm.amount == doctest::Approx(0.6f));
}

TEST_CASE("ConfigLoader parses an explicit delay section") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "steps": [] } ],
      "delay": { "delayTimeSeconds": 0.18, "feedback": 0.35, "mix": 0.25 }
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    CHECK(config.delay.delayTimeSeconds == doctest::Approx(0.18f));
    CHECK(config.delay.feedback == doctest::Approx(0.35f));
    CHECK(config.delay.mix == doctest::Approx(0.25f));
}

TEST_CASE("ConfigLoader throws when variationStrategy is markovChain but markovChain is missing") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "a",
      "variationStrategy": "markovChain",
      "instruments": [],
      "patterns": [ { "id": "a", "steps": [] } ]
    }
    )JSON";
    CHECK_THROWS_AS(ConfigLoader::LoadFromString(json), std::runtime_error);
}

TEST_CASE("ConfigLoader expands a melodies block into steps, with 'steps' fully optional") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "melodies": [
        { "instrument": "lead", "notes": "A B G F#" }
      ] } ]
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    REQUIRE(config.patterns.size() == 1);
    const std::vector<StepConfig>& steps = config.patterns[0].steps;
    REQUIRE(steps.size() == 4);
    CHECK(steps[0].note == NoteName::A);
    CHECK(steps[0].beat == doctest::Approx(0.0));
    CHECK(steps[1].note == NoteName::B);
    CHECK(steps[1].beat == doctest::Approx(1.0));
    CHECK(steps[2].note == NoteName::G);
    CHECK(steps[2].beat == doctest::Approx(2.0));
    CHECK(steps[3].note == NoteName::Fs);
    CHECK(steps[3].beat == doctest::Approx(3.0));
    for (const StepConfig& step : steps) {
        CHECK(step.instrument == "lead");
        CHECK(step.hasNote);
    }
}

TEST_CASE("ConfigLoader merges manual steps and melodies on the same pattern") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1",
        "steps": [ { "beat": 0.0, "instrument": "kick", "velocity": 1.0 } ],
        "melodies": [ { "instrument": "lead", "notes": "C D" } ]
      } ]
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    REQUIRE(config.patterns.size() == 1);
    CHECK(config.patterns[0].steps.size() == 3); // 1 manual + 2 melody-expanded
}

TEST_CASE("ConfigLoader applies melody field defaults and overrides") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "melodies": [
        { "instrument": "lead", "notes": "A5 B", "startBeat": 2.0, "noteLengthBeats": 0.5, "gateFraction": 0.5 }
      ] } ]
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    const std::vector<StepConfig>& steps = config.patterns[0].steps;
    REQUIRE(steps.size() == 2);
    CHECK(steps[0].octave == 5);   // explicit per-note override
    CHECK(steps[1].octave == 4);   // falls back to defaultOctave
    CHECK(steps[0].beat == doctest::Approx(2.0));
    CHECK(steps[1].beat == doctest::Approx(2.5));
    CHECK(steps[0].gate == doctest::Approx(0.25)); // 0.5 beats * 0.5 gateFraction
}

TEST_CASE("ConfigLoader throws when a melody is missing 'notes' or 'instrument'") {
    const char* missingNotes = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "melodies": [ { "instrument": "lead" } ] } ]
    }
    )JSON";
    CHECK_THROWS_AS(ConfigLoader::LoadFromString(missingNotes), std::runtime_error);

    const char* missingInstrument = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "melodies": [ { "notes": "A B" } ] } ]
    }
    )JSON";
    CHECK_THROWS_AS(ConfigLoader::LoadFromString(missingInstrument), std::runtime_error);
}

TEST_CASE("ConfigLoader propagates a malformed note-string as a runtime_error naming the pattern") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "melodies": [ { "instrument": "lead", "notes": "A H G" } ] } ]
    }
    )JSON";
    CHECK_THROWS_AS(ConfigLoader::LoadFromString(json), std::runtime_error);
}

TEST_CASE("ConfigLoader parses addLayer and removeLayer variation options") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "steps": [] }, { "id": "p2", "steps": [] } ],
      "variationRules": [
        { "id": "layering", "scope": "perBar", "options": [
          { "type": "addLayer", "pattern": "p2", "weight": 0.4 },
          { "type": "removeLayer", "pattern": "p2", "weight": 0.6 }
        ] }
      ]
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    REQUIRE(config.variationRules.size() == 1);
    REQUIRE(config.variationRules[0].options.size() == 2);
    CHECK(config.variationRules[0].options[0].type == VariationOptionType::AddLayer);
    CHECK(config.variationRules[0].options[0].targetId == "p2");
    CHECK(config.variationRules[0].options[1].type == VariationOptionType::RemoveLayer);
    CHECK(config.variationRules[0].options[1].targetId == "p2");
}

TEST_CASE("ConfigLoader defaults pan to 0.0 and parses an explicit pan for synth and sample instruments") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [
        { "id": "syn", "type": "synth", "waveform": "square",
          "envelope": { "attack": 0.01, "decay": 0.1, "sustain": 0.7, "release": 0.2 } },
        { "id": "lead", "type": "synth", "waveform": "sine", "pan": -0.6,
          "envelope": { "attack": 0.01, "decay": 0.1, "sustain": 0.7, "release": 0.2 } },
        { "id": "snare", "type": "sample", "file": "samples/snare.wav", "pan": 0.8 }
      ],
      "patterns": [ { "id": "p1", "steps": [] } ]
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    REQUIRE(config.instruments.size() == 3);
    CHECK(config.instruments[0].pan == doctest::Approx(0.0f)); // not set -> center
    CHECK(config.instruments[1].pan == doctest::Approx(-0.6f));
    CHECK(config.instruments[2].pan == doctest::Approx(0.8f));
}

TEST_CASE("ConfigLoader expands a generatedChords block into same-beat stacked-triad steps") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "generatedChords": [
        { "instrument": "pad", "key": "C", "scale": "major", "degrees": [0, 3, 4, 0], "chordLengthBeats": 4.0 }
      ] } ]
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    REQUIRE(config.patterns.size() == 1);
    const std::vector<StepConfig>& steps = config.patterns[0].steps;
    REQUIRE(steps.size() == 12); // 4 degrees * 3 tones (triad, seventh defaults to false)
    for (const StepConfig& step : steps) {
        CHECK(step.instrument == "pad");
        CHECK(step.hasNote);
    }
}

TEST_CASE("ConfigLoader expands a generatedBasslines block, reproducible via seed") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "generatedBasslines": [
        { "instrument": "bass", "key": "C", "scale": "major", "degrees": [0, 3, 4, 0],
          "chordLengthBeats": 4.0, "noteLengthBeats": 1.0, "seed": 42 }
      ] } ]
    }
    )JSON";

    CompositionConfig configA = ConfigLoader::LoadFromString(json);
    CompositionConfig configB = ConfigLoader::LoadFromString(json);

    REQUIRE(configA.patterns[0].steps.size() == 16); // 4 degrees * 4 slots/span
    REQUIRE(configA.patterns[0].steps.size() == configB.patterns[0].steps.size());
    for (size_t i = 0; i < configA.patterns[0].steps.size(); ++i) {
        CHECK(configA.patterns[0].steps[i].note == configB.patterns[0].steps[i].note);
        CHECK(configA.patterns[0].steps[i].instrument == "bass");
    }
}

TEST_CASE("ConfigLoader throws when a generatedChord is missing 'degrees' or 'instrument'") {
    const char* missingDegrees = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "generatedChords": [ { "instrument": "pad" } ] } ]
    }
    )JSON";
    CHECK_THROWS_AS(ConfigLoader::LoadFromString(missingDegrees), std::runtime_error);

    const char* missingInstrument = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "generatedChords": [ { "degrees": [0, 4] } ] } ]
    }
    )JSON";
    CHECK_THROWS_AS(ConfigLoader::LoadFromString(missingInstrument), std::runtime_error);
}

TEST_CASE("ConfigLoader parses gameParameters with and without an explicit default") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "steps": [] } ],
      "gameParameters": [
        { "name": "danger", "default": 0.25 },
        { "name": "intensity" }
      ]
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    REQUIRE(config.gameParameters.size() == 2);
    CHECK(config.gameParameters[0].name == "danger");
    CHECK(config.gameParameters[0].defaultValue == doctest::Approx(0.25f));
    CHECK(config.gameParameters[1].name == "intensity");
    CHECK(config.gameParameters[1].defaultValue == doctest::Approx(0.0f)); // not set -> default
}

TEST_CASE("ConfigLoader parses gainCrossfades with defaults") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "steps": [] } ],
      "gainCrossfades": [
        { "instrument": "pad", "parameter": "danger", "gainAtMax": 0.8, "smoothingSeconds": 1.5 }
      ]
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    REQUIRE(config.gainCrossfades.size() == 1);
    CHECK(config.gainCrossfades[0].instrument == "pad");
    CHECK(config.gainCrossfades[0].parameter == "danger");
    CHECK(config.gainCrossfades[0].paramAtGainMin == doctest::Approx(0.0f)); // not set -> default
    CHECK(config.gainCrossfades[0].gainAtMin == doctest::Approx(0.0f));
    CHECK(config.gainCrossfades[0].paramAtGainMax == doctest::Approx(1.0f));
    CHECK(config.gainCrossfades[0].gainAtMax == doctest::Approx(0.8f));
    CHECK(config.gainCrossfades[0].smoothingSeconds == doctest::Approx(1.5f));
}

TEST_CASE("ConfigLoader throws when a gainCrossfades entry is missing 'instrument' or 'parameter'") {
    const char* missingParameter = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "steps": [] } ],
      "gainCrossfades": [ { "instrument": "pad" } ]
    }
    )JSON";
    CHECK_THROWS_AS(ConfigLoader::LoadFromString(missingParameter), std::runtime_error);
}

TEST_CASE("ConfigLoader parses a variation rule's gateParameter/gateMin/gateMax, defaulting to an unbounded gate") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "steps": [] } ],
      "variationRules": [
        { "id": "gated", "scope": "perBar", "gateParameter": "danger", "gateMin": 0.5, "gateMax": 1.0,
          "options": [ { "type": "noOp", "weight": 1.0 } ] },
        { "id": "ungated", "scope": "perBar", "options": [ { "type": "noOp", "weight": 1.0 } ] }
      ]
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    REQUIRE(config.variationRules.size() == 2);
    CHECK(config.variationRules[0].gateParameter == "danger");
    CHECK(config.variationRules[0].gateMin == doctest::Approx(0.5f));
    CHECK(config.variationRules[0].gateMax == doctest::Approx(1.0f));
    CHECK(config.variationRules[1].gateParameter.empty());
    CHECK(config.variationRules[1].gateMin < -1e30f); // -infinity -> unbounded below
    CHECK(config.variationRules[1].gateMax > 1e30f);  // +infinity -> unbounded above
}

TEST_CASE("ConfigLoader parses a variation option's weight-scaling fields, defaulting to unscaled") {
    const char* json = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "steps": [] } ],
      "variationRules": [
        { "id": "r", "scope": "perBar",
          "options": [
            { "type": "noOp", "weight": 1.0,
              "weightParameter": "danger",
              "paramAtWeightMin": 0.0, "weightMultiplierAtMin": 0.1,
              "paramAtWeightMax": 1.0, "weightMultiplierAtMax": 9.0 },
            { "type": "noOp", "weight": 1.0 }
          ] }
      ]
    }
    )JSON";

    CompositionConfig config = ConfigLoader::LoadFromString(json);
    REQUIRE(config.variationRules.size() == 1);
    REQUIRE(config.variationRules[0].options.size() == 2);

    const VariationOptionConfig& scaled = config.variationRules[0].options[0];
    CHECK(scaled.weightParameter == "danger");
    CHECK(scaled.paramAtWeightMin == doctest::Approx(0.0f));
    CHECK(scaled.weightMultiplierAtMin == doctest::Approx(0.1f));
    CHECK(scaled.paramAtWeightMax == doctest::Approx(1.0f));
    CHECK(scaled.weightMultiplierAtMax == doctest::Approx(9.0f));

    const VariationOptionConfig& unscaled = config.variationRules[0].options[1];
    CHECK(unscaled.weightParameter.empty());
    CHECK(unscaled.paramAtWeightMin == doctest::Approx(0.0f));  // defaults, unused since weightParameter is empty
    CHECK(unscaled.weightMultiplierAtMin == doctest::Approx(1.0f));
    CHECK(unscaled.paramAtWeightMax == doctest::Approx(1.0f));
    CHECK(unscaled.weightMultiplierAtMax == doctest::Approx(1.0f));
}

TEST_CASE("ConfigLoader throws when an addLayer/removeLayer option is missing 'pattern'") {
    const char* missingPatternOnAdd = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "steps": [] } ],
      "variationRules": [
        { "id": "r", "scope": "perBar", "options": [ { "type": "addLayer", "weight": 1.0 } ] }
      ]
    }
    )JSON";
    CHECK_THROWS_AS(ConfigLoader::LoadFromString(missingPatternOnAdd), std::runtime_error);

    const char* missingPatternOnRemove = R"JSON(
    {
      "tempo": { "bpm": 100, "beatsPerBar": 4 },
      "startPattern": "p1",
      "instruments": [],
      "patterns": [ { "id": "p1", "steps": [] } ],
      "variationRules": [
        { "id": "r", "scope": "perBar", "options": [ { "type": "removeLayer", "weight": 1.0 } ] }
      ]
    }
    )JSON";
    CHECK_THROWS_AS(ConfigLoader::LoadFromString(missingPatternOnRemove), std::runtime_error);
}
