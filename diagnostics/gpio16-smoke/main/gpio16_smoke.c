/* Standalone GPIO16 LED diagnostic. No Panel v4, radio, Wi-Fi or NVS. */
#include <stdint.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "driver/gpio.h"

#define SMOKE_GPIO GPIO_NUM_16
#define SMOKE_PIXELS 8u
#define SMOKE_BRIGHTNESS 204u /* 80% of 255 */
#define SMOKE_RMT_HZ 10000000u

static const char *TAG = "OT_GPIO16_SMOKE";
static uint8_t pixels[SMOKE_PIXELS * 3u]; /* GRB order */
static void fill_grb(uint8_t red, uint8_t green, uint8_t blue)
{
    for (unsigned i = 0; i < SMOKE_PIXELS; ++i) {
        pixels[i * 3u] = green;
        pixels[i * 3u + 1u] = red;
        pixels[i * 3u + 2u] = blue;
    }
}
static void transmit(rmt_channel_handle_t tx, rmt_encoder_handle_t enc)
{
    const rmt_transmit_config_t config = {
        .loop_count = 0, .flags = {.eot_level = 0}
    };
    ESP_ERROR_CHECK(rmt_transmit(tx, enc, pixels, sizeof(pixels), &config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(tx, 2000));
    /* Each next frame follows 2 seconds of low idle, much longer than
       the >280us WS2815 reset/latch window. */
}
void app_main(void)
{
    ESP_LOGW(TAG, "BARE-BONES DIAGNOSTIC: GPIO16 only, 8 LEDs, brightness 204/255");
    ESP_LOGI(TAG, "ESP-IDF RMT TX, no DMA, 10MHz, WS2815 bit timings, GRB");
    ESP_LOGW(TAG, "GPIO17 unused; NO Panel v4, BLE, Wi-Fi, or NVS access.");
    const rmt_tx_channel_config_t channel_cfg = {
        .gpio_num = SMOKE_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = SMOKE_RMT_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 2,
        .flags = {
            .invert_out = 0, .with_dma = 0,
            .io_od_mode = 0, .init_level = 0
        }
    };
    rmt_channel_handle_t tx = NULL;
    ESP_ERROR_CHECK(rmt_new_tx_channel(&channel_cfg, &tx));
    const rmt_bytes_encoder_config_t encoder_cfg = {
        .bit0 = {.level0 = 1, .duration0 = 3, .level1 = 0, .duration1 = 9},
        .bit1 = {.level0 = 1, .duration0 = 9, .level1 = 0, .duration1 = 3},
        .flags = {.msb_first = 1}
    };
    rmt_encoder_handle_t enc = NULL;
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&encoder_cfg, &enc));
    ESP_ERROR_CHECK(rmt_enable(tx));
    /* Give the user time to restore 12V strip power after USB flash. */
    vTaskDelay(pdMS_TO_TICKS(3000));
    const struct {const char *name; uint8_t r,g,b;} sequence[] = {
        {"RED", 204,0,0}, {"GREEN", 0,204,0},
        {"BLUE", 0,0,204}, {"WHITE", 204,204,204}
    };
    for (unsigned round = 0; round < 3; ++round) {
        for (unsigned i = 0; i < sizeof(sequence)/sizeof(sequence[0]); ++i) {
            fill_grb(sequence[i].r,sequence[i].g,sequence[i].b);
            transmit(tx,enc);
            ESP_LOGI(TAG, "round %u/3 %s: RMT DONE (not optical proof)",
                     round+1,sequence[i].name);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
    fill_grb(0,0,0);
    transmit(tx,enc);
    ESP_LOGI(TAG, "TEST COMPLETE: first 8 LEDs OFF. Observe actual optical result.");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
