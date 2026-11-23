#include "audio_codec.h"
#include "board.h"
#include "settings.h"

#include <esp_log.h>
#include <cstring>
#include <driver/i2s_common.h>

#define TAG "AudioCodec"

AudioCodec::AudioCodec() {
}

AudioCodec::~AudioCodec() {
}

void AudioCodec::OutputData(std::vector<int16_t>& data) {
    Write(data.data(), data.size());
}

bool AudioCodec::InputData(std::vector<int16_t>& data) {
    int samples = Read(data.data(), data.size());
    if (samples > 0) {
        return true;
    }
    return false;
}

void AudioCodec::Start() {
    Settings settings("audio", false);
    output_volume_ = settings.GetInt("output_volume", output_volume_);
    if (output_volume_ <= 0) {
        // A persisted 0 comes from an earlier session's mute. Restore a
        // comfortable level instead of a near-silent 10, which is easily
        // mistaken for stutter/weak audio.
        ESP_LOGW(TAG, "Output volume value (%d) is too small, setting to default (70)", output_volume_);
        output_volume_ = 70;
    }

    ESP_LOGI(TAG, "Audio codec started");
}

void AudioCodec::SetOutputVolume(int volume) {
    SetOutputVolume(volume, true);
}

void AudioCodec::SetOutputVolume(int volume, bool persist) {
    output_volume_ = volume;
    ESP_LOGI(TAG, "Set output volume to %d (persist=%d)", output_volume_, persist ? 1 : 0);
    if (!persist) {
        return;
    }
    Settings settings("audio", true);
    settings.SetInt("output_volume", output_volume_);
}

void AudioCodec::SetInputGain(float gain) {
    input_gain_ = gain;
    ESP_LOGI(TAG, "Set input gain to %.1f", input_gain_);
}

void AudioCodec::EnableInput(bool enable) {
    if (enable == input_enabled_) {
        return;
    }
    input_enabled_ = enable;
    ESP_LOGI(TAG, "Set input enable to %s", enable ? "true" : "false");
}

void AudioCodec::EnableOutput(bool enable) {
    if (enable == output_enabled_) {
        return;
    }
    output_enabled_ = enable;
    ESP_LOGI(TAG, "Set output enable to %s", enable ? "true" : "false");
}
