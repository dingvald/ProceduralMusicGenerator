#include "engine/SynthVoice.h"

namespace pmg {

void SynthVoice::Configure(uint32_t sampleRate, Waveform waveform, float dutyCycle, const ArpeggioConfig& arpeggioConfig,
                            const ADSRParams& envelopeParams) {
    m_oscillator.SetSampleRate(sampleRate);
    m_oscillator.SetWaveform(waveform);
    m_oscillator.SetDutyCycle(dutyCycle);
    m_arpeggiator.Configure(arpeggioConfig, sampleRate);
    m_envelope.Configure(envelopeParams, sampleRate);
}

void SynthVoice::NoteOn(float frequencyHz, float velocity, int gateDurationSamples) {
    m_baseFrequency = frequencyHz;
    m_oscillator.SetFrequency(frequencyHz);
    m_oscillator.Reset();
    m_arpeggiator.NoteOn();
    m_envelope.NoteOn();
    m_velocity = velocity;
    m_hasNote = true;
    m_samplesUntilRelease = gateDurationSamples;
}

void SynthVoice::NoteOff() {
    m_envelope.NoteOff();
}

float SynthVoice::RenderSample() {
    if (!m_hasNote) {
        return 0.0f;
    }

    if (m_samplesUntilRelease == 0) {
        m_envelope.NoteOff();
        --m_samplesUntilRelease; // step below zero so this doesn't refire every sample
    } else if (m_samplesUntilRelease > 0) {
        --m_samplesUntilRelease;
    }

    m_oscillator.SetFrequency(m_baseFrequency * m_arpeggiator.NextMultiplier());
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
