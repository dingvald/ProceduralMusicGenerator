#include <cmath>
#include <vector>

#include "doctest/doctest.h"
#include "engine/Oscillator.h"

using namespace pmg;

TEST_CASE("Oscillator output stays within [-1, 1] for all waveforms") {
    const uint32_t sampleRate = 48000;
    Waveform waveforms[] = {Waveform::Sine, Waveform::Saw, Waveform::Square, Waveform::Triangle, Waveform::Noise};

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

TEST_CASE("Oscillator Noise output is always exactly +1 or -1") {
    const uint32_t sampleRate = 48000;
    Oscillator osc;
    osc.SetSampleRate(sampleRate);
    osc.SetFrequency(1000.0f);
    osc.SetWaveform(Waveform::Noise);

    for (uint32_t i = 0; i < sampleRate; ++i) {
        float sample = osc.NextSample();
        CHECK((sample == doctest::Approx(1.0f) || sample == doctest::Approx(-1.0f)));
    }
}

TEST_CASE("Oscillator Noise output is not degenerate: it visits both +1 and -1") {
    const uint32_t sampleRate = 48000;
    Oscillator osc;
    osc.SetSampleRate(sampleRate);
    osc.SetFrequency(1000.0f);
    osc.SetWaveform(Waveform::Noise);

    bool sawPositive = false;
    bool sawNegative = false;
    for (uint32_t i = 0; i < sampleRate; ++i) {
        float sample = osc.NextSample();
        if (sample > 0.0f) sawPositive = true;
        if (sample < 0.0f) sawNegative = true;
    }
    CHECK(sawPositive);
    CHECK(sawNegative);
}

TEST_CASE("Oscillator Noise clocks the LFSR roughly once per period at the configured frequency") {
    const uint32_t sampleRate = 48000;
    const float frequency = 1000.0f; // -> up to ~1000 LFSR shifts/sec

    Oscillator osc;
    osc.SetSampleRate(sampleRate);
    osc.SetFrequency(frequency);
    osc.SetWaveform(Waveform::Noise);

    int changes = 0;
    float previous = osc.NextSample();
    for (uint32_t i = 1; i < sampleRate; ++i) {
        float current = osc.NextSample();
        if (current != previous) {
            ++changes;
        }
        previous = current;
    }

    // A shifted LFSR bit can coincidentally repeat the previous value, so
    // only roughly half of ~1000 shifts/sec are expected to register as a
    // visible change; this is a loose sanity bound, not an exact count.
    CHECK(changes > 0);
    CHECK(changes < static_cast<int>(frequency));
}

TEST_CASE("Oscillator Sine NextSample(phaseModulation) matches the closed-form phase-shifted sine") {
    const uint32_t sampleRate = 48000;
    const double kPi = 3.14159265358979323846;
    const double phaseModulations[] = {0.0, 0.1, 0.25, 0.5, -0.3, 1.75};

    for (double pm : phaseModulations) {
        Oscillator osc;
        osc.SetSampleRate(sampleRate);
        osc.SetFrequency(440.0f);
        osc.SetWaveform(Waveform::Sine);

        // Freshly constructed, so m_phase == 0 for this first call.
        float sample = osc.NextSample(pm);
        float expected = static_cast<float>(std::sin(2.0 * kPi * pm));
        CHECK(sample == doctest::Approx(expected).epsilon(0.0001));
    }
}

TEST_CASE("Oscillator non-Sine waveforms ignore phaseModulation") {
    const uint32_t sampleRate = 48000;
    Waveform waveforms[] = {Waveform::Saw, Waveform::Square, Waveform::Triangle, Waveform::Noise};

    for (Waveform waveform : waveforms) {
        Oscillator unmodulated;
        unmodulated.SetSampleRate(sampleRate);
        unmodulated.SetFrequency(220.0f);
        unmodulated.SetWaveform(waveform);

        Oscillator modulated;
        modulated.SetSampleRate(sampleRate);
        modulated.SetFrequency(220.0f);
        modulated.SetWaveform(waveform);

        for (uint32_t i = 0; i < 2000; ++i) {
            CHECK(modulated.NextSample(0.4) == doctest::Approx(unmodulated.NextSample(0.0)));
        }
    }
}

TEST_CASE("Oscillator Noise LFSR free-runs and is not reseeded by Reset()") {
    const uint32_t sampleRate = 48000;
    const float shiftEveryFrequency = static_cast<float>(sampleRate); // phaseIncrement == 1 -> one LFSR shift per sample

    Oscillator fresh;
    fresh.SetSampleRate(sampleRate);
    fresh.SetFrequency(shiftEveryFrequency);
    fresh.SetWaveform(Waveform::Noise);
    std::vector<float> freshSequence;
    for (int i = 0; i < 50; ++i) {
        freshSequence.push_back(fresh.NextSample());
    }

    // Advance a second oscillator's LFSR well past the fresh oscillator's
    // state, then Reset() it (simulating a note retrigger) and capture the
    // next samples. If Reset() reseeded the LFSR, this would exactly match
    // freshSequence; it doesn't, because Reset() leaves the LFSR alone.
    Oscillator used;
    used.SetSampleRate(sampleRate);
    used.SetFrequency(shiftEveryFrequency);
    used.SetWaveform(Waveform::Noise);
    for (int i = 0; i < 5000; ++i) {
        used.NextSample();
    }
    used.Reset();
    std::vector<float> afterResetSequence;
    for (int i = 0; i < 50; ++i) {
        afterResetSequence.push_back(used.NextSample());
    }

    bool identical = true;
    for (size_t i = 0; i < freshSequence.size(); ++i) {
        if (freshSequence[i] != afterResetSequence[i]) {
            identical = false;
            break;
        }
    }
    CHECK_FALSE(identical);
}
