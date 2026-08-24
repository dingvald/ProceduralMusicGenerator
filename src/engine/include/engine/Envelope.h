#pragma once

#include <cstdint>

namespace pmg {

struct ADSRParams {
    float attackSec = 0.01f;
    float decaySec = 0.1f;
    float sustainLevel = 0.7f;
    float releaseSec = 0.2f;
};

// Linear-ramp ADSR envelope generator. Per-sample state machine, audio-thread
// safe (no allocation, no locking).
class Envelope {
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void Configure(const ADSRParams& params, uint32_t sampleRate);
    void NoteOn();
    void NoteOff();

    float NextSample();
    bool IsFinished() const;
    Stage GetStage() const { return m_stage; }

private:
    ADSRParams m_params;
    uint32_t m_sampleRate = 48000;
    Stage m_stage = Stage::Idle;
    float m_level = 0.0f;
    float m_attackRate = 0.0f;
    float m_decayRate = 0.0f;
    float m_releaseRate = 0.0f;
};

} // namespace pmg
