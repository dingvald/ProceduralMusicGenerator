#pragma once

#include <memory>
#include <string>
#include <vector>

#include "engine/CompositionConfig.h"
#include "engine/IVariationStrategy.h"
#include "engine/RandomSource.h"

namespace pmg {

// Thin driver: owns the loaded variation rules and a pluggable
// IVariationStrategy (constructor-injected, defaults to
// RuleBasedVariationStrategy). Builds a VariationContext and delegates.
class VariationEngine {
public:
    explicit VariationEngine(std::vector<VariationRuleConfig> rules,
                              std::unique_ptr<IVariationStrategy> strategy = nullptr);

    std::vector<VariationDecision> Evaluate(int barIndex, const std::string& currentPatternId, RandomSource& rng);

private:
    std::vector<VariationRuleConfig> m_rules;
    std::unique_ptr<IVariationStrategy> m_strategy;
};

} // namespace pmg
