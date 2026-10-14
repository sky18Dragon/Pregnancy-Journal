#include "sticky_sd_audio_test.h"

#if defined(STICKY_SD_AUDIO_TEST) && STICKY_SD_AUDIO_TEST
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "board_sd_card.h"
#include "canvas.h"
#include "driver/gpio.h"
#include "driver/i2s_pdm.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "pcm_wav.h"
#include "pin_config.h"
#include "sticky_button.h"
#include "sticky_display.h"

namespace {
constexpr char kTag[] = "sd_audio_test";
constexpr size_t kBlockBytes = 4096;
constexpr size_t kBlocks = 32;  // 128 KiB PSRAM, about four seconds of PCM.
portMUX_TYPE overflow_lock = portMUX_INITIALIZER_UNLOCKED;
bool did_overflow = false;
struct Block { uint32_t index; uint32_t size; };
struct Recording {
    uint8_t *pool = nullptr;
    QueueHandle_t free_blocks = nullptr;
    QueueHandle_t ready_blocks = nullptr;
    i2s_chan_handle_t mic = nullptr;
    std::atomic<bool> cancelled{false}, capture_done{false}, running{false};
    std::atomic<esp_err_t> error{ESP_OK};
    std::atomic<uint32_t> written{0}, max_write_ms{0};
    char temporary[64] = {}, final_path[64] = {};
} recording;

void fail(esp_err_t error)
{
    esp_err_t expected = ESP_OK;
    recording.error.compare_exchange_strong(expected, error);
    recording.cancelled = true;
}

bool overflow(i2s_chan_handle_t, i2s_event_data_t *, void *)
{
    // Xtensa std::atomic is not guaranteed lock-free. Use the ISR-safe mux.
    portENTER_CRITICAL_ISR(&overflow_lock);
    did_overflow = true;
    portEXIT_CRITICAL_ISR(&overflow_lock);
    return false;
}

bool capture_overflowed()
{
    portENTER_CRITICAL(&overflow_lock);
    const bool result = did_overflow;
    portEXIT_CRITICAL(&overflow_lock);
    return result;
}

void capture(void *)
{
    uint32_t captured = 0;
    while (captured < pcm_wav::kPrototypeBytes && !recording.cancelled) {
        uint32_t index;
        if (xQueueReceive(recording.free_blocks, &index, 0) != pdTRUE) {
            fail(ESP_ERR_NO_MEM);
            break;
        }
        size_t received = 0;
        const size_t wanted = std::min<size_t>(kBlockBytes, pcm_wav::kPrototypeBytes - captured);
        const esp_err_t result = i2s_channel_read(recording.mic,
            recording.pool + index * kBlockBytes, wanted, &received, 1000);
        if (result != ESP_OK || received == 0 || received % 2 != 0 || capture_overflowed()) {
            xQueueSend(recording.free_blocks, &index, 0);
            fail(result == ESP_OK ? ESP_FAIL : result);
            break;
        }
        Block block{index, static_cast<uint32_t>(received)};
        if (xQueueSend(recording.ready_blocks, &block, 0) != pdTRUE) {
            xQueueSend(recording.free_blocks, &index, 0);
            fail(ESP_ERR_NO_MEM);
            break;
        }
        captured += received;
    }
    i2s_channel_disable(recording.mic);
    if (capture_overflowed()) fail(ESP_ERR_NO_MEM);
    gpio_set_level(static_cast<gpio_num_t>(PIN_MIC_EN), 0);
    recording.capture_done = true;
    vTaskDelete(nullptr);
}

bool write_all(int fd, const uint8_t *bytes, size_t count)
{
    while (count) {
        if (!board_sd_card_present()) return false;
        const ssize_t written = write(fd, bytes, count);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return false;
        bytes += written;
        count -= written;
    }
    return true;
}

bool verify(uint32_t expected_crc)
{
    int fd = open(recording.temporary, O_RDONLY);
    if (fd < 0) return false;
    uint8_t buffer[kBlockBytes];
    const auto expected_header = pcm_wav::header(pcm_wav::kPrototypeBytes);
    bool ok = read(fd, buffer, expected_header.size()) == static_cast<ssize_t>(expected_header.size()) &&
              std::memcmp(buffer, expected_header.data(), expected_header.size()) == 0;
    uint32_t crc = UINT32_MAX, total = 0;
    while (ok) {
        const ssize_t count = read(fd, buffer, sizeof(buffer));
        if (count < 0) { ok = false; break; }
        if (count == 0) break;
        total += count;
        if (total > pcm_wav::kPrototypeBytes) { ok = false; break; }
        crc = pcm_wav::crc32(crc, buffer, count);
    }
    if (close(fd) != 0) ok = false;
    return ok && total == pcm_wav::kPrototypeBytes && crc == expected_crc;
}

esp_err_t init_microphone()
{
    gpio_config_t power = {};
    power.pin_bit_mask = 1ULL << PIN_MIC_EN;
    power.mode = GPIO_MODE_OUTPUT;
    esp_err_t result = gpio_config(&power);
    if (result != ESP_OK) return result;
    gpio_set_level(static_cast<gpio_num_t>(PIN_MIC_EN), 0);
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    result = i2s_new_channel(&channel, nullptr, &recording.mic);
    if (result != ESP_OK) return result;
    i2s_pdm_rx_config_t config = {};
    config.clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(pcm_wav::kSampleRate);
    config.slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
    config.gpio_cfg.clk = static_cast<gpio_num_t>(PIN_MIC_CLK);
    config.gpio_cfg.din = static_cast<gpio_num_t>(PIN_MIC_DATA);
    result = i2s_channel_init_pdm_rx_mode(recording.mic, &config);
    if (result != ESP_OK) return result;
    i2s_event_callbacks_t callbacks = {};
    callbacks.on_recv_q_ovf = overflow;
    return i2s_channel_register_event_callback(recording.mic, &callbacks, nullptr);
}

void record_worker(void *)
{
    int fd = -1;
    bool capture_started = false;
    uint32_t crc = UINT32_MAX;
    uint64_t total_bytes = 0, free_bytes = 0;
    if (esp_vfs_fat_info("/sdcard", &total_bytes, &free_bytes) != ESP_OK ||
        free_bytes < pcm_wav::kPrototypeBytes + 64 * 1024) {
        ESP_LOGE(kTag, "capacity preflight failed; no recording started");
        fail(ESP_ERR_INVALID_SIZE);
    }
    if (!recording.cancelled && mkdir("/sdcard/S0TEST", 0700) != 0 && errno != EEXIST) fail(ESP_FAIL);
    // FAT 8.3 names avoid changing global long-filename configuration.
    for (int attempt = 0; attempt < 16 && !recording.cancelled; ++attempt) {
        const unsigned long id = esp_random();
        snprintf(recording.temporary, sizeof(recording.temporary), "/sdcard/S0TEST/%08lX.TMP", id);
        snprintf(recording.final_path, sizeof(recording.final_path), "/sdcard/S0TEST/%08lX.WAV", id);
        struct stat existing;
        if (stat(recording.final_path, &existing) == 0) continue;
        fd = open(recording.temporary, O_CREAT | O_EXCL | O_RDWR, 0600);
        if (fd >= 0 || errno != EEXIST) break;
    }
    if (fd < 0) fail(ESP_FAIL);
    recording.pool = static_cast<uint8_t *>(heap_caps_malloc(kBlockBytes * kBlocks, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    recording.free_blocks = xQueueCreate(kBlocks, sizeof(uint32_t));
    recording.ready_blocks = xQueueCreate(kBlocks, sizeof(Block));
    if (!recording.pool || !recording.free_blocks || !recording.ready_blocks) fail(ESP_ERR_NO_MEM);
    if (!recording.cancelled) {
        for (uint32_t i = 0; i < kBlocks; ++i) xQueueSend(recording.free_blocks, &i, 0);
        const auto header = pcm_wav::header(0);
        if (!write_all(fd, header.data(), header.size()) || fsync(fd) != 0) fail(ESP_FAIL);
    }
    if (!recording.cancelled) {
        const esp_err_t result = init_microphone();
        if (result != ESP_OK) fail(result);
    }
    if (!recording.cancelled) {
        gpio_set_level(static_cast<gpio_num_t>(PIN_MIC_EN), 1);
        vTaskDelay(pdMS_TO_TICKS(20));
        const esp_err_t result = i2s_channel_enable(recording.mic);
        if (result != ESP_OK) fail(result);
        else if (xTaskCreate(capture, "pdm_capture", 4096, nullptr, 8, nullptr) != pdPASS) {
            i2s_channel_disable(recording.mic);
            fail(ESP_ERR_NO_MEM);
        } else capture_started = true;
    }
    while (capture_started && (!recording.capture_done || uxQueueMessagesWaiting(recording.ready_blocks))) {
        Block block;
        if (xQueueReceive(recording.ready_blocks, &block, pdMS_TO_TICKS(100)) != pdTRUE) continue;
        const auto *bytes = recording.pool + block.index * kBlockBytes;
        const int64_t started = esp_timer_get_time();
        if (!recording.cancelled) {
            if (!write_all(fd, bytes, block.size)) fail(ESP_FAIL);
            else {
                crc = pcm_wav::crc32(crc, bytes, block.size);
                recording.written.fetch_add(block.size);
            }
        }
        const auto elapsed = static_cast<uint32_t>((esp_timer_get_time() - started) / 1000);
        if (elapsed > recording.max_write_ms) recording.max_write_ms = elapsed;
        xQueueSend(recording.free_blocks, &block.index, 0);
    }
    if (!recording.cancelled && recording.written == pcm_wav::kPrototypeBytes) {
        const auto header = pcm_wav::header(recording.written);
        if (lseek(fd, 0, SEEK_SET) != 0 || !write_all(fd, header.data(), header.size()) || fsync(fd) != 0)
            fail(ESP_FAIL);
    } else fail(ESP_FAIL);
    if (fd >= 0 && close(fd) != 0) fail(ESP_FAIL);
    if (!recording.cancelled && !verify(crc)) fail(ESP_FAIL);
    if (!recording.cancelled) {
        struct stat existing;
        // Single writer owns this namespace. Never intentionally replace an existing file.
        if (stat(recording.final_path, &existing) == 0 ||
            rename(recording.temporary, recording.final_path) != 0) fail(ESP_FAIL);
    }
    if (recording.mic) i2s_del_channel(recording.mic);
    gpio_set_level(static_cast<gpio_num_t>(PIN_MIC_EN), 0);
    if (recording.free_blocks) vQueueDelete(recording.free_blocks);
    if (recording.ready_blocks) vQueueDelete(recording.ready_blocks);
    heap_caps_free(recording.pool);
    ESP_LOGI(kTag, "result=%s pcm_bytes=%lu max_write_ms=%lu file=%s",
        esp_err_to_name(recording.error.load()), static_cast<unsigned long>(recording.written.load()),
        static_cast<unsigned long>(recording.max_write_ms.load()),
        recording.cancelled ? recording.temporary : recording.final_path);
    recording.running = false;
    vTaskDelete(nullptr);
}

esp_err_t show(const char *status, bool partial = false)
{
    Canvas *canvas = sticky_display_canvas();
    canvas->clear();
    canvas->draw_text(32, 40, "S0 SD + AUDIO TEST", 3);
    canvas->draw_text(32, 120, status, 2);
    canvas->draw_text(32, 210, "30s / PCM16 / 16kHz / MONO", 2);
    canvas->draw_text(32, 270, "No format. No upload. No Flash fallback.", 1);
    const esp_err_t result = partial ? sticky_display_refresh_partial() : sticky_display_refresh_monochrome();
    if (result != ESP_OK) {
        ESP_LOGE(kTag, "display=%s", esp_err_to_name(result));
        if (recording.running) fail(result);
    }
    ESP_LOGI(kTag, "status=\"%s\" refresh=%s", status, esp_err_to_name(result));
    return result;
}
}  // namespace

void sticky_sd_audio_test_run()
{
    // Mount before any display clocks put an inserted SD card in SPI mode.
    const esp_err_t mounted = board_sd_card_mount();
    if (mounted != ESP_OK) gpio_set_level(static_cast<gpio_num_t>(PIN_SD_EN), 0);
    ESP_LOGI(kTag, "mount=%s", esp_err_to_name(mounted));
    if (sticky_display_init() != ESP_OK) {
        if (mounted == ESP_OK) board_sd_card_unmount();
        return;
    }
    if (mounted != ESP_OK) { show("Card unavailable. Power off and check card."); return; }
#if defined(STICKY_SD_DISPLAY_TEST) && STICKY_SD_DISPLAY_TEST
    // Reproduce the finish path without microphone access or file writes.
    show("BEFORE UNMOUNT", false);
    vTaskDelay(pdMS_TO_TICKS(2000));
    const esp_err_t display_unmounted = board_sd_card_unmount();
    ESP_LOGI(kTag, "display_only unmount=%s", esp_err_to_name(display_unmounted));
    show(display_unmounted == ESP_OK ? "AFTER UNMOUNT - CHECK SCREEN" : "UNMOUNT FAILED", false);
    return;
#endif
    if (sticky_button_init() != ESP_OK) {
        board_sd_card_unmount();
        show("Button init failed. See serial log.");
        return;
    }
    show("Press TOP once to record. Keep card inserted.");
    for (;;) {
        StickyButtonEvent event;
        if (!board_sd_card_present()) {
            board_sd_card_unmount();
            show("Card removed. Reboot required.");
            return;
        }
        if (sticky_button_take_event(event) && event == StickyButtonEvent::SingleClick) break;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    show("Recording starts after this refresh...");
    recording.running = true;
    if (xTaskCreate(record_worker, "sd_audio_writer", 8192, nullptr, 5, nullptr) != pdPASS) {
        recording.running = false;
        fail(ESP_ERR_NO_MEM);
    }
    int tick = 0;
    while (recording.running) {
        if (!board_sd_card_present()) fail(ESP_ERR_NOT_FOUND);
        vTaskDelay(pdMS_TO_TICKS(100));
        if (++tick % 50 == 0 && recording.running) {
            char status[80];
            snprintf(status, sizeof(status), "Recording / verifying: %lu bytes",
                     static_cast<unsigned long>(recording.written.load()));
            show(status, tick % 100 != 0);
        }
    }
    const esp_err_t unmounted = board_sd_card_unmount();
    ESP_LOGI(kTag, "unmount=%s", esp_err_to_name(unmounted));
    const bool ok = recording.error == ESP_OK && unmounted == ESP_OK;
    show(ok ? "PASS: WAV readback verified. Reboot to repeat." : "FAIL: see serial log. Reboot required.");
}
#endif
