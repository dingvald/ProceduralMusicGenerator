# Procedural Music Generator

A procedural/generative music engine written in modern C++. It supports:

- **Procedural synth playback** — oscillator (sine/saw/square/triangle/noise) + ADSR envelope
  voices. Square supports a configurable **duty cycle** (e.g. 12.5/25/50/75%), like the pulse
  channels on real chip sound hardware (NES, Game Boy) — a 50% square and a 25%-duty pulse are
  recognizably different timbres even at the same pitch. **Noise** is a free-running 15-bit LFSR
  (matching the NES APU's noise channel), for procedurally generated percussion instead of only
  WAV-sample drums.
- **Sample playback** — WAV samples decoded and triggered alongside synth voices.
- **Real-time dynamic variation** — a pluggable `IVariationStrategy` behind `VariationEngine`
  decides pattern swaps, track mutes, and layering live. Two implementations ship: a rule-based,
  weighted-random strategy, and a **Markov-chain strategy** that samples the next pattern from a
  weighted transition table keyed by the pattern currently playing.
- **Simultaneous pattern layering** — beyond the one always-playing base pattern, additional
  patterns (with entirely independent lengths/timing) can be added on top and later removed while
  the base keeps going uninterrupted — e.g. an occasional harmony line layered over a steady
  drum/bass groove.
- **JSON-driven composition** — tempo, instruments, patterns, and variation rules (or a Markov
  transition table) are all defined in a JSON file loaded at startup (see
  `assets/composition_demo.json` and `assets/composition_demo_markov.json`).
- **Lo-fi output stage** — an optional post-mix `LoFiProcessor` quantizes amplitude to a coarse
  bit depth and/or holds samples across a configurable window (naive, unfiltered downsampling),
  emulating a real chip's low-resolution DAC instead of a clean modern one.
- **Arpeggiator** — an optional per-instrument `Arpeggiator` cycles a held note's pitch through a
  list of semitone offsets at a fixed rate, faking a chord on a single channel — the standard way
  chip music implied harmony despite hardware with only 1-3 melodic voices.
- **Compact melody authoring** — instead of writing a full step object per note, a pattern can
  include a `"melodies"` block with a plain note-string like `"A B G F#"`, expanded into steps at
  load time. Pitch is always an absolute note name + octave (e.g. `A4`) — there's no separate
  scale-degree/key system to learn first.

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
  (shipped — `Oscillator::SetDutyCycle`), coarse **bit-depth/sample-hold quantization** on the
  output stage (shipped — `LoFiProcessor`), an LFSR **noise channel** for percussion instead of
  only sample playback (shipped — `Waveform::Noise`), and low polyphony per channel faked into a
  chord via a fast single-channel **arpeggio** (shipped — `Arpeggiator`).
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
DemoApp composition_demo_melody.json --null-audio
DemoApp composition_demo_full.json --null-audio
DemoApp composition_demo_layers.json --null-audio
```

### Rendering to a WAV file (no audio hardware required)

`DemoApp` accepts `--render-wav <path>` (optionally with `--seconds <n>`, default `30`) to render
a composition straight to a 16-bit PCM WAV file instead of opening a playback device at all —
useful for actually hearing the engine's output in an environment with no sound card, or for
capturing a specific run to share:

```
DemoApp composition_demo_full.json --render-wav out.wav --seconds 25
```

This drives the exact same `Sequencer`/`VariationEngine`/`Mixer`/`LoFiProcessor` pipeline as real
playback (via `AudioEngine::InitializeOffline()` + the now-public `AudioEngine::RenderFrames()`),
just as fast as the CPU can go rather than paced to a real-time device callback, and encodes the
output with miniaudio's WAV encoder.

### Running the tests

Build the `Tests` project and run the resulting executable from the `assets/` directory (or any
directory containing `composition_demo.json`, since one test loads it indirectly via
`ConfigLoader`). All test cases should pass; doctest prints a pass/fail summary.

## Composition JSON

See `assets/composition_demo.json` for a worked example: two one-bar patterns, a 25%-duty pulse
synth lead instrument, a sample-based kick instrument, a noise-channel hi-hat instrument, an
arpeggiated chordal pad instrument, and two variation rules (a per-bar pattern swap and an
occasional kick mute). A synth step carries pitch as an absolute note name + octave — `"note"`
(e.g. `"C"`, `"F#"`, `"Bb"`) and optional `"octave"` (default `4`, so `"note": "A"` alone means
A4) — resolved via `Theory::NoteToFrequency` once at load time; steps without a `"note"` field are
treated as sample triggers. A synth step's `"gate"` (fraction of a beat, default `0.25`) is how
long the note is held before it's auto-released.

```json
{ "beat": 1.0, "instrument": "lead_synth", "note": "F#", "octave": 4, "velocity": 0.7, "gate": 0.4 }
```

A synth instrument with `"waveform": "noise"` has no real pitch — its `"note"`/`"octave"` instead
select an LFSR clock rate via the same absolute-pitch resolution as a pitched instrument (a high
octave gives a fast clock and a bright/hissy texture; a low one gives a slow clock and a
duller/rumbling one) — e.g. `assets/composition_demo.json`'s hi-hat uses `"note": "C", "octave": 8`.
`"dutyCycle"` is ignored for noise, like the other non-`Square` waveforms.

### Melody note-strings

Instead of one step object per note, a pattern can include a `"melodies"` array — each entry a
compact note-string that expands into steps at load time:

```json
"melodies": [
  { "instrument": "lead_synth", "notes": "A B G F#" }
]
```

See `assets/composition_demo_melody.json` for a full worked example (a kick/hi-hat percussion
pattern plus a `"melodies"` lead line using exactly `"A B G F#"`).

Each whitespace-separated token in `"notes"` is either a note or a rest:

- **Note**: a letter `A`-`G` (case-insensitive), optional accidental (`#` or lowercase `b`), and
  optional single-digit octave — e.g. `A`, `f#`, `Bb3`. Omitting the octave uses the melody's
  `"defaultOctave"` (default `4`).
- **Rest**: `R` (case-insensitive) — advances time without playing a note.
- Either can end with `:N` to set that token's duration in beats (e.g. `A:2`, `R:0.5`), overriding
  the melody's `"noteLengthBeats"` (default `1.0`) for just that token.

A running beat cursor starts at `"startBeat"` (default `0.0`) and advances by each token's
duration in turn, whether it's a note or a rest. `"gateFraction"` (default `0.8`) is the fraction
of that duration actually held before auto-release, and `"velocity"` (default `0.8`) applies to
every note in the block. A malformed token (unknown letter, double accidental, trailing garbage,
non-positive duration, a rest carrying pitch/octave, etc.) throws, naming the offending token.

A pattern can mix a `"melodies"` block with manual `"steps"` (e.g. a compact lead line alongside
hand-placed percussion) — both `"steps"` and `"melodies"` are optional, but at least one should be
present for the pattern to do anything. **Set `"lengthBars"` to actually cover your melody**: a
pattern loops on `"lengthBars"` × the composition's `beatsPerBar` (see **Architecture notes**
below), and neither manual nor melody-expanded steps are validated against that length — a note
placed beyond it (e.g. an 8-beat melody in a `"lengthBars": 1` pattern, which is only 4 beats) will
silently never play, every cycle, rather than erroring or being deferred. `assets/composition_demo_melody.json`'s
8-note, 8-beat melody uses `"lengthBars": 2` for exactly this reason.

