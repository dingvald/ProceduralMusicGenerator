#include "doctest/doctest.h"
#include "engine/Oscillator.h"

using namespace pmg;

TEST_CASE("Oscillator output stays within [-1, 1] for all waveforms") {
    const uint32_t sampleRate = 48000;
    Waveform waveforms[] = {Waveform::Sine, Waveform::Saw, Waveform::Square, Waveform::Triangle};

    for (Waveform waveform : waveforms) {
        Oscillator osc;
        osc.SetSampleRate(sampleRate);
        osc.SetFrequency(440.0f);
        osc.SetWaveform(waveform);

        for (uint32_t i = 0; i < sampleRate; ++i) {
            float sample = osc.NextSample();
            CHECK(sample >= -1.0001f);
            CHECK(sample <= 1.0001f);
        }
    }
}

TEST_CASE("Oscillator sine zero-crossing count matches expected period") {
    const uint32_t sampleRate = 48000;
    const float frequency = 100.0f; // 100 Hz -> ~200 zero crossings/sec

    Oscillator osc;
    osc.SetSampleRate(sampleRate);
    osc.SetFrequency(frequency);
    osc.SetWaveform(Waveform::Sine);

    int zeroCrossings = 0;
    float previous = osc.NextSample();
    for (uint32_t i = 1; i < sampleRate; ++i) {
        float current = osc.NextSample();
        if ((previous < 0.0f && current >= 0.0f) || (previous >= 0.0f && current < 0.0f)) {
            ++zeroCrossings;
        }
        previous = current;
    }

    CHECK(zeroCrossings == doctest::Approx(static_cast<int>(frequency) * 2).epsilon(0.05));
}

TEST_CASE("Oscillator Square duty cycle controls the fraction of the period spent high") {
    const uint32_t sampleRate = 48000;
    const float frequency = 100.0f; // low relative to sampleRate so PolyBLEP edge smoothing is negligible
    const float dutyCycles[] = {0.125f, 0.25f, 0.5f, 0.75f};

    for (float duty : dutyCycles) {
        Oscillator osc;
        osc.SetSampleRate(sampleRate);
        osc.SetFrequency(frequency);
        osc.SetWaveform(Waveform::Square);
        osc.SetDutyCycle(duty);

        int highSamples = 0;
        for (uint32_t i = 0; i < sampleRate; ++i) {
            if (osc.NextSample() > 0.0f) {
                ++highSamples;
            }
        }

        double fractionHigh = static_cast<double>(highSamples) / sampleRate;
        CHECK(fractionHigh == doctest::Approx(duty).epsilon(0.03));
    }
}

TEST_CASE("Oscillator Square duty cycle is clamped to a safe range and stays bounded") {
    const uint32_t sampleRate = 48000;
    const float requestedDuties[] = {-1.0f, 0.0f, 1.0f, 2.0f};

    for (float requestedDuty : requestedDuties) {
        Oscillator osc;
        osc.SetSampleRate(sampleRate);
        osc.SetFrequency(440.0f);
        osc.SetWaveform(Waveform::Square);
        osc.SetDutyCycle(requestedDuty);

        for (uint32_t i = 0; i < sampleRate; ++i) {
            float sample = osc.NextSample();
            CHECK(sample >= -1.0001f);
            CHECK(sample <= 1.0001f);
        }
    }
}

TEST_CASE("Oscillator Triangle ignores duty cycle and stays symmetric") {
    const uint32_t sampleRate = 48000;
    const float frequency = 100.0f;

    Oscillator narrowDuty;
    narrowDuty.SetSampleRate(sampleRate);
    narrowDuty.SetFrequency(frequency);
    narrowDuty.SetWaveform(Waveform::Triangle);
    narrowDuty.SetDutyCycle(0.125f);

    Oscillator defaultDuty;
    defaultDuty.SetSampleRate(sampleRate);
    defaultDuty.SetFrequency(frequency);
    defaultDuty.SetWaveform(Waveform::Triangle);

    for (uint32_t i = 0; i < sampleRate; ++i) {
        CHECK(narrowDuty.NextSample() == doctest::Approx(defaultDuty.NextSample()).epsilon(0.001));
    }
}
