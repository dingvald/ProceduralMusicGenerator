#include "engine/SynthVoice.h"

namespace pmg {

void SynthVoice::Configure(uint32_t sampleRate, Waveform waveform, float dutyCycle, const ArpeggioConfig& arpeggioConfig,
                            const ADSRParams& envelopeParams, const VibratoConfig& vibratoConfig,
                            const FmConfig& fmConfig) {
    m_oscillator.SetSampleRate(sampleRate);
    m_oscillator.SetWaveform(waveform);
    m_oscillator.SetDutyCycle(dutyCycle);
    m_arpeggiator.Configure(arpeggioConfig, sampleRate);
    m_envelope.Configure(envelopeParams, sampleRate);
    m_vibrato.Configure(vibratoConfig, sampleRate);
    m_fmModulator.SetSampleRate(sampleRate);
    m_fmModulator.SetWaveform(Waveform::Sine);
    m_fm = fmConfig;
}

void SynthVoice::NoteOn(float frequencyHz, float velocity, int gateDurationSamples) {
    m_baseFrequency = frequencyHz;
    m_oscillator.SetFrequency(frequencyHz);
    m_oscillator.Reset();
    m_arpeggiator.NoteOn();
    m_vibrato.NoteOn();
    m_fmModulator.Reset();
    m_envelope.NoteOn();
    m_velocity = velocity;
    m_hasNote = true;
    m_samplesUntilRelease = gateDurationSamples;
}

void SynthVoice::NoteOff() {
    m_envelope.NoteOff();
}

void SynthVoice::Reset() {
    m_hasNote = false;
    m_samplesUntilRelease = -1;
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

    float carrierFreq = m_baseFrequency * m_arpeggiator.NextMultiplier() * m_vibrato.NextMultiplier();
    m_oscillator.SetFrequency(carrierFreq);

    float phaseModulation = 0.0f;
    if (m_fm.amount > 0.0f) {
        m_fmModulator.SetFrequency(carrierFreq * m_fm.ratio);
        phaseModulation = m_fmModulator.NextSample() * m_fm.amount;
    }

    float sample = m_oscillator.NextSample(phaseModulation) * m_envelope.NextSample() * m_velocity;
    if (m_envelope.IsFinished()) {
        m_hasNote = false;
    }
    return sample;
}

bool SynthVoice::IsActive() const {
    return m_hasNote;
}

} // namespace pmg