A synth instrument with `"waveform": "square"` accepts an optional `"dutyCycle"` field (0-1,
default `0.5`), matching a chip pulse channel's duty setting — e.g. `0.125`, `0.25`, `0.5`, and
`0.75` are the four duty cycles NES pulse channels support, each with a distinct timbre at the
same pitch. It's ignored for other waveforms; `Waveform::Triangle` always integrates a fixed 50%
pulse internally regardless of this field, since a chip's triangle channel has no duty control.

A synth instrument accepts an optional `"arpeggio"` field:

```json
"arpeggio": { "semitones": [0, 3, 7], "rateHz": 16 }
```

While a note is held, the voice's frequency cycles through `"semitones"` (offsets from the note's
base pitch, restarting from the first offset on every new note) at `"rateHz"` steps per second —
`assets/composition_demo.json`'s `arp_pad` instrument uses this to imply a minor triad on a single
channel. Omitting `"arpeggio"` (or an empty `"semitones"` list) disables it, leaving the voice at
its plain triggered pitch.

### Variation rule option types

Each `"variationRules"` entry's `"options"` array is a weighted list; one option fires per rule
per bar (see **Architecture notes** below for exactly when). `"type"` is one of:

- `"swapPattern"` (+ `"pattern"`): replaces the one always-playing **base** pattern (see layering,
  below) with a different one.
