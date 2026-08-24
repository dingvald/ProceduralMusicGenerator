#pragma once

#include "engine/IVariationStrategy.h"

namespace pmg {

// Rule-based / weighted-random variation strategy: for each rule in scope,
// picks one of its options via RandomSource::NextWeightedIndex and
// translates a non-NoOp choice into a VariationDecision.
class RuleBasedVariationStrategy : public IVariationStrategy {
public:
    std::vector<VariationDecision> Decide(const VariationContext& context, RandomSource& rng) override;
};

} // namespace pmg
