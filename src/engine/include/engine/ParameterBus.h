#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <string>

namespace pmg {

// Fixed-capacity id string carried inside audio-thread Commands. Instrument
// and track ids in this project come from the JSON config and are short
// (a handful of characters), so converting a FixedId back to std::string on
// the audio thread stays allocation-free in practice via small-string
// optimization on libstdc++/libc++/MSVC STL. This is a practical, not a
// hard, real-time guarantee — documented v1 limitation.
struct FixedId {
    static constexpr size_t kCapacity = 32;
    char data[kCapacity] = {};

    FixedId() = default;
    explicit FixedId(const std::string& s) {
        size_t n = s.size() < kCapacity - 1 ? s.size() : kCapacity - 1;
        std::memcpy(data, s.data(), n);
        data[n] = '\0';
    }

    std::string ToString() const { return std::string(data); }
};

enum class CommandType {
    NoteOn,
    NoteOff,
    TriggerSample,
    SetTrackMuted,
    SetTrackGain,
};

struct Command {
    CommandType type = CommandType::NoteOn;
    FixedId id;               // instrument id (NoteOn/TriggerSample) or track id (mute/gain)
    float floatValue = 0.0f;  // frequency (NoteOn), gain (TriggerSample/SetTrackGain)
    float floatValue2 = 0.0f; // velocity (NoteOn)
    int intValue = 0;         // voice handle (NoteOff); gate duration in samples (NoteOn)
    bool boolValue = false;   // muted flag (SetTrackMuted)
};

// Fixed-capacity single-producer/single-consumer lock-free ring buffer.
// The control thread (Sequencer / main loop) is the sole producer; the
// audio device callback thread is the sole consumer, draining fully at the
// top of every callback before rendering. No locks, no allocation, bounded
// capacity. Overflow policy: Push() returns false and the command is
// dropped (drop-newest) if the bus is full.
class ParameterBus {
public:
    static constexpr size_t kCapacity = 256;

    bool Push(const Command& command) {
        size_t head = m_head.load(std::memory_order_relaxed);
        size_t nextHead = (head + 1) % kCapacity;
        if (nextHead == m_tail.load(std::memory_order_acquire)) {
            return false; // full
        }
        m_buffer[head] = command;
        m_head.store(nextHead, std::memory_order_release);
        return true;
    }

    bool Pop(Command& outCommand) {
        size_t tail = m_tail.load(std::memory_order_relaxed);
        if (tail == m_head.load(std::memory_order_acquire)) {
            return false; // empty
        }
        outCommand = m_buffer[tail];
        m_tail.store((tail + 1) % kCapacity, std::memory_order_release);
        return true;
    }

private:
    std::array<Command, kCapacity> m_buffer;
    std::atomic<size_t> m_head{0};
    std::atomic<size_t> m_tail{0};
};

} // namespace pmg
