#pragma once

#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/CompositionConfig.h"

namespace pmg {

// Thread-safe map<string, float> of runtime parameters a host application
// (e.g. a game) sets live to steer procedural variation and instrument gain
// -- e.g. Set("danger", 0.8f) from the game's own thread, read every bar by
// VariationRuleConfig's gate (VariationEngine::Evaluate) and every control-
// thread tick by GainCrossfader (Sequencer::Update) on whatever thread(s)
// those run on. Composition JSON refers to parameters by the same string
// name (see GameParameterConfig, VariationRuleConfig::gateParameter,
// GainCrossfadeConfig::parameter).
//
// The declared parameter set (GameParameterConfig list, normally a
// composition's top-level "gameParameters") is fixed at construction time,
// which is what makes Set/Get lock-free after that: the map itself is built
// once, single-threaded, before any concurrent access can happen, and never
// gains or loses keys afterward -- only the std::atomic<float> values inside
// change, which needs no external synchronization. Set on an undeclared name
// is a silent no-op (matching Mixer::NoteOn/TriggerSample's existing "drop
// unknown ids rather than throw" convention, since this is a hot game-loop
// call); Get on an undeclared name returns 0.0f.
class GameParameters {
public:
    explicit GameParameters(const std::vector<GameParameterConfig>& declared) {
        for (const GameParameterConfig& config : declared) {
            m_values.emplace(config.name, config.defaultValue);
        }
    }

    void Set(const std::string& name, float value) {
        auto it = m_values.find(name);
        if (it != m_values.end()) {
            it->second.store(value, std::memory_order_relaxed);
        }
    }

    float Get(const std::string& name) const {
        auto it = m_values.find(name);
        return it != m_values.end() ? it->second.load(std::memory_order_relaxed) : 0.0f;
    }

    bool Has(const std::string& name) const { return m_values.count(name) > 0; }

private:
    std::unordered_map<std::string, std::atomic<float>> m_values;
};

} // namespace pmg
