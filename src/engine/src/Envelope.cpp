#include "engine/Envelope.h"

namespace pmg {

void Envelope::Configure(const ADSRParams& params, uint32_t sampleRate) {
    m_params = params;
    m_sampleRate = sampleRate > 0 ? sampleRate : 48000;

    m_attackRate = m_params.attackSec > 0.0f
        ? 1.0f / (m_params.attackSec * static_cast<float>(m_sampleRate))
        : 1.0f;

    m_decayRate = m_params.decaySec > 0.0f
        ? (1.0f - m_params.sustainLevel) / (m_params.decaySec * static_cast<float>(m_sampleRate))
        : (1.0f - m_params.sustainLevel);
}

void Envelope::NoteOn() {
    m_stage = Stage::Attack;
    m_level = 0.0f;
}

void Envelope::NoteOff() {
    if (m_stage == Stage::Idle) {
        return;
    }
    m_stage = Stage::Release;
    m_releaseRate = m_params.releaseSec > 0.0f
        ? m_level / (m_params.releaseSec * static_cast<float>(m_sampleRate))
        : m_level;
}

float Envelope::NextSample() {
    switch (m_stage) {
        case Stage::Idle:
            return 0.0f;

        case Stage::Attack:
            m_level += m_attackRate;
            if (m_level >= 1.0f) {
                m_level = 1.0f;
                m_stage = Stage::Decay;
            }
            break;

        case Stage::Decay:
            m_level -= m_decayRate;
            if (m_level <= m_params.sustainLevel) {
                m_level = m_params.sustainLevel;
                // A zero sustain level means "one-shot decay, no held tail"
                // (the shape a percussive/noise instrument wants). Holding
                // in Stage::Sustain at silence would be observationally the
                // same as finishing, but SynthVoice/Mixer only ever free a
                // voice's pool slot once IsFinished() is true, and nothing
                // else in the engine sends NoteOff for a one-shot hit -- so
                // without this, the voice would sit "active" forever.
                m_stage = m_params.sustainLevel <= 0.0f ? Stage::Idle : Stage::Sustain;
            }
            break;

        case Stage::Sustain:
            m_level = m_params.sustainLevel;
            break;

        case Stage::Release:
            m_level -= m_releaseRate;
            if (m_level <= 0.0f) {
                m_level = 0.0f;
                m_stage = Stage::Idle;
            }
            break;
    }

    return m_level;
}

bool Envelope::IsFinished() const {
    return m_stage == Stage::Idle;
}

} // namespace pmg
