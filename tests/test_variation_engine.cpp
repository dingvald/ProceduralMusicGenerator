#include <map>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "engine/GameParameters.h"
#include "engine/RuleBasedVariationStrategy.h"
#include "engine/VariationEngine.h"

using namespace pmg;

namespace {

VariationRuleConfig MakeRule() {
    VariationRuleConfig rule;
    rule.id = "r";

    VariationOptionConfig optionB;
    optionB.type = VariationOptionType::SwapPattern;
    optionB.targetId = "pattern_b";
    optionB.weight = 0.3f;

    VariationOptionConfig optionA;
    optionA.type = VariationOptionType::SwapPattern;
    optionA.targetId = "pattern_a";
    optionA.weight = 0.7f;

    rule.options = {optionB, optionA};
    return rule;
}

} // namespace

TEST_CASE("RuleBasedVariationStrategy is deterministic for a fixed seed") {
    VariationContext context;
    context.barIndex = 0;
    context.currentPatternId = "pattern_a";
    context.rulesInScope = {MakeRule()};

    RuleBasedVariationStrategy strategy;
    RandomSource rng1(42);
    RandomSource rng2(42);

    for (int i = 0; i < 50; ++i) {
        auto d1 = strategy.Decide(context, rng1);
        auto d2 = strategy.Decide(context, rng2);
        REQUIRE(d1.size() == d2.size());
        for (size_t j = 0; j < d1.size(); ++j) {
            CHECK(d1[j].targetId == d2[j].targetId);
            CHECK(d1[j].type == d2[j].type);
        }
    }
}

TEST_CASE("RuleBasedVariationStrategy selection frequency tracks configured weights") {
    VariationContext context;
    context.barIndex = 0;
    context.currentPatternId = "pattern_a";
    context.rulesInScope = {MakeRule()};

    RuleBasedVariationStrategy strategy;
    RandomSource rng(1234);

    const int trials = 20000;
    std::map<std::string, int> counts;
    for (int i = 0; i < trials; ++i) {
        auto decisions = strategy.Decide(context, rng);
        REQUIRE(decisions.size() == 1);
        counts[decisions[0].targetId]++;
    }

    double freqB = static_cast<double>(counts["pattern_b"]) / trials;
    double freqA = static_cast<double>(counts["pattern_a"]) / trials;

    CHECK(freqB == doctest::Approx(0.3).epsilon(0.05));
    CHECK(freqA == doctest::Approx(0.7).epsilon(0.05));
}

TEST_CASE("RuleBasedVariationStrategy maps each option type to its own distinct decision type") {
    // Regression test: the type mapping used to be a two-way ternary
    // (SwapPattern vs. everything else treated as SetTrackMuted), which
    // would have silently misclassified AddLayer/RemoveLayer options.
    VariationRuleConfig swapRule;
    swapRule.id = "swap";
    VariationOptionConfig swapOption;
    swapOption.type = VariationOptionType::SwapPattern;
    swapOption.targetId = "pattern_b";
    swapOption.weight = 1.0f;
    swapRule.options = {swapOption};

    VariationRuleConfig muteRule;
    muteRule.id = "mute";
    VariationOptionConfig muteOption;
    muteOption.type = VariationOptionType::SetTrackMuted;
    muteOption.targetId = "kick";
    muteOption.boolValue = true;
    muteOption.weight = 1.0f;
    muteRule.options = {muteOption};

    VariationRuleConfig addRule;
    addRule.id = "add";
    VariationOptionConfig addOption;
    addOption.type = VariationOptionType::AddLayer;
    addOption.targetId = "harmony_layer";
    addOption.weight = 1.0f;
    addRule.options = {addOption};

    VariationRuleConfig removeRule;
    removeRule.id = "remove";
    VariationOptionConfig removeOption;
    removeOption.type = VariationOptionType::RemoveLayer;
    removeOption.targetId = "harmony_layer";
    removeOption.weight = 1.0f;
    removeRule.options = {removeOption};

    VariationContext context;
    context.barIndex = 0;
    context.currentPatternId = "pattern_a";
    context.rulesInScope = {swapRule, muteRule, addRule, removeRule};

    RuleBasedVariationStrategy strategy;
    RandomSource rng(1);
    std::vector<VariationDecision> decisions = strategy.Decide(context, rng);

    REQUIRE(decisions.size() == 4);
    CHECK(decisions[0].type == VariationDecision::Type::SwapPattern);
    CHECK(decisions[0].targetId == "pattern_b");
    CHECK(decisions[1].type == VariationDecision::Type::SetTrackMuted);
    CHECK(decisions[1].targetId == "kick");
    CHECK(decisions[1].boolValue == true);
    CHECK(decisions[2].type == VariationDecision::Type::AddLayer);
    CHECK(decisions[2].targetId == "harmony_layer");
    CHECK(decisions[3].type == VariationDecision::Type::RemoveLayer);
    CHECK(decisions[3].targetId == "harmony_layer");
}