- `"setTrackMuted"` (+ `"track"`, `"muted"`): mutes/unmutes an instrument.
- `"addLayer"` / `"removeLayer"` (+ `"pattern"`): adds or removes an *additional*, independently-timed
  pattern playing simultaneously alongside the base — see `assets/composition_demo_layers.json`,
  where a 3-bar `harmony_layer` pattern is layered on and off over a steady 1-bar `groove` base:

  ```json
  { "type": "addLayer", "pattern": "harmony_layer", "weight": 0.25 },
  { "type": "removeLayer", "pattern": "harmony_layer", "weight": 0.15 }
  ```

  `addLayer` for a pattern already active, or `removeLayer` for one that isn't, is a no-op —
  rules don't need to track layer state themselves. `removeLayer` can never remove the base
  pattern (only `swapPattern` replaces that), so a composition can't end up with zero patterns
  playing. A removed layer's already-triggered notes ring out naturally through their own
  envelope/gate rather than cutting off abruptly; only *new* notes from that layer stop.
- `"noOp"`: does nothing — useful as a weighted "most of the time, don't change anything" option.

`"addLayer"`/`"removeLayer"` are only available under `"variationStrategy": "ruleBased"` —
`MarkovChainVariationStrategy` only ever produces `swapPattern` decisions (or none), since it
models transitions between mutually-exclusive base-pattern states, not layering.

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

An optional top-level `"loFi"` object configures the post-mix `LoFiProcessor` (see both demo
compositions):

```json
"loFi": { "bitDepth": 4, "holdFactor": 4 }
```

- `"bitDepth"` (default `16`, clamped to `[1, 16]`): quantizes the final mixed sample to
  `2^bitDepth` amplitude levels. `16` is a no-op; `4` (16 levels) gives a rough NES-DAC-like
  graininess.
- `"holdFactor"` (default `1`, clamped to `>= 1`): a naive, unfiltered sample-and-hold — the
  output only updates once every N samples, aliasing on purpose rather than being cleanly
  resampled. `1` is a no-op; `4` at a 48kHz device rate behaves like a ~12kHz update rate.

Both fields are independent and can be used alone or together. Omitting `"loFi"` entirely leaves
output unquantized, matching pre-`LoFiProcessor` behavior.

### Full worked example: every feature in one composition

The annotated listing below walks through a single composition that exercises every feature
described above at once: all five waveforms (via 7 instruments — square lead, saw harmony,
triangle bass, an arpeggiated square pad, a sample kick, and two differently-pitched noise
instruments for snare/hi-hat), three patterns (a sparse `"intro"` built from manual `"steps"`
only, plus a `"verse"` and `"chorus"` that each layer **three simultaneous `"melodies"` note-strings**
— lead, harmony, bass — on top of manual percussion steps in the same pattern), rule-based
variation (pattern swapping + two independent mute rules), a Markov transition table (present but
inert here — see the note below), and a lo-fi output stage. It isn't meant to sound good, just to
show the JSON shape for each feature side by side.

JSON has no comment syntax, so this listing uses `//` purely for this explanation — it is **not**
valid JSON as written. The exact same composition with the comments stripped is a real, loadable
file at `assets/composition_demo_full.json` (verified against `ConfigLoader` and runnable via
`DemoApp composition_demo_full.json --null-audio`); use that file, not this listing, if you want to
copy-paste a starting point.

