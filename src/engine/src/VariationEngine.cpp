#include "engine/VariationEngine.h"

#include <algorithm>
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

// An option with no weightParameter is returned unscaled (also what every
// option looked like before weight scaling existed). Otherwise its weight
// is multiplied by a factor that ramps linearly from weightMultiplierAtMin
// to weightMultiplierAtMax as the named parameter crosses
// [paramAtWeightMin, paramAtWeightMax], clamped outside that range -- the
// same shape as GainCrossfadeConfig's mapping. Negative results are clamped
// to 0 (RandomSource::NextWeightedIndex already treats a non-positive
// weight as "never chosen", but a negative effective weight would be
// confusing to reason about even though it's handled safely).
VariationOptionConfig ApplyWeightScaling(VariationOptionConfig option, const GameParameters& gameParameters) {
    if (option.weightParameter.empty()) {
        return option;
    }

    float value = gameParameters.Get(option.weightParameter);
    float range = option.paramAtWeightMax - option.paramAtWeightMin;
    float t = range != 0.0f ? (value - option.paramAtWeightMin) / range
                             : (value >= option.paramAtWeightMin ? 1.0f : 0.0f);
    t = std::clamp(t, 0.0f, 1.0f);
    float multiplier = option.weightMultiplierAtMin + t * (option.weightMultiplierAtMax - option.weightMultiplierAtMin);

    option.weight = std::max(0.0f, option.weight * multiplier);
    return option;
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
        if (!RuleGateSatisfied(rule, m_gameParameters)) {
            continue;
        }

        VariationRuleConfig scaledRule = rule;
        for (VariationOptionConfig& option : scaledRule.options) {
            option = ApplyWeightScaling(std::move(option), m_gameParameters);
        }
        context.rulesInScope.push_back(std::move(scaledRule));
    }

    return m_strategy->Decide(context, rng);
}

} // namespace pmg
