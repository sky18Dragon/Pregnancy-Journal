#include "sticky_voice_classifier_test.h"

#if defined(STICKY_VOICE_CLASSIFIER_TEST) && STICKY_VOICE_CLASSIFIER_TEST

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstring>

#include "board_power.h"
#include "canvas.h"
#include "driver/gpio.h"
#include "driver/i2s_pdm.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "model_path.h"
#include "pin_config.h"
#include "sticky_button.h"
#include "sticky_display.h"
#include "voice_intent.h"

namespace {

constexpr char kTag[] = "voice_classifier_test";
constexpr int kModelDurationMs = 30000;
constexpr float kModelDetectionThreshold = 0.30F;
constexpr int kTrailingSilenceMs = 600;

struct CommandPhrase {
    int command_id;
    const char *pinyin;
};

// MultiNet5 Chinese accepts space-separated pinyin syllables without tones.
// Multiple phrases intentionally share one stable product intent ID.
constexpr CommandPhrase kCommandPhrases[] = {
    {1, "ti zhong ji lu"},
    {1, "ji lu ti zhong"},
    {2, "chan jian ji lu"},
    {2, "ji lu chan jian"},
    {3, "xi guan ji lu"},
    {3, "ji lu xi guan"},
    {4, "bei wang ji lu"},
    {4, "ji lu bei wang"},
};

struct Runtime {
    srmodel_list_t *models = nullptr;
    esp_mn_iface_t *multinet = nullptr;
    model_iface_data_t *model_data = nullptr;
    i2s_chan_handle_t microphone = nullptr;
    int samples_per_chunk = 0;
    bool microphone_enabled = false;
    std::array<float, 4> best_probability{};
};

Runtime s_runtime;

void show_status(const char *status, const char *detail)
{
    Canvas *canvas = sticky_display_canvas();
    canvas->clear();
    canvas->draw_text(32, 38, "S0 VOICE CLASSIFIER", 3);
    canvas->draw_text(32, 118, status, 2);
    canvas->draw_text(32, 185, detail, 1);
    canvas->draw_text(32, 255, "Hold TOP 0.6s, speak, then release.", 1);
    canvas->draw_text(32, 300, "No SD write. No network. No audio saved.", 1);
    const esp_err_t result = sticky_display_refresh_monochrome();
    ESP_LOGI(kTag,
             "screen_status=\"%s\" detail=\"%s\" refresh=%s",
             status,
             detail,
             esp_err_to_name(result));
}

void log_heap(const char *phase)
{
    ESP_LOGI(kTag,
             "heap phase=%s internal_free=%u internal_min=%u psram_free=%u psram_min=%u",
             phase,
             static_cast<unsigned>(heap_caps_get_free_size(
                 MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(
                 MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(
                 heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
             static_cast<unsigned>(
                 heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM)));
}

esp_err_t init_model()
{
    log_heap("before_model");
    s_runtime.models = esp_srmodel_init("model");
    if (s_runtime.models == nullptr) {
        return ESP_ERR_NOT_FOUND;
    }
    char *model_name =
        esp_srmodel_filter(s_runtime.models, ESP_MN_PREFIX, ESP_MN_CHINESE);
    if (model_name == nullptr) {
        return ESP_ERR_NOT_FOUND;
    }
    s_runtime.multinet = esp_mn_handle_from_name(model_name);
    if (s_runtime.multinet == nullptr) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    s_runtime.model_data =
        s_runtime.multinet->create(model_name, kModelDurationMs);
    if (s_runtime.model_data == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    s_runtime.samples_per_chunk =
        s_runtime.multinet->get_samp_chunksize(s_runtime.model_data);
    if (s_runtime.samples_per_chunk <= 0 ||
        s_runtime.multinet->get_samp_rate(s_runtime.model_data) != 16000) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (s_runtime.multinet->set_det_threshold(s_runtime.model_data,
                                              kModelDetectionThreshold) != 0) {
        return ESP_FAIL;
    }
    esp_err_t result =
        esp_mn_commands_alloc(s_runtime.multinet, s_runtime.model_data);
    if (result != ESP_OK) {
        return result;
    }
    for (const CommandPhrase &phrase : kCommandPhrases) {
        result = esp_mn_commands_add(phrase.command_id, phrase.pinyin);
        if (result != ESP_OK) {
            return result;
        }
    }
    esp_mn_error_t *command_errors = esp_mn_commands_update();
    if (command_errors != nullptr) {
        ESP_LOGE(kTag,
                 "commands=invalid count=%d",
                 static_cast<int>(command_errors->num));
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(kTag,
             "model=ready name=%s sample_rate=16000 chunk_samples=%d threshold=%.2f phrases=%u",
             model_name,
             s_runtime.samples_per_chunk,
             static_cast<double>(kModelDetectionThreshold),
             static_cast<unsigned>(std::size(kCommandPhrases)));
    esp_mn_commands_print();
    log_heap("after_model_and_commands");
    return ESP_OK;
}

esp_err_t init_microphone()
{
    gpio_config_t power = {};
    power.pin_bit_mask = 1ULL << PIN_MIC_EN;
    power.mode = GPIO_MODE_OUTPUT;
    esp_err_t result = gpio_config(&power);
    if (result != ESP_OK) {
        return result;
    }
    gpio_set_level(static_cast<gpio_num_t>(PIN_MIC_EN), 0);

    i2s_chan_config_t channel =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    result = i2s_new_channel(&channel, nullptr, &s_runtime.microphone);
    if (result != ESP_OK) {
        return result;
    }
    i2s_pdm_rx_config_t config = {};
    config.clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(16000);
    config.slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
    config.gpio_cfg.clk = static_cast<gpio_num_t>(PIN_MIC_CLK);
    config.gpio_cfg.din = static_cast<gpio_num_t>(PIN_MIC_DATA);
    return i2s_channel_init_pdm_rx_mode(s_runtime.microphone, &config);
}

void remember_results()
{
    esp_mn_results_t *results =
        s_runtime.multinet->get_results(s_runtime.model_data);
    if (results == nullptr) {
        return;
    }
    ESP_LOGI(kTag,
             "detect state=%d candidates=%d raw=\"%s\"",
             static_cast<int>(results->state),
             results->num,
             results->raw_string);
    for (int index = 0; index < results->num && index < ESP_MN_RESULT_MAX_NUM;
         ++index) {
        const VoiceIntent intent =
            voice_intent_from_command_id(results->command_id[index]);
        const int slot = static_cast<int>(intent) - 1;
        ESP_LOGI(kTag,
                 "candidate rank=%d command_id=%d phrase_id=%d probability=%.4f intent=%s",
                 index,
                 results->command_id[index],
                 results->phrase_id[index],
                 static_cast<double>(results->prob[index]),
                 voice_intent_name(intent));
        if (slot >= 0 && slot < static_cast<int>(s_runtime.best_probability.size())) {
            s_runtime.best_probability[slot] =
                std::max(s_runtime.best_probability[slot], results->prob[index]);
        }
    }
}

esp_mn_state_t feed_model(int16_t *samples)
{
    const esp_mn_state_t state =
        s_runtime.multinet->detect(s_runtime.model_data, samples);
    if (state == ESP_MN_STATE_DETECTED) {
        remember_results();
    }
    return state;
}

VoiceIntentDecision finish_decision(int16_t *samples)
{
    std::memset(samples,
                0,
                static_cast<size_t>(s_runtime.samples_per_chunk) *
                    sizeof(int16_t));
    const int chunks = std::max(
        1,
        kTrailingSilenceMs * 16000 /
            (1000 * s_runtime.samples_per_chunk));
    for (int index = 0; index < chunks; ++index) {
        if (feed_model(samples) == ESP_MN_STATE_TIMEOUT) {
            break;
        }
    }

    VoiceIntentCandidate candidates[4] = {};
    for (size_t index = 0; index < std::size(candidates); ++index) {
        candidates[index] = {
            static_cast<int>(index + 1U),
            s_runtime.best_probability[index],
        };
    }
    return voice_intent_decide(candidates, std::size(candidates));
}

[[noreturn]] void enter_test_sleep()
{
    if (s_runtime.microphone_enabled) {
        i2s_channel_disable(s_runtime.microphone);
        s_runtime.microphone_enabled = false;
    }
    gpio_set_level(static_cast<gpio_num_t>(PIN_MIC_EN), 0);
    show_status("SLEEPING", "Release SIDE keys; press TOP to wake");
    sticky_display_sleep();
    ESP_LOGI(kTag,
             "sleep=enter source=side_button_chord mic=off wake=top_button");
    board_power_enter_deep_sleep();
}

void run_capture()
{
    const int64_t prepare_started_us = esp_timer_get_time();
    s_runtime.best_probability.fill(0.0F);
    s_runtime.multinet->clean(s_runtime.model_data);
    gpio_set_level(static_cast<gpio_num_t>(PIN_MIC_EN), 1);
    vTaskDelay(pdMS_TO_TICKS(20));
    esp_err_t result = i2s_channel_enable(s_runtime.microphone);
    if (result != ESP_OK) {
        gpio_set_level(static_cast<gpio_num_t>(PIN_MIC_EN), 0);
        show_status("MIC START FAILED", esp_err_to_name(result));
        return;
    }
    s_runtime.microphone_enabled = true;
    const size_t bytes_per_chunk =
        static_cast<size_t>(s_runtime.samples_per_chunk) * sizeof(int16_t);
    auto *samples = static_cast<int16_t *>(heap_caps_malloc(
        bytes_per_chunk, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (samples == nullptr) {
        i2s_channel_disable(s_runtime.microphone);
        s_runtime.microphone_enabled = false;
        gpio_set_level(static_cast<gpio_num_t>(PIN_MIC_EN), 0);
        show_status("NO MEMORY", "Input chunk allocation failed");
        return;
    }

    const int64_t started_us = esp_timer_get_time();
    const uint32_t ready_latency_ms = static_cast<uint32_t>(
        (started_us - prepare_started_us) / 1000LL);
    bool released = false;
    bool timed_out = false;
    uint32_t chunks = 0U;
    ESP_LOGI(kTag,
             "capture=start ready_latency_ms=%" PRIu32
             " policy=screen_persistent silent=true",
             ready_latency_ms);
    while (!released && !timed_out) {
        size_t received = 0U;
        result = i2s_channel_read(s_runtime.microphone,
                                  samples,
                                  bytes_per_chunk,
                                  &received,
                                  100);
        if (result == ESP_OK && received == bytes_per_chunk) {
            ++chunks;
            timed_out = feed_model(samples) == ESP_MN_STATE_TIMEOUT;
        } else if (result != ESP_ERR_TIMEOUT) {
            ESP_LOGE(kTag,
                     "capture=read result=%s bytes=%u",
                     esp_err_to_name(result),
                     static_cast<unsigned>(received));
            break;
        }

        StickyButtonEvent event = StickyButtonEvent::None;
        while (sticky_button_take_event(event)) {
            if (event == StickyButtonEvent::LongPressEnd) {
                released = true;
            } else if (event == StickyButtonEvent::SingleClick ||
                       event == StickyButtonEvent::DoubleClick) {
                ESP_LOGE(kTag,
                         "button=unexpected_click_during_long event=%u result=fail",
                         static_cast<unsigned>(event));
            }
        }
        if (esp_timer_get_time() - started_us >=
            static_cast<int64_t>(kModelDurationMs) * 1000LL) {
            timed_out = true;
        }
    }

    i2s_channel_disable(s_runtime.microphone);
    s_runtime.microphone_enabled = false;
    gpio_set_level(static_cast<gpio_num_t>(PIN_MIC_EN), 0);
    const VoiceIntentDecision decision = finish_decision(samples);
    heap_caps_free(samples);
    const uint32_t elapsed_ms = static_cast<uint32_t>(
        (esp_timer_get_time() - started_us) / 1000LL);
    ESP_LOGI(kTag,
             "capture=end released=%s timeout=%s elapsed_ms=%" PRIu32
             " chunks=%" PRIu32 " intent=%s probability=%.4f runner_up=%.4f manual=%s",
             released ? "true" : "false",
             timed_out ? "true" : "false",
             elapsed_ms,
             chunks,
             voice_intent_name(decision.intent),
             static_cast<double>(decision.probability),
             static_cast<double>(decision.runner_up_probability),
             decision.needs_manual_selection ? "true" : "false");

    char detail[96];
    std::snprintf(detail,
                  sizeof(detail),
                  "p=%.2f next=%.2f / %lums",
                  static_cast<double>(decision.probability),
                  static_cast<double>(decision.runner_up_probability),
                  static_cast<unsigned long>(elapsed_ms));
    show_status(decision.needs_manual_selection
                    ? "MANUAL CATEGORY REQUIRED"
                    : voice_intent_name(decision.intent),
                detail);
}

}  // namespace

void sticky_voice_classifier_test_run()
{
    if (sticky_display_init() != ESP_OK) {
        return;
    }
    show_status("LOADING MODEL", "ESP-SR MultiNet5 Q8 / Chinese");
    esp_err_t result = sticky_button_init();
    if (result == ESP_OK) {
        result = init_model();
    }
    if (result == ESP_OK) {
        result = init_microphone();
    }
    if (result != ESP_OK) {
        show_status("INIT FAILED", esp_err_to_name(result));
        ESP_LOGE(kTag, "init=%s", esp_err_to_name(result));
        return;
    }
    const esp_sleep_wakeup_cause_t wake_cause = esp_sleep_get_wakeup_cause();
    show_status(wake_cause == ESP_SLEEP_WAKEUP_EXT1 ? "READY AFTER WAKE"
                                                    : "READY",
                "Commands: weight/checkup/habit/memo");
    ESP_LOGI(kTag,
             "ready wake_cause=%d after_deep_sleep=%s",
             static_cast<int>(wake_cause),
             wake_cause == ESP_SLEEP_WAKEUP_UNDEFINED ? "false" : "true");

    for (;;) {
        StickyButtonEvent event = StickyButtonEvent::None;
        if (!sticky_button_take_event(event)) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        ESP_LOGI(kTag, "button_event=%u", static_cast<unsigned>(event));
        if (event == StickyButtonEvent::LongPressStart) {
            run_capture();
        } else if (event == StickyButtonEvent::SleepChord) {
            enter_test_sleep();
        } else if (event == StickyButtonEvent::SingleClick ||
                   event == StickyButtonEvent::DoubleClick) {
            show_status("PRESS TOO SHORT", "Hold until READY threshold, then speak");
        }
    }
}

#endif
