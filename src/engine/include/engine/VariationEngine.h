#pragma once

#include <memory>
#include <string>
#include <vector>

#include "engine/CompositionConfig.h"
#include "engine/GameParameters.h"
#include "engine/IVariationStrategy.h"
#include "engine/RandomSource.h"

namespace pmg {

// Thin driver: owns the loaded variation rules and a pluggable
// IVariationStrategy (constructor-injected, defaults to
// RuleBasedVariationStrategy). Builds a VariationContext -- filtering rules
// by their optional GameParameters gate first -- and delegates.
class VariationEngine {
public:
    // gameParameters must outlive this VariationEngine; the reference is
    // read (never mutated) once per Evaluate() call, to test each rule's
    // optional gate (VariationRuleConfig::gateParameter).
    explicit VariationEngine(std::vector<VariationRuleConfig> rules, const GameParameters& gameParameters,
                              std::unique_ptr<IVariationStrategy> strategy = nullptr);

    std::vector<VariationDecision> Evaluate(int barIndex, const std::string& currentPatternId, RandomSource& rng);

private:
    std::vector<VariationRuleConfig> m_rules;
    const GameParameters& m_gameParameters;
    std::unique_ptr<IVariationStrategy> m_strategy;
};

} // namespace pmg
