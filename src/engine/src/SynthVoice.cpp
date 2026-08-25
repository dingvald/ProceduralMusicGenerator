#include "engine/SynthVoice.h"

namespace pmg {

void SynthVoice::Configure(uint32_t sampleRate, Waveform waveform, float dutyCycle, const ADSRParams& envelopeParams) {
    m_oscillator.SetSampleRate(sampleRate);
    m_oscillator.SetWaveform(waveform);
    m_oscillator.SetDutyCycle(dutyCycle);
    m_envelope.Configure(envelopeParams, sampleRate);
}

void SynthVoice::NoteOn(float frequencyHz, float velocity) {
    m_oscillator.SetFrequency(frequencyHz);
    m_oscillator.Reset();
    m_envelope.NoteOn();
    m_velocity = velocity;
    m_hasNote = true;
}

void SynthVoice::NoteOff() {
    m_envelope.NoteOff();
}

float SynthVoice::RenderSample() {
    if (!m_hasNote) {
        return 0.0f;
    }

    float sample = m_oscillator.NextSample() * m_envelope.NextSample() * m_velocity;
    if (m_envelope.IsFinished()) {
        m_hasNote = false;
    }
    return sample;
}

bool SynthVoice::IsActive() const {
    return m_hasNote;
}

} // namespace pmg