```jsonc
{
  // Global playback settings: 100 BPM, 4 beats/bar, and which pattern plays first.
  "sampleRate": 48000,
  "tempo": { "bpm": 100, "beatsPerBar": 4 },
  "startPattern": "intro",

  // Which IVariationStrategy VariationEngine uses. "ruleBased" (shown here) reads
  // "variationRules" below; "markovChain" would instead read "markovChain" below. Both blocks
  // are present in this file for illustration, but only the one matching this field actually
  // drives playback — the other is parsed and otherwise ignored.
  "variationStrategy": "ruleBased",

  // Post-mix bit-crush + sample-and-hold, applied once to the final mixed signal.
  "loFi": { "bitDepth": 4, "holdFactor": 3 },

  "instruments": [
    {
      // A 25%-duty pulse wave lead — the "melodies" block below drives this one.
      "id": "lead_synth",
      "type": "synth",
      "waveform": "square",
      "dutyCycle": 0.25,
      "gain": 0.32,
      "envelope": { "attack": 0.01, "decay": 0.12, "sustain": 0.55, "release": 0.2 }
    },
    {
      // Sawtooth (unused by the other demo files) as a second, harmony-line voice.
      "id": "harmony_synth",
      "type": "synth",
      "waveform": "saw",
      "gain": 0.22,
      "envelope": { "attack": 0.02, "decay": 0.15, "sustain": 0.4, "release": 0.25 }
    },
    {
      // Triangle wave (also unused elsewhere) as a punchy, short-sustain bass voice.
      "id": "bass_synth",
      "type": "synth",
      "waveform": "triangle",
      "gain": 0.4,
      "envelope": { "attack": 0.005, "decay": 0.08, "sustain": 0.8, "release": 0.05 }
    },
    {
      // Held notes cycle through a major triad + octave at 12 steps/sec, faking a chord
      // on one channel. "arpeggio" is what makes this differ from a plain square instrument.
      "id": "arp_pad",
      "type": "synth",
      "waveform": "square",
      "dutyCycle": 0.5,
      "arpeggio": { "semitones": [0, 4, 7, 12], "rateHz": 12 },
      "gain": 0.15,
      "envelope": { "attack": 0.03, "decay": 0.2, "sustain": 0.6, "release": 0.4 }
    },
    {
      // A "sample" instrument instead of "synth" — plays a decoded WAV, no waveform/envelope.
      "id": "kick",
      "type": "sample",
      "file": "samples/kick.wav",
      "gain": 0.95
    },
    {
      // Noise-channel snare. Its "note"/"octave" (below, in the steps) select an LFSR clock
      // rate rather than a pitch — a lower octave than the hi-hat gives a duller rattle.
      "id": "snare",
      "type": "synth",
      "waveform": "noise",
      "gain": 0.28,
      "envelope": { "attack": 0.001, "decay": 0.09, "sustain": 0.0, "release": 0.02 }
    },
    {
      // Same noise channel, driven at a much higher octave for a bright, hissy hi-hat.
      "id": "hihat",
      "type": "synth",
      "waveform": "noise",
      "gain": 0.22,
      "envelope": { "attack": 0.001, "decay": 0.035, "sustain": 0.0, "release": 0.01 }
    }
  ],

  "patterns": [
    {
      // A 1-bar intro with no melodies at all — just a held arp chord and steady hi-hats,
      // to show a pattern can be built from "steps" alone.
      "id": "intro",
      "lengthBars": 1,
      "steps": [
        { "beat": 0.0, "instrument": "arp_pad", "note": "C", "octave": 3, "velocity": 0.5, "gate": 3.8 },
        { "beat": 0.0, "instrument": "hihat", "note": "C", "octave": 8, "velocity": 0.4 }
        // ... hi-hat continues on every off-beat through beat 3.5; see the real file.
      ]
    },
    {
      // 2 bars (8 beats) — long enough for the melodies below, which each total 8 beats
      // themselves (see "Set lengthBars to actually cover your melody" earlier in this doc).
      "id": "verse",
      "lengthBars": 2,
      "melodies": [
        {
          // Lead line: default octave 4, mixes bare note letters ("E" -> E4), a duration
          // override ("B:0.5"), a rest with its own override ("R:0.5"), an accidental
          // ("F#"), and a stretched final note ("F#:2"). Beats: 1+1+1+.5+.5+1+1+2 = 8.
          "instrument": "lead_synth",
          "notes": "E G A B:0.5 R:0.5 A G F#:2",
          "defaultOctave": 4,
          "noteLengthBeats": 1.0,
          "gateFraction": 0.75,
          "velocity": 0.8
        },
        {
          // Second, simultaneous melody line on a different instrument — this is what
          // "multiple melodies in one pattern" means: both blocks share the same beat clock.
          // "noteLengthBeats": 2.0 makes every un-overridden token 2 beats (C,R,E = 6 beats),
          // then "D:1" and a trailing "R:1" bring the total to 8.
          "instrument": "harmony_synth",
          "notes": "C R E D:1 R:1",
          "defaultOctave": 3,
          "noteLengthBeats": 2.0,
          "gateFraction": 0.6,
          "velocity": 0.55
        },
        {
          // A third, simultaneous melody — a root-note bass line under the lead/harmony,
          // using explicit octaves ("E2") instead of relying on "defaultOctave".
          "instrument": "bass_synth",
          "notes": "E2 E2 G2 G2 A2 A2 G2 R",
          "defaultOctave": 2,
          "noteLengthBeats": 1.0,
          "gateFraction": 0.9,
          "velocity": 0.75
        }
      ],
      "steps": [
        // Manual "steps" mixed into the same pattern as the "melodies" above: a long-held
        // arp chord, and a kick/snare/hi-hat percussion grid across all 8 beats.
        { "beat": 0.0, "instrument": "arp_pad", "note": "C", "octave": 3, "velocity": 0.5, "gate": 7.8 },
        { "beat": 0.0, "instrument": "kick", "velocity": 1.0 },
        { "beat": 1.0, "instrument": "snare", "note": "C", "octave": 5, "velocity": 0.6 }
        // ... full kick/snare/hi-hat grid continues through beat 7.5; see the real file.
      ]
    },
    {
      // A second full section, same shape as "verse" (three simultaneous melodies + manual
      // percussion) but different notes/chord ("F" instead of "C") and a busier kick pattern,
      // so the variation rules below have two contrasting sections to swap between.
      "id": "chorus",
      "lengthBars": 2,
      "melodies": [
        { "instrument": "lead_synth", "notes": "A B C5 D5 C5 B A:2", "defaultOctave": 4,
          "noteLengthBeats": 1.0, "gateFraction": 0.7, "velocity": 0.9 },
        { "instrument": "harmony_synth", "notes": "F R G R", "defaultOctave": 3,
          "noteLengthBeats": 2.0, "gateFraction": 0.6, "velocity": 0.55 },
        { "instrument": "bass_synth", "notes": "F2 F2 G2 G2 A2 A2 G2 R", "defaultOctave": 2,
          "noteLengthBeats": 1.0, "gateFraction": 0.9, "velocity": 0.8 }
      ],
      "steps": [
        { "beat": 0.0, "instrument": "arp_pad", "note": "F", "octave": 3, "velocity": 0.55, "gate": 7.8 },
        { "beat": 0.0, "instrument": "kick", "velocity": 1.0 },
        { "beat": 1.5, "instrument": "kick", "velocity": 0.8 }
        // ... full grid continues; see the real file.
      ]
    }
  ],

  // Read by RuleBasedVariationStrategy since "variationStrategy" above is "ruleBased".
  // Each rule fires once per its "scope" and samples one weighted option.
  "variationRules": [
    {
      // Swaps which of the 3 patterns is active, weighted toward "verse".
      "id": "swap_section",
      "scope": "perBar",
      "options": [
        { "type": "swapPattern", "pattern": "intro", "weight": 0.05 },
        { "type": "swapPattern", "pattern": "verse", "weight": 0.55 },
        { "type": "swapPattern", "pattern": "chorus", "weight": 0.3 },
        { "type": "noOp", "weight": 0.1 }
      ]
    },
    {
      // A second, independent rule — occasionally drops the bass out for a bar.
      "id": "mute_bass_occasionally",
      "scope": "perBar",
      "options": [
        { "type": "setTrackMuted", "track": "bass_synth", "muted": true, "weight": 0.2 },
        { "type": "setTrackMuted", "track": "bass_synth", "muted": false, "weight": 0.8 }
      ]
    },
    {
      // A third, independent rule — occasionally drops the snare too.
      "id": "mute_snare_occasionally",
      "scope": "perBar",
      "options": [
        { "type": "setTrackMuted", "track": "snare", "muted": true, "weight": 0.15 },
        { "type": "setTrackMuted", "track": "snare", "muted": false, "weight": 0.85 }
      ]
    }
  ],

  // Shown for reference only in this file: with "variationStrategy": "ruleBased" above,
  // MarkovChainVariationStrategy never runs, so this table is parsed but has no effect on
  // playback. Flip "variationStrategy" to "markovChain" to make this the active strategy
  // instead (and the "variationRules" above would then become the inert one).
  "markovChain": {
    "patternTransitions": {
      "intro": [
        { "pattern": "verse", "weight": 1.0 }
      ],
      "verse": [
        { "pattern": "verse", "weight": 0.4 },
        { "pattern": "chorus", "weight": 0.5 },
        { "pattern": "intro", "weight": 0.1 }
      ],
      "chorus": [
        { "pattern": "verse", "weight": 0.6 },
        { "pattern": "chorus", "weight": 0.4 }
      ]
    }
  }
}
```

