#include "engine/VariationEngine.h"

#include <utility>

#include "engine/RuleBasedVariationStrategy.h"

namespace pmg {

namespace {

// A rule with no gate is always in scope (this is also what every rule
// looked like before gating existed, so ungated compositions are
// unaffected). A gated rule needs its named parameter's current value to
// lie within [gateMin, gateMax] -- an undeclared parameter name reads as
// 0.0f (see GameParameters::Get), which is only in-scope if that default
// range happens to include 0.0f.
bool RuleGateSatisfied(const VariationRuleConfig& rule, const GameParameters& gameParameters) {
    if (rule.gateParameter.empty()) {
        return true;
    }
    float value = gameParameters.Get(rule.gateParameter);
    return value >= rule.gateMin && value <= rule.gateMax;
}

} // namespace

VariationEngine::VariationEngine(std::vector<VariationRuleConfig> rules, const GameParameters& gameParameters,
                                  std::unique_ptr<IVariationStrategy> strategy)
    : m_rules(std::move(rules)),
      m_gameParameters(gameParameters),
      m_strategy(strategy ? std::move(strategy) : std::make_unique<RuleBasedVariationStrategy>()) {}

std::vector<VariationDecision> VariationEngine::Evaluate(int barIndex, const std::string& currentPatternId,
                                                          RandomSource& rng) {
    VariationContext context;
    context.barIndex = barIndex;
    context.currentPatternId = currentPatternId;
    for (const VariationRuleConfig& rule : m_rules) {
        if (RuleGateSatisfied(rule, m_gameParameters)) {
            context.rulesInScope.push_back(rule);
        }
    }

    return m_strategy->Decide(context, rng);
}

} // namespace pmg
