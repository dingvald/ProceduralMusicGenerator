#include "engine/AudioEngine.h"

#include "miniaudio/miniaudio.h"

namespace pmg {

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() {
    Shutdown();
}

bool AudioEngine::Initialize(const AudioEngineConfig& config) {
    if (m_initialized) {
        return true;
    }

    m_config = config;
    m_mixer.Configure(m_config.sampleRate);
    m_delayProcessor.Configure(m_config.delay, m_config.sampleRate);
    m_loFiProcessor.Configure(m_config.loFi);

    auto context = std::make_unique<ma_context>();
    ma_context_config contextConfig = ma_context_config_init();

    ma_result contextResult;
    if (m_config.useNullBackend) {
        ma_backend backends[] = {ma_backend_null};
        contextResult = ma_context_init(backends, 1, &contextConfig, context.get());
    } else {
        contextResult = ma_context_init(nullptr, 0, &contextConfig, context.get());
    }
    if (contextResult != MA_SUCCESS) {
        return false;
    }

    auto device = std::make_unique<ma_device>();
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = ma_format_f32;
    deviceConfig.playback.channels = m_config.channels;
    deviceConfig.sampleRate = m_config.sampleRate;
    deviceConfig.dataCallback = &AudioEngine::DataCallback;
    deviceConfig.pUserData = this;

    ma_result deviceResult = ma_device_init(context.get(), &deviceConfig, device.get());
    if (deviceResult != MA_SUCCESS) {
        ma_context_uninit(context.get());
        return false;
    }

    m_context = std::move(context);
    m_device = std::move(device);
    m_initialized = true;
    return true;
}

bool AudioEngine::InitializeOffline(const AudioEngineConfig& config) {
    if (m_initialized) {
        return true;
    }

    m_config = config;
    m_mixer.Configure(m_config.sampleRate);
    m_delayProcessor.Configure(m_config.delay, m_config.sampleRate);
    m_loFiProcessor.Configure(m_config.loFi);

    m_initialized = true;
    return true;
}

void AudioEngine::Start() {
    if (m_initialized && m_device) {
        ma_device_start(m_device.get());
    }
}

void AudioEngine::Stop() {
    if (m_initialized && m_device) {
        ma_device_stop(m_device.get());
    }
}

void AudioEngine::Shutdown() {
    if (m_device) {
        ma_device_uninit(m_device.get());
        m_device.reset();
    }
    if (m_context) {
        ma_context_uninit(m_context.get());
        m_context.reset();
    }
    m_initialized = false;
}

void AudioEngine::DataCallback(ma_device* device, void* output, const void* /*input*/, uint32_t frameCount) {
    auto* self = static_cast<AudioEngine*>(device->pUserData);
    if (self) {
        self->RenderFrames(static_cast<float*>(output), frameCount);
    }
}

void AudioEngine::RenderFrames(float* output, uint32_t frameCount) {
    Command command;
    while (m_parameterBus.Pop(command)) {
        switch (command.type) {
            case CommandType::NoteOn:
                m_mixer.NoteOn(command.id.ToString(), command.floatValue, command.floatValue2, command.intValue);
                break;
            case CommandType::NoteOff:
                m_mixer.NoteOff(command.intValue);
                break;
            case CommandType::TriggerSample:
                m_mixer.TriggerSample(command.id.ToString(), command.floatValue);
                break;
            case CommandType::SetTrackMuted:
                m_mixer.SetTrackMuted(command.id.ToString(), command.boolValue);
                break;
            case CommandType::SetTrackGain:
                m_mixer.SetTrackGain(command.id.ToString(), command.floatValue);
                break;
        }
    }

    uint32_t channels = m_config.channels;
    for (uint32_t frame = 0; frame < frameCount; ++frame) {
        float sample = m_loFiProcessor.Process(m_delayProcessor.Process(m_mixer.RenderNextSample()));
        for (uint32_t ch = 0; ch < channels; ++ch) {
            output[frame * channels + ch] = sample;
        }
    }

    m_framesProcessed.fetch_add(frameCount, std::memory_order_relaxed);
}

} // namespace pmg