## Architecture notes

- **Threading**: the audio callback thread (real-time, miniaudio) and the control thread (`main`,
  driving `Sequencer`/`VariationEngine`) communicate only through `ParameterBus`, a lock-free
  SPSC command queue — see `src/engine/include/engine/ParameterBus.h`.
- **A pattern loops on its own length, not the global bar cadence**: `Sequencer::Update` tracks two
  independent things from the same elapsed-beat clock — how often to ask `VariationEngine` for a
  decision (every composition-wide `beatsPerBar`, e.g. `"scope": "perBar"` rules firing every 4
  beats) and how often each *active layer's* step-scan cursor wraps back to its own pattern's beat
  0 (`pattern.lengthBars * beatsPerBar`, tracked per layer via an anchor beat,
  `PatternLayer::patternStartBeat`, that advances by whole pattern-cycles rather than snapping to
  "now" so playback stays locked to the beat grid instead of drifting). These used to be the same
  period (the step-scan reset lived inside the bar-evaluation loop), which meant any pattern
  longer than one bar had its tail beyond beat `beatsPerBar` permanently skipped every cycle —
  invisible until melody note-strings made multi-bar patterns with real content past beat 4
  common. A pattern swap (mid-`Update`, via a variation decision) resets the anchor to the swap
  instant, so the newly active pattern always starts from its own beat 0 rather than wherever it'd
  land phase-locked to the old pattern's cycle. `Sequencer` has no direct unit tests (it needs a
  real `AudioEngine` for its elapsed-frame clock, which doesn't fit this project's fast/
  deterministic doctest style) — this fix, and the layering mechanism below, were both verified by
  temporarily logging trigger events (a 2-bar melody firing all 8 notes in order and looping
  cleanly; a layer's notes firing concurrently with the base once added, stopping cleanly once
  removed, and restarting from its own beat 0 on re-add) against both a reverted and a fixed
  build.
