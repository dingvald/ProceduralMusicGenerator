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
        switch (chosen.type) {
            case VariationOptionType::SwapPattern:
                decision.type = VariationDecision::Type::SwapPattern;
                break;
            case VariationOptionType::SetTrackMuted:
                decision.type = VariationDecision::Type::SetTrackMuted;
                break;
            case VariationOptionType::AddLayer:
                decision.type = VariationDecision::Type::AddLayer;
                break;
            case VariationOptionType::RemoveLayer:
                decision.type = VariationDecision::Type::RemoveLayer;
                break;
            case VariationOptionType::NoOp:
                break; // already filtered out above; kept for switch completeness
        }
        decisions.push_back(decision);
    }

    return decisions;
}

} // namespace pmg
