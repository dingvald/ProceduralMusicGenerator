#include "engine/RuleBasedVariationStrategy.h"

namespace pmg {

std::vector<VariationDecision> RuleBasedVariationStrategy::Decide(const VariationContext& context, RandomSource& rng) {
    std::vector<VariationDecision> decisions;

    for (const VariationRuleConfig& rule : context.rulesInScope) {
        if (rule.options.empty()) {
            continue;
        }

        std::vector<float> weights;
        weights.reserve(rule.options.size());
        for (const VariationOptionConfig& option : rule.options) {
            weights.push_back(option.weight);
        }

        int chosenIndex = rng.NextWeightedIndex(weights);
        if (chosenIndex < 0) {
            continue;
        }

        const VariationOptionConfig& chosen = rule.options[static_cast<size_t>(chosenIndex)];
        if (chosen.type == VariationOptionType::NoOp) {
            continue;
        }

        VariationDecision decision;
        decision.targetId = chosen.targetId;
        decision.boolValue = chosen.boolValue;
        decision.type = chosen.type == VariationOptionType::SwapPattern
            ? VariationDecision::Type::SwapPattern
            : VariationDecision::Type::SetTrackMuted;
        decisions.push_back(decision);
    }

    return decisions;
}

} // namespace pmg
