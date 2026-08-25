#pragma once

#include <vector>

#include "engine/CompositionConfig.h"

namespace pmg {

// Parses a MelodyConfig's compact note-string (e.g. "A B G F#") into
// StepConfig entries, ready to append to a PatternConfig's steps.
//
// Grammar, per whitespace-separated token:
//   token       := restToken | noteToken
//   restToken   := ('R'|'r') durationSuffix?
//   noteToken   := noteLetter accidental? octaveDigit? durationSuffix?
//   noteLetter  := 'A'..'G' | 'a'..'g'
//   accidental  := '#' | 'b'        (lowercase 'b' only; unambiguous, since
//                                    a second note letter can't appear
//                                    mid-token)
//   octaveDigit := '0'..'9'         (overrides MelodyConfig::defaultOctave
//                                    for just this note)
//   durationSuffix := ':' number    (absolute beats for this token's slot;
//                                    must be > 0; overrides
//                                    MelodyConfig::noteLengthBeats)
//
// A running beat cursor starts at melody.startBeat. Each token advances the
// cursor by its slot duration (per-token override if present, else
// noteLengthBeats), whether it's a note or a rest; only notes emit a
// StepConfig, at gate = slotDuration * gateFraction.
//
// Pure string parsing -- no JSON dependency, so this stays independently
// testable and keeps nlohmann::json confined to ConfigLoader. Throws
// std::runtime_error naming the offending token on any malformed input.
// Empty or whitespace-only strings are legal and produce an empty result.
std::vector<StepConfig> ParseNoteString(const MelodyConfig& melody);

} // namespace pmg
