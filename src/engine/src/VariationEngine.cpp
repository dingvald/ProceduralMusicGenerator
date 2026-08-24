#include "engine/VariationEngine.h"

#include <utility>

#include "engine/RuleBasedVariationStrategy.h"

namespace pmg {

VariationEngine::VariationEngine(std::vector<VariationRuleConfig> rules, std::unique_ptr<IVariationStrategy> strategy)
    : m_rules(std::move(rules)),
      m_strategy(strategy ? std::move(strategy) : std::make_unique<RuleBasedVariationStrategy>()) {}

std::vector<VariationDecision> VariationEngine::Evaluate(int barIndex, const std::string& currentPatternId,
                                                          RandomSource& rng) {
    VariationContext context;
    context.barIndex = barIndex;
    context.currentPatternId = currentPatternId;
    context.rulesInScope = m_rules; // v1: all loaded rules are always in scope; `scope` field reserved for future filtering

    return m_strategy->Decide(context, rng);
}

} // namespace pmg