- **Layering**: `Sequencer` owns a `std::vector<PatternLayer>` (see `Sequencer.h`) instead of a
  single current-pattern id. Layer `[0]` is the base (seeded from `"startPattern"`; only
  `swapPattern` decisions ever touch it); `addLayer`/`removeLayer` decisions push/erase layers at
  index >= 1. Every active layer runs the same independent step-scan/cycle logic described above
  (`Sequencer::UpdateLayer`), so a 12-beat layer and a 4-beat base genuinely play at once, each on
  its own clock — this needed no changes anywhere below `Sequencer` (`Mixer` already handles
  concurrent `NoteOn`s to the same instrument, one per available voice).
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
- **Output pipeline**: `AudioEngine::RenderFrames` calls `Mixer::RenderNextSample()` for the
  mixed-down mono sample, then always runs it through `LoFiProcessor::Process` before writing to
  the device buffer — the processor is a no-op when unconfigured, so this costs nothing when
  `"loFi"` is absent from the composition. It's a single global stage (like a real chip's shared
  DAC), not per-instrument.
- **Noise channel**: `Oscillator`'s `Waveform::Noise` case is a 15-bit Fibonacci LFSR (taps at
  bits 0/1, matching the NES APU's noise channel), clocked once per period at the voice's
  configured frequency and held between clocks. `Reset()` deliberately does not reseed it, so a
  retriggered noise instrument (e.g. a hi-hat hit repeatedly) doesn't replay an identical
  pseudo-random sequence every time — the LFSR free-runs continuously off its own clock, like real
  hardware's noise generator does, independent of note triggers.
