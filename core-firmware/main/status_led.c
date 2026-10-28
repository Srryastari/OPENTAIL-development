#include "status_led.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "led_strip.h"
#include "led_strip_spi.h"
#include "driver/spi_master.h"
#include "board_config.h"

#define OT_ST_TICK_MS 10
#define OT_ST_SELF_TEST_MS 5000u
#define OT_ST_HEARTBEAT_TIMEOUT_MS 8000
static const char *TAG="OT_STATUS_LED";
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static led_strip_handle_t status_pixel=NULL;
static bool available=false,wifi_ready=false,had_session=false,had_ble_session=false,show_active=false,fault=false,ble_connected=false;
static uint32_t startup_ms=0,last_heartbeat_ms=0;
/* No NVS persistence; 5-second diagnostic test affects ONLY onboard WS2812. */
static ot_st_mode_t self_test_mode=OT_ST_OFF;
static uint32_t self_test_start_ms=0,self_test_deadline_ms=0;
static bool self_test_now(uint32_t now){
    return ot_st_test_window_active(self_test_mode,now,self_test_deadline_ms);
}
static uint32_t ms_now(void){return (uint32_t)(esp_timer_get_time()/1000LL);}

static ot_st_mode_t mode_at(uint32_t now){
    if(fault)return OT_ST_FAULT;
    if((uint32_t)(now-startup_ms)<780u)return OT_ST_STARTUP;
    /* A real fault always wins. A self-test never changes the real fault flag. */
    if(self_test_now(now))return self_test_mode;
    /* Native Show Mode indication returns whenever a real show is rendering. */
    if(show_active)return OT_ST_SHOW;
    bool wifi_session=had_session && (uint32_t)(now-last_heartbeat_ms)<OT_ST_HEARTBEAT_TIMEOUT_MS;
    if(wifi_session && ble_connected)return OT_ST_BOTH;
    if(ble_connected)return OT_ST_BLE;
    if(wifi_session)return OT_ST_CONNECTED;
    if(had_session || had_ble_session)return OT_ST_DISCONNECTED;
    if(wifi_ready)return OT_ST_WIFI;
    return OT_ST_OFF;
}
void ot_st_snapshot(ot_st_snapshot_t *out){
    if(!out)return;
    uint32_t now=ms_now();
    portENTER_CRITICAL(&lock);
    bool test_active=!fault && (uint32_t)(now-startup_ms)>=780u && self_test_now(now);
    *out=(ot_st_snapshot_t){.available=available,.enabled=true,.wifi_ready=wifi_ready,
        .show_active=show_active,.ever_connected=had_session,.ble_connected=ble_connected,.brightness=OT_ST_FIXED_BRIGHTNESS,
        .mode=mode_at(now),.self_test_active=test_active,
        .self_test_mode=test_active?self_test_mode:OT_ST_OFF,
        .self_test_remaining_ms=test_active?(self_test_deadline_ms-now):0u};
    portEXIT_CRITICAL(&lock);
}
bool ot_st_self_test(ot_st_mode_t mode){
    /* Red is reserved for actual CORE faults, not a simulated fault self-test. */
    if(mode!=OT_ST_SHOW && mode!=OT_ST_OFF)return false;
    uint32_t now=ms_now();
    portENTER_CRITICAL(&lock);
    /* Cannot conceal a real fault, start during initialization, or use a missing LED. */
    bool ok=(mode==OT_ST_OFF) || (available && !fault && (uint32_t)(now-startup_ms)>=780u);
    if(ok){
        self_test_mode=mode;
        self_test_start_ms=now;
        self_test_deadline_ms=mode==OT_ST_OFF?now:now+OT_ST_SELF_TEST_MS;
    }
    portEXIT_CRITICAL(&lock);
    return ok;
}
void ot_st_ble_connected(bool active){
    portENTER_CRITICAL(&lock);
    ble_connected=active;
    if(active)had_ble_session=true;
    portEXIT_CRITICAL(&lock);
}
void ot_st_wifi_ready(void){portENTER_CRITICAL(&lock);wifi_ready=true;portEXIT_CRITICAL(&lock);}
void ot_st_set_fault(const char *reason){
    portENTER_CRITICAL(&lock);fault=true;portEXIT_CRITICAL(&lock);
    ESP_LOGE(TAG,"CORE fault: %s",reason?reason:"unknown");
}
void ot_st_show_active(bool on){portENTER_CRITICAL(&lock);show_active=on;portEXIT_CRITICAL(&lock);}
void ot_st_session(bool active){
    uint32_t now=ms_now();portENTER_CRITICAL(&lock);
    if(active){had_session=true;last_heartbeat_ms=now;}
    else if(had_session)last_heartbeat_ms=now-OT_ST_HEARTBEAT_TIMEOUT_MS;
    portEXIT_CRITICAL(&lock);
}
void ot_st_format_json(char *buf,size_t size){
    ot_st_snapshot_t s;ot_st_snapshot(&s);
    snprintf(buf,size,"{\"supported\":true,\"available\":%s,\"enabled\":%s,\"brightness\":%u,\"mode\":\"%s\",\"gpio\":%u,\"studio_session_seen\":%s,\"ble_connected\":%s,\"self_test_active\":%s,\"self_test_mode\":\"%s\",\"self_test_remaining_ms\":%lu}",
        s.available?"true":"false",s.enabled?"true":"false",s.brightness,ot_st_mode_name(s.mode),
        (unsigned)OT_STATUS_LED_GPIO,s.ever_connected?"true":"false",s.ble_connected?"true":"false",
        s.self_test_active?"true":"false",s.self_test_active?(s.self_test_mode==OT_ST_SHOW?"rainbow":"fault"):"none",
        (unsigned long)s.self_test_remaining_ms);
}
static void status_task(void *arg){
    (void)arg;ot_st_rgb_t prior={255,255,255};
    while(1){
        uint32_t now=ms_now();ot_st_snapshot_t s;ot_st_snapshot(&s);
        /* Show/rainbow self-test starts its cycle from zero, not from uptime. */
        uint32_t phase=now-startup_ms;
        if(s.self_test_active){
            portENTER_CRITICAL(&lock);
            phase=now-self_test_start_ms;
            portEXIT_CRITICAL(&lock);
        }
        ot_st_rgb_t c=ot_st_render(s.mode,phase,s.brightness,s.enabled);
        if(status_pixel && (c.r!=prior.r||c.g!=prior.g||c.b!=prior.b)){
            esp_err_t e=led_strip_set_pixel(status_pixel,0,c.r,c.g,c.b);
            if(e==ESP_OK)e=led_strip_refresh(status_pixel);
            if(e!=ESP_OK){ESP_LOGE(TAG,"Onboard RGB update failed: %s",esp_err_to_name(e));
                portENTER_CRITICAL(&lock);available=false;portEXIT_CRITICAL(&lock);status_pixel=NULL;
            }
        }
        prior=c;
        vTaskDelay(pdMS_TO_TICKS(OT_ST_TICK_MS));
    }
}
void ot_st_init(void){
    startup_ms=ms_now();
    const led_strip_config_t cfg={.strip_gpio_num=OT_STATUS_LED_GPIO,.max_leds=1,
        .led_model=LED_MODEL_WS2812,.color_component_format=LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags={.invert_out=false}};
    const led_strip_spi_config_t spi={.clk_src=SPI_CLK_SRC_DEFAULT,.spi_bus=SPI2_HOST,.flags={.with_dma=false}};
    esp_err_t e=led_strip_new_spi_device(&cfg,&spi,&status_pixel);
    if(e==ESP_OK){
        available=true;
        ESP_LOGI(TAG,"Onboard WS2812 status RGB initialized at GPIO%d via SPI2",OT_STATUS_LED_GPIO);
        e=led_strip_clear(status_pixel);
    }
    if(e!=ESP_OK){ESP_LOGE(TAG,"Onboard status LED unavailable: %s; continuing without it",esp_err_to_name(e));available=false;status_pixel=NULL;}
    if(xTaskCreate(status_task,"ot_status_rgb",4096,NULL,5,NULL)!=pdPASS){
        ESP_LOGE(TAG,"Could not start onboard RGB task");available=false;
    }
}
