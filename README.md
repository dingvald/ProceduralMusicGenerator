# Procedural Music Generator

A procedural/generative music engine written in modern C++. It supports:

- **Procedural synth playback** — oscillator (sine/saw/square/triangle) + ADSR envelope voices.
- **Sample playback** — WAV samples decoded and triggered alongside synth voices.
- **Real-time dynamic variation** — a pluggable `IVariationStrategy` behind `VariationEngine`
  decides pattern swaps and track mutes live. Two implementations ship: a rule-based,
  weighted-random strategy, and a **Markov-chain strategy** that samples the next pattern from a
  weighted transition table keyed by the pattern currently playing.
- **JSON-driven composition** — tempo, key/scale, instruments, patterns, and variation rules (or a
  Markov transition table) are all defined in a JSON file loaded at startup (see
  `assets/composition_demo.json` and `assets/composition_demo_markov.json`).

Built with [premake5](https://premake.github.io/); targets Windows via Visual Studio 2026 for
this pass (see **Build** below for the current premake action caveat).

## Design goal: lo-fi chiptune for retro games

This engine's target output is **chiptune-style background music for retro-styled games** — the
sound of 8/16-bit sound chips (NES 2A03, Game Boy, SID), not a general-purpose synth. That
constrains which features earn a place here:

- **Implementation can be modern; the sound must stay period-authentic.** PolyBLEP band-limiting
  (already in `Oscillator`) is in scope because it shapes *how* a waveform is generated, not
  because it should make the output sound like a clean modern synth — real chips didn't alias
  the way a naive oscillator does either; they produced clean, simple waveforms at a coarse,
  quantized resolution. Features that would push the sound toward a generic modern synth
  (e.g. smooth continuous-parameter modulation, orchestral-style layering) are out of scope
  unless they're in service of an authentically chip-like effect.
- Favor primitives real sound chips actually had: pulse waves with a **selectable duty cycle**
  (not just a fixed 50% square), a **noise channel** (LFSR-style) for percussion instead of only
  sample playback, low **polyphony per channel** (chip channels rarely stacked notes — arpeggios
  faked chords by cycling one channel's pitch quickly), and coarse **bit-depth/sample-rate**
  quantization on the output stage.
- The variation system (rule-based / Markov-chain pattern swapping, track muting) is squarely in
  scope as-is — procedural arrangement variation is exactly how chiptune loops avoided feeling
  static despite tight hardware constraints.

New feature proposals should be checked against this: does it make the engine better at
authentic-feeling chiptune, or does it just make it a better generic synth?

## Layout

```
premake5.lua              premake workspace: Engine (StaticLib), DemoApp, Tests
third_party/               vendored single-header libs (miniaudio, nlohmann/json, doctest)
src/engine/include/engine  Engine public headers
src/engine/src             Engine implementation
src/app/main.cpp           Demo console app
tests/                     doctest unit tests
assets/                    composition_demo*.json + samples/kick.wav (used by DemoApp)
```

See `third_party/THIRD_PARTY_NOTICES.md` for exact vendored versions/licenses.

## Build

### Windows / Visual Studio 2026 (primary target)

```
premake5 vs2026
```

**Caveat, confirmed by running `premake5 --help` against the current premake5 release**: this
premake5 build's Visual Studio support tops out at the `vs2022` action — there is no `vs2026`
action yet. Until upstream premake adds one:

```
premake5 vs2022
```

then open `build/ProceduralMusicGenerator.sln` in Visual Studio 2026 — VS2026 retargets/upgrades
an older-format solution on first open, so this is normally sufficient with no script changes.
Re-check `premake5 --help` periodically in case a newer premake5 release adds a `vs2026` action.

The generated solution builds three projects: `Engine` (static lib), `DemoApp` (console app,
depends on `Engine`), and `Tests` (console app, depends on `Engine`). `DemoApp`'s debug working
directory is set to `assets/`, so `composition_demo.json` and `samples/kick.wav` resolve
correctly when launched from the Visual Studio debugger.

### What's verified so far

This project was scaffolded and verified from a Linux container with no Windows toolchain or
Visual Studio available, so full VS2026/WASAPI verification is still pending on a real Windows
machine. What **was** verified in that environment:

- `premake5.lua` parses cleanly: both `premake5 gmake2` and `premake5 vs2022` generate correct
  project files (Makefile / .sln+.vcxproj) with no errors.
- All engine + demo app + test sources compile cleanly with both g++ 13 and clang++ 18 at
  `-std=c++20 -Wall -Wextra` (no warnings) against the exact same file set premake5 builds.
- The `Tests` binary runs and all test cases pass (doctest).
- `DemoApp` runs end-to-end against miniaudio's null backend (`--null-audio` flag — see below):
  it loads `composition_demo.json`, decodes `samples/kick.wav`, opens/starts/stops the audio
  device, and the Sequencer/VariationEngine produce live pattern-swap and track-mute decisions,
  logged to the console, bar by bar.
- Against a real ALSA device (no sound card present in this container) `AudioEngine::Initialize`
  correctly reports failure rather than crashing — the code path was exercised even though no
  audio hardware was available to fully confirm it.

**Not verifiable in this environment** and left for a follow-up pass on Windows: MSVC-specific
warnings, real WASAPI device enumeration/opening, actual VS2026 project-file
compatibility/retargeting, and audible confirmation of synth + sample + variation playback.

### Headless smoke test (any platform)

`DemoApp` accepts a `--null-audio` flag that forces miniaudio's null backend, so the full
pipeline (config load, sample decode, device open/start/stop, sequencing, variation) can be
exercised without real audio hardware. It also accepts an optional composition JSON path
(defaults to `composition_demo.json`):

```
DemoApp --null-audio
DemoApp composition_demo_markov.json --null-audio
```

### Running the tests

Build the `Tests` project and run the resulting executable from the `assets/` directory (or any
directory containing `composition_demo.json`, since one test loads it indirectly via
`ConfigLoader`). All test cases should pass; doctest prints a pass/fail summary.

## Composition JSON

See `assets/composition_demo.json` for a worked example: two one-bar patterns, a saw-wave synth
lead instrument, a sample-based kick instrument, and two variation rules (a per-bar pattern swap
and an occasional kick mute). Degree-based synth steps (`"degree"`) are resolved against the
composition's `key`/`scale` once at load time via `Theory::DegreeToFrequency`; steps without a
`"degree"` field are treated as sample triggers.

An optional top-level `"variationStrategy"` field selects which `IVariationStrategy` `main.cpp`
constructs: `"ruleBased"` (default — reads `"variationRules"`) or `"markovChain"` (reads
`"markovChain"`). See `assets/composition_demo_markov.json` for a worked Markov example, using the
same patterns/instruments as the rule-based demo:

```json
"variationStrategy": "markovChain",
"markovChain": {
  "patternTransitions": {
    "pattern_a": [
      { "pattern": "pattern_a", "weight": 0.5 },
      { "pattern": "pattern_b", "weight": 0.5 }
    ],
    "pattern_b": [
      { "pattern": "pattern_a", "weight": 0.8 },
      { "pattern": "pattern_b", "weight": 0.2 }
    ]
  }
}
```

Each key in `"patternTransitions"` is a source pattern id; its array is a weighted list of
possible next patterns (weights need not sum to 1). A pattern with no entry, or whose sampled
next pattern equals the current one, simply keeps playing. `ConfigLoader` throws if
`"variationStrategy"` is `"markovChain"` but no `"markovChain.patternTransitions"` is provided.

## Architecture notes

- **Threading**: the audio callback thread (real-time, miniaudio) and the control thread (`main`,
  driving `Sequencer`/`VariationEngine`) communicate only through `ParameterBus`, a lock-free
  SPSC command queue — see `src/engine/include/engine/ParameterBus.h`.
- **Variation seam**: `IVariationStrategy` is the pluggable interface behind `VariationEngine`.
  Two implementations ship: `RuleBasedVariationStrategy` (weighted-random per rule; the default)
  and `MarkovChainVariationStrategy` (weighted transition table keyed by current pattern id).
  `main.cpp` selects between them based on the composition's `"variationStrategy"` field and
  passes the chosen strategy into `VariationEngine`'s constructor — `Sequencer` and
  `VariationEngine` itself need no changes to support a strategy swap, which is the seam's whole
  point. A further strategy (e.g. weighted by musical tension, or driven by an external signal)
  can be added the same way.
- **Engine is a static lib**, not header-only, so `miniaudio.h`'s implementation
  (`MINIAUDIO_IMPLEMENTATION`) is compiled exactly once, in `src/engine/src/miniaudio_impl.cpp`.
