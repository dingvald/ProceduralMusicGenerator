#pragma once

#include <string>
#include <vector>

#include "engine/CompositionConfig.h"
#include "engine/RandomSource.h"

namespace pmg {

struct VariationContext {
    int barIndex = 0;
    std::string currentPatternId;
    std::vector<VariationRuleConfig> rulesInScope;
};

struct VariationDecision {
    enum class Type { NoOp, SwapPattern, SetTrackMuted };
    Type type = Type::NoOp;
    std::string targetId;   // pattern id (SwapPattern) or track id (SetTrackMuted)
    bool boolValue = false; // muted state (SetTrackMuted)
};

// Pluggable variation-decision seam. Two implementations ship:
// RuleBasedVariationStrategy (weighted random per rule) and
// MarkovChainVariationStrategy (weighted transition table keyed by the
// current pattern id). Either can be constructed and passed into
// VariationEngine with no changes needed here or in Sequencer — this
// interface is the swap point that makes that possible.
class IVariationStrategy {
public:
    virtual ~IVariationStrategy() = default;

    // One rule can yield at most one decision; a bar can produce several
    // decisions (one per rule in scope), applied independently by the
    // caller (e.g. a pattern swap and a track mute in the same bar).
    virtual std::vector<VariationDecision> Decide(const VariationContext& context, RandomSource& rng) = 0;
};

} // namespace pmg
