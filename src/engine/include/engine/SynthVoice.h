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

    // gateDurationSamples, if >= 0, auto-releases the note (equivalent to
    // calling NoteOff) once that many samples have been rendered; -1 (the
    // default) holds the note until an explicit NoteOff instead.
    void NoteOn(float frequencyHz, float velocity, int gateDurationSamples = -1);
    void NoteOff();

    float RenderSample();
    bool IsActive() const;

private:
    Oscillator m_oscillator;
    Envelope m_envelope;
    float m_velocity = 1.0f;
    bool m_hasNote = false;
    int m_samplesUntilRelease = -1; // < 0: no scheduled auto-release
};

} // namespace pmg
