#include <vector>

#include "doctest/doctest.h"
#include "engine/GameParameters.h"

using namespace pmg;

namespace {

std::vector<GameParameterConfig> MakeDeclared() {
    GameParameterConfig danger;
    danger.name = "danger";
    danger.defaultValue = 0.25f;

    GameParameterConfig intensity;
    intensity.name = "intensity";
    intensity.defaultValue = 0.0f;

    return {danger, intensity};
}

} // namespace

TEST_CASE("GameParameters initializes declared parameters to their default values") {
    GameParameters params(MakeDeclared());
    CHECK(params.Get("danger") == doctest::Approx(0.25f));
    CHECK(params.Get("intensity") == doctest::Approx(0.0f));
}

TEST_CASE("GameParameters Set/Get round-trips a declared parameter") {
    GameParameters params(MakeDeclared());
    params.Set("danger", 0.9f);
    CHECK(params.Get("danger") == doctest::Approx(0.9f));

    params.Set("danger", -1.0f); // no clamping -- caller's responsibility
    CHECK(params.Get("danger") == doctest::Approx(-1.0f));
}

TEST_CASE("GameParameters Set on an undeclared name is a silent no-op") {
    GameParameters params(MakeDeclared());
    params.Set("does_not_exist", 5.0f);
    CHECK(params.Get("does_not_exist") == doctest::Approx(0.0f));
}

TEST_CASE("GameParameters Get on an undeclared name returns 0.0") {
    GameParameters params(MakeDeclared());
    CHECK(params.Get("nope") == doctest::Approx(0.0f));
}

TEST_CASE("GameParameters Has reports declared vs. undeclared names") {
    GameParameters params(MakeDeclared());
    CHECK(params.Has("danger"));
    CHECK(params.Has("intensity"));
    CHECK_FALSE(params.Has("nope"));
}

TEST_CASE("GameParameters with no declared parameters is inert") {
    GameParameters params({});
    CHECK_FALSE(params.Has("anything"));
    params.Set("anything", 1.0f); // must not crash/throw
    CHECK(params.Get("anything") == doctest::Approx(0.0f));
}