namespace {

VariationRuleConfig MakeGatedSwapRule(const std::string& gateParameter, float gateMin, float gateMax) {
    VariationRuleConfig rule;
    rule.id = "gated";
    rule.gateParameter = gateParameter;
    rule.gateMin = gateMin;
    rule.gateMax = gateMax;

    VariationOptionConfig option;
    option.type = VariationOptionType::SwapPattern;
    option.targetId = "intense";
    option.weight = 1.0f;
    rule.options = {option};
    return rule;
}

} // namespace

TEST_CASE("VariationEngine excludes a gated rule when its parameter is outside [gateMin, gateMax]") {
    GameParameters params({GameParameterConfig{"danger", 0.0f}});
    VariationEngine engine({MakeGatedSwapRule("danger", 0.5f, 1.0f)}, params);

    RandomSource rng(1);
    std::vector<VariationDecision> decisions = engine.Evaluate(0, "calm", rng);
    CHECK(decisions.empty());
}

TEST_CASE("VariationEngine includes a gated rule once its parameter enters [gateMin, gateMax]") {
    GameParameters params({GameParameterConfig{"danger", 0.0f}});
    VariationEngine engine({MakeGatedSwapRule("danger", 0.5f, 1.0f)}, params);

    params.Set("danger", 0.75f);
    RandomSource rng(1);
    std::vector<VariationDecision> decisions = engine.Evaluate(0, "calm", rng);

    REQUIRE(decisions.size() == 1);
    CHECK(decisions[0].type == VariationDecision::Type::SwapPattern);
    CHECK(decisions[0].targetId == "intense");
}

TEST_CASE("VariationEngine treats an ungated rule (empty gateParameter) as always in scope") {
    GameParameters params({});
    VariationEngine engine({MakeGatedSwapRule("", 0.5f, 1.0f)}, params);

    RandomSource rng(1);
    std::vector<VariationDecision> decisions = engine.Evaluate(0, "calm", rng);

    REQUIRE(decisions.size() == 1);
    CHECK(decisions[0].targetId == "intense");
}

TEST_CASE("VariationEngine gate is boundary-inclusive at both gateMin and gateMax") {
    GameParameters params({GameParameterConfig{"danger", 0.5f}});
    VariationEngine lowerBound({MakeGatedSwapRule("danger", 0.5f, 1.0f)}, params);
    RandomSource rng1(1);
    CHECK(lowerBound.Evaluate(0, "calm", rng1).size() == 1);

    params.Set("danger", 1.0f);
    VariationEngine upperBound({MakeGatedSwapRule("danger", 0.5f, 1.0f)}, params);
    RandomSource rng2(1);
    CHECK(upperBound.Evaluate(0, "calm", rng2).size() == 1);
}

namespace {

VariationRuleConfig MakeWeightScaledChoiceRule() {
    VariationRuleConfig rule;
    rule.id = "choice";

    VariationOptionConfig stayCalm;
    stayCalm.type = VariationOptionType::SwapPattern;
    stayCalm.targetId = "calm";
    stayCalm.weight = 1.0f; // unscaled -- stays fixed regardless of the parameter

    VariationOptionConfig escalate;
    escalate.type = VariationOptionType::SwapPattern;
    escalate.targetId = "intense";
    escalate.weight = 1.0f;
    escalate.weightParameter = "danger";
    escalate.paramAtWeightMin = 0.0f;
    escalate.weightMultiplierAtMin = 0.1f; // rare at danger == 0
    escalate.paramAtWeightMax = 1.0f;
    escalate.weightMultiplierAtMax = 9.0f; // dominant at danger == 1

    rule.options = {stayCalm, escalate};
    return rule;
}

} // namespace

