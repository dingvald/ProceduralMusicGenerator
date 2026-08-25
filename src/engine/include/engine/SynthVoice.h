#pragma once

#include <cstdint>

#include "engine/Envelope.h"
#include "engine/Oscillator.h"

namespace pmg {

// One synth voice: an oscillator gated by an ADSR envelope. Audio-thread
// safe; Mixer owns a fixed pool of these and reuses inactive slots.
class SynthVoice {
public:
    void Configure(uint32_t sampleRate, Waveform waveform, float dutyCycle, const ADSRParams& envelopeParams);

    void NoteOn(float frequencyHz, float velocity);
    void NoteOff();

    float RenderSample();
    bool IsActive() const;

private:
    Oscillator m_oscillator;
    Envelope m_envelope;
    float m_velocity = 1.0f;
    bool m_hasNote = false;
};

} // namespace pmg
