#pragma once

#include <cstdint>

#include "engine/Arpeggiator.h"
#include "engine/Envelope.h"
#include "engine/FmConfig.h"
#include "engine/Oscillator.h"
#include "engine/Vibrato.h"

namespace pmg {

// One synth voice: an oscillator gated by an ADSR envelope, with an
// optional Arpeggiator and Vibrato modulating its frequency while held, and
// an optional second Oscillator phase-modulating the carrier for 2-op FM
// (only audible when the carrier waveform is Sine -- see
// Oscillator::NextSample). Audio-thread safe; Mixer owns a fixed pool of
// these and reuses inactive slots.
class SynthVoice {
public:
    void Configure(uint32_t sampleRate, Waveform waveform, float dutyCycle, const ArpeggioConfig& arpeggioConfig,
                   const ADSRParams& envelopeParams, const VibratoConfig& vibratoConfig, const FmConfig& fmConfig);

    // gateDurationSamples, if >= 0, auto-releases the note (equivalent to
    // calling NoteOff) once that many samples have been rendered; -1 (the
    // default) holds the note until an explicit NoteOff instead.
    void NoteOn(float frequencyHz, float velocity, int gateDurationSamples = -1);
    void NoteOff();

    // Forcibly deactivates the voice outside the normal envelope-release
    // path, e.g. when the Mixer is being reconfigured for a different
    // composition (hot reload / track switch) and stale voices need to fall
    // silent immediately rather than ring out. The next Configure()+NoteOn()
    // starts the voice cleanly regardless of this call.
    void Reset();

    float RenderSample();
    bool IsActive() const;

private:
    Oscillator m_oscillator;
    Envelope m_envelope;
    Arpeggiator m_arpeggiator;
    Vibrato m_vibrato;
    Oscillator m_fmModulator;
    FmConfig m_fm;
    float m_baseFrequency = 440.0f;
    float m_velocity = 1.0f;
    bool m_hasNote = false;
    int m_samplesUntilRelease = -1; // < 0: no scheduled auto-release
};

} // namespace pmg