TEST_CASE("VariationEngine weight scaling shifts selection frequency as the parameter changes") {
    GameParameters params({GameParameterConfig{"danger", 0.0f}});
    VariationEngine lowDanger({MakeWeightScaledChoiceRule()}, params);
    RandomSource rngLow(7);

    int trials = 20000;
    int escalateCountLow = 0;
    for (int i = 0; i < trials; ++i) {
        auto decisions = lowDanger.Evaluate(0, "calm", rngLow);
        REQUIRE(decisions.size() == 1);
        if (decisions[0].targetId == "intense") {
            ++escalateCountLow;
        }
    }
    // weight 1.0 (stayCalm) vs weight 1.0*0.1 (escalate) -> expected freq 0.1/(1+0.1) ~= 0.0909
    double freqLow = static_cast<double>(escalateCountLow) / trials;
    CHECK(freqLow == doctest::Approx(0.1 / 1.1).epsilon(0.05));

    params.Set("danger", 1.0f);
    VariationEngine highDanger({MakeWeightScaledChoiceRule()}, params);
    RandomSource rngHigh(7);
    int escalateCountHigh = 0;
    for (int i = 0; i < trials; ++i) {
        auto decisions = highDanger.Evaluate(0, "calm", rngHigh);
        REQUIRE(decisions.size() == 1);
        if (decisions[0].targetId == "intense") {
            ++escalateCountHigh;
        }
    }
    // weight 1.0 vs weight 1.0*9.0 -> expected freq 9/(1+9) = 0.9
    double freqHigh = static_cast<double>(escalateCountHigh) / trials;
    CHECK(freqHigh == doctest::Approx(9.0 / 10.0).epsilon(0.05));

    CHECK(freqHigh > freqLow);
}

TEST_CASE("VariationEngine weight scaling clamps the parameter outside [paramAtWeightMin, paramAtWeightMax]") {
    GameParameters params({GameParameterConfig{"danger", -5.0f}}); // far below paramAtWeightMin
    VariationEngine engine({MakeWeightScaledChoiceRule()}, params);
    RandomSource rng(3);

    int trials = 5000;
    int escalateCount = 0;
    for (int i = 0; i < trials; ++i) {
        if (engine.Evaluate(0, "calm", rng)[0].targetId == "intense") {
            ++escalateCount;
        }
    }
    // Clamped to paramAtWeightMin's multiplier (0.1), same as danger == 0.0 exactly ->
    // expected freq 0.1/(1+0.1) ~= 0.0909.
    double freq = static_cast<double>(escalateCount) / trials;
    CHECK(freq == doctest::Approx(0.1 / 1.1).epsilon(0.07));
}

TEST_CASE("VariationEngine weight scaling with an undeclared parameter reads as 0.0") {
    GameParameters params({}); // "danger" never declared -> Get() always returns 0.0
    VariationEngine engine({MakeWeightScaledChoiceRule()}, params);
    RandomSource rng(3);

    int trials = 5000;
    int escalateCount = 0;
    for (int i = 0; i < trials; ++i) {
        if (engine.Evaluate(0, "calm", rng)[0].targetId == "intense") {
            ++escalateCount;
        }
    }
    // Same as danger == 0.0 exactly -> expected freq 0.1/(1+0.1) ~= 0.0909.
    double freq = static_cast<double>(escalateCount) / trials;
    CHECK(freq == doctest::Approx(0.1 / 1.1).epsilon(0.07));
}

TEST_CASE("VariationEngine weight scaling with an empty weightParameter leaves weight unscaled") {
    VariationRuleConfig rule;
    rule.id = "unscaled";
    VariationOptionConfig option;
    option.type = VariationOptionType::SwapPattern;
    option.targetId = "pattern_b";
    option.weight = 0.3f;
    // weightParameter left empty -- weightMultiplierAtMin/Max must be ignored entirely.
    option.weightMultiplierAtMin = 0.0f;
    option.weightMultiplierAtMax = 0.0f;
    rule.options = {option};

    GameParameters params({});
    VariationEngine engine({rule}, params);
    RandomSource rng(1);

    auto decisions = engine.Evaluate(0, "pattern_a", rng);
    REQUIRE(decisions.size() == 1);
    CHECK(decisions[0].targetId == "pattern_b");
}
