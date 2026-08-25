#pragma once

#include "engine/CompositionConfig.h"
#include "engine/IVariationStrategy.h"

namespace pmg {

// Markov-chain variation strategy: given the current pattern, picks the
// next pattern by sampling a weighted transition table loaded from the
// JSON composition's "markovChain.patternTransitions". Unlike
// RuleBasedVariationStrategy, a decision depends only on the current state
// (the pattern just played) rather than on independently-configured rules
// — this is IVariationStrategy's intended plug-in seam in action: it can
// be constructed and handed to VariationEngine with no changes to
// VariationEngine or Sequencer.
class MarkovChainVariationStrategy : public IVariationStrategy {
public:
    explicit MarkovChainVariationStrategy(MarkovChainConfig transitions);

    std::vector<VariationDecision> Decide(const VariationContext& context, RandomSource& rng) override;

private:
    MarkovChainConfig m_transitions;
};

} // namespace pmg
