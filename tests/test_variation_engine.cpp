#include <map>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "engine/RuleBasedVariationStrategy.h"

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
