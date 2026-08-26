#pragma once

namespace pmg {

// 2-operator phase-modulation FM, modeled on how Yamaha OPN/OPL chips (e.g.
// the YM2612 in the Sega Genesis) built FM voices from sine operators. Only
// audible when the carrier's Waveform is Sine -- see
// Oscillator::NextSample's phaseModulation parameter. amount == 0 (the
// default) disables FM entirely.
struct FmConfig {
    float ratio = 1.0f;  // modulator frequency = carrier frequency * ratio
    float amount = 0.0f; // peak phase deviation in cycles (turns); 0 = disabled
};

} // namespace pmg
