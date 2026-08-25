#include <string>

#include "doctest/doctest.h"
#include "engine/MarkovChainVariationStrategy.h"

using namespace pmg;

namespace {

MarkovChainConfig MakeChain() {
    MarkovChainConfig chain;
    chain["a"] = {{"a", 0.2f}, {"b", 0.8f}};
    chain["b"] = {{"a", 0.9f}, {"b", 0.1f}};
    return chain;
}

} // namespace

TEST_CASE("MarkovChainVariationStrategy is deterministic for a fixed seed") {
    MarkovChainVariationStrategy strategy(MakeChain());
    VariationContext context;
    context.barIndex = 0;
    context.currentPatternId = "a";

    RandomSource rng1(99);
    RandomSource rng2(99);

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

TEST_CASE("MarkovChainVariationStrategy transition frequency tracks configured weights") {
    MarkovChainVariationStrategy strategy(MakeChain());
    VariationContext context;
    context.barIndex = 0;
    context.currentPatternId = "a";

    RandomSource rng(555);
    const int trials = 20000;
    int stayed = 0; // no decision means a self-transition (stayed on 'a')
    int movedToB = 0;

    for (int i = 0; i < trials; ++i) {
        auto decisions = strategy.Decide(context, rng);
        if (decisions.empty()) {
            ++stayed;
        } else {
            REQUIRE(decisions.size() == 1);
            CHECK(decisions[0].type == VariationDecision::Type::SwapPattern);
            CHECK(decisions[0].targetId == "b");
            ++movedToB;
        }
    }

    REQUIRE(stayed + movedToB == trials);
    double freqMovedToB = static_cast<double>(movedToB) / trials;
    CHECK(freqMovedToB == doctest::Approx(0.8).epsilon(0.05));
}

TEST_CASE("MarkovChainVariationStrategy returns no decision for a state with no configured transitions") {
    MarkovChainVariationStrategy strategy(MakeChain());
    VariationContext context;
    context.barIndex = 0;
    context.currentPatternId = "unknown_pattern";

    RandomSource rng(1);
    auto decisions = strategy.Decide(context, rng);
    CHECK(decisions.empty());
}