- **Gate-based auto-release**: a step's `"gate"` (a fraction of a beat) is converted by
  `Sequencer::FireStep` into a sample count and carried on the `NoteOn` command itself
  (`Command::intValue` — reused for this rather than adding a field, since it's otherwise only
  meaningful on `NoteOff`). `SynthVoice` counts those samples down and calls the envelope's
  `NoteOff()` itself once they elapse, entirely on the audio thread. This sidesteps a real
  constraint: the control thread that fires a step never learns which voice handle `Mixer`
  assigned it (that only exists on the audio thread, after the command is drained), so a
  *separately scheduled* `NoteOff` command could never target the right voice — baking the
  duration into the `NoteOn` itself avoids needing to route one by instrument id at all. Passing
  no gate duration (the default, `-1`) holds a voice until an explicit `NoteOff` instead, e.g. for
  a note with indefinite/manual duration.
- **One-shot (zero-sustain) envelopes free their voice even faster**: independent of the above,
  `Envelope`'s `Decay` stage transitions straight to `Idle` (instead of holding in `Sustain`) when
  `"sustain"` is `0`, so a percussive one-shot instrument's voice is freed as soon as it finishes
  decaying — typically well before its gate would even elapse.
- **Arpeggiator**: `SynthVoice` recomputes its oscillator's frequency every sample as
  `baseFrequency * Arpeggiator::NextMultiplier()`, where the multiplier is `2^(semitoneOffset/12)`
  for whichever offset the arpeggio is currently on. `NoteOn()` resets the arpeggio to its first
  offset (not wherever a prior note's cycle left off), matching how chip trackers restart an arp
  per note. With no configured offsets, `NextMultiplier()` always returns `1.0`, so an
  unarpeggiated voice's frequency math is unchanged.
- **Melody note-strings are pure JSON-authoring sugar**: `NoteStringParser::ParseNoteString` is
  plain string parsing with no JSON dependency (independently unit-tested), called by
  `ConfigLoader::ParsePattern` for each `"melodies"` entry; its output is ordinary `StepConfig`s
  appended to the pattern's `steps`, so `Pattern::ResolvePattern`, `Sequencer`, `SynthVoice`, and
  `ParameterBus` have no idea a step came from a note-string rather than a hand-written step
  object — everything downstream of `ConfigLoader` treats them identically.
- **Absolute pitch, not scale degrees**: pitch used to be scale-degree-based (`"degree"` resolved
  against a composition-wide `"key"`/`"scale"`), but that added a second, more abstract way to say
  "which pitch" alongside note-string authoring's absolute notes. It was removed in favor of one
  mechanism: every synth step now carries an absolute `NoteName` + octave, resolved via
  `Theory::NoteToFrequency` (a plain MIDI-math function, no key/scale context needed) in
  `Pattern::ResolvePattern`. This is also why the noise channel's `"note"`/`"octave"`-selects-an-
  LFSR-clock-rate behavior (above) needs no special-casing to explain: it's the exact same
  resolution path every pitched step already goes through.
