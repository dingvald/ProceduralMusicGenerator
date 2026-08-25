#include "engine/MarkovChainVariationStrategy.h"

#include <utility>

namespace pmg {

MarkovChainVariationStrategy::MarkovChainVariationStrategy(MarkovChainConfig transitions)
    : m_transitions(std::move(transitions)) {}

std::vector<VariationDecision> MarkovChainVariationStrategy::Decide(const VariationContext& context, RandomSource& rng) {
    std::vector<VariationDecision> decisions;

    auto it = m_transitions.find(context.currentPatternId);
    if (it == m_transitions.end() || it->second.empty()) {
        return decisions; // no outgoing transitions configured for this state; stay put
    }

    const std::vector<MarkovTransitionConfig>& outgoing = it->second;
    std::vector<float> weights;
    weights.reserve(outgoing.size());
    for (const MarkovTransitionConfig& transition : outgoing) {
        weights.push_back(transition.weight);
    }

    int chosenIndex = rng.NextWeightedIndex(weights);
    if (chosenIndex < 0) {
        return decisions;
    }

    const std::string& nextPattern = outgoing[static_cast<size_t>(chosenIndex)].toPattern;
    if (nextPattern == context.currentPatternId) {
        return decisions; // self-transition: stay on the current pattern, nothing to apply
    }

    VariationDecision decision;
    decision.type = VariationDecision::Type::SwapPattern;
    decision.targetId = nextPattern;
    decisions.push_back(decision);
    return decisions;
}

} // namespace pmg
