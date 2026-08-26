#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "engine/SampleLoader.h"

using namespace pmg;

namespace {

// Writes a minimal canonical 44-byte-header PCM16 WAV file. interleaved
// holds one int16 per channel per frame (interleaved if channels > 1).
void WriteTestWav(const std::string& path, const std::vector<int16_t>& interleaved, uint16_t channels,
                   uint32_t sampleRate) {
    uint32_t dataSize = static_cast<uint32_t>(interleaved.size() * sizeof(int16_t));
    uint16_t blockAlign = static_cast<uint16_t>(channels * sizeof(int16_t));
    uint32_t byteRate = sampleRate * blockAlign;
    uint32_t chunkSize = 36 + dataSize;
    uint16_t bitsPerSample = 16;
    uint16_t audioFormat = 1; // PCM
    uint32_t subchunk1Size = 16;

    std::ofstream file(path, std::ios::binary);
    REQUIRE(file.is_open());

    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&chunkSize), 4);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    file.write(reinterpret_cast<const char*>(&subchunk1Size), 4);
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    file.write(reinterpret_cast<const char*>(&channels), 2);
    file.write(reinterpret_cast<const char*>(&sampleRate), 4);
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataSize), 4);
    file.write(reinterpret_cast<const char*>(interleaved.data()), dataSize);
}

// One temp path per test case (doctest test case names aren't valid
// filenames verbatim, so a fixed counter keeps this simple and collision-free
// within a single test run).
std::string TempWavPath() {
    static int counter = 0;
    return (std::filesystem::temp_directory_path() / ("pmg_test_sample_loader_" + std::to_string(counter++) + ".wav"))
        .string();
}

} // namespace

TEST_CASE("LoadSampleAsset decodes a mono PCM16 WAV at the target sample rate") {
    std::string path = TempWavPath();
    // 0, mid-positive, mid-negative, max-positive, max-negative
    WriteTestWav(path, {0, 16384, -16384, 32767, -32768}, 1, 44100);

    auto asset = LoadSampleAsset(path, 44100);
    REQUIRE(asset != nullptr);
    CHECK(asset->channels == 1);
    CHECK(asset->sampleRate == 44100);
    REQUIRE(asset->interleavedPCM.size() == 5);
    CHECK(asset->interleavedPCM[0] == doctest::Approx(0.0f).epsilon(0.01));
    CHECK(asset->interleavedPCM[1] == doctest::Approx(0.5f).epsilon(0.01));
    CHECK(asset->interleavedPCM[2] == doctest::Approx(-0.5f).epsilon(0.01));
    CHECK(asset->interleavedPCM[3] == doctest::Approx(1.0f).epsilon(0.01));
    CHECK(asset->interleavedPCM[4] == doctest::Approx(-1.0f).epsilon(0.01));

    std::filesystem::remove(path);
}

TEST_CASE("LoadSampleAsset downmixes a stereo WAV to mono") {
    std::string path = TempWavPath();
    // Frame 0: L=+full, R=-full -> should average to ~0.
    // Frame 1: L=+half, R=+half -> should average to ~0.5.
    WriteTestWav(path, {32767, -32768, 16384, 16384}, 2, 44100);

    auto asset = LoadSampleAsset(path, 44100);
    CHECK(asset->channels == 1);
    REQUIRE(asset->interleavedPCM.size() == 2);
    CHECK(asset->interleavedPCM[0] == doctest::Approx(0.0f).epsilon(0.02));
    CHECK(asset->interleavedPCM[1] == doctest::Approx(0.5f).epsilon(0.02));

    std::filesystem::remove(path);
}

TEST_CASE("LoadSampleAsset resamples to the requested target sample rate") {
    std::string path = TempWavPath();
    std::vector<int16_t> samples(441, 16384); // 10ms of constant-ish signal at 44100Hz
    WriteTestWav(path, samples, 1, 44100);

    auto asset = LoadSampleAsset(path, 22050); // half the source rate
    CHECK(asset->sampleRate == 22050);
    // Resampling isn't exact (filter delay/padding), so just check the frame
    // count landed in the right ballpark rather than hardcoding an exact value.
    CHECK(asset->interleavedPCM.size() > 150);
    CHECK(asset->interleavedPCM.size() < 300);

    std::filesystem::remove(path);
}

TEST_CASE("LoadSampleAsset throws when the file doesn't exist") {
    CHECK_THROWS_AS(LoadSampleAsset("/nonexistent/path/does_not_exist.wav", 44100), std::runtime_error);
}

TEST_CASE("LoadSampleAsset throws when the file isn't a decodable audio format") {
    std::string path = TempWavPath();
    {
        std::ofstream file(path, std::ios::binary);
        REQUIRE(file.is_open());
        file << "this is not a wav file";
    }

    CHECK_THROWS_AS(LoadSampleAsset(path, 44100), std::runtime_error);

    std::filesystem::remove(path);
}
