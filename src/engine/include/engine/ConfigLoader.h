#pragma once

#include <string>

#include "engine/CompositionConfig.h"

namespace pmg {

// Parses composition JSON into a CompositionConfig. Throws
// std::runtime_error with a clear message on missing/malformed required
// fields (deliberately fails loud rather than silently defaulting) and lets
// nlohmann::json::parse_error propagate on malformed JSON syntax.
class ConfigLoader {
public:
    static CompositionConfig LoadFromFile(const std::string& path);
    static CompositionConfig LoadFromString(const std::string& jsonText);
};

} // namespace pmg
