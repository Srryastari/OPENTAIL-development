#pragma once
/* Identity for correlating the same physical CORE over BLE and Wi-Fi.
 * This public MAC-derived identifier is NOT authentication or a secret.
 * Fail closed (empty identifier) if the base MAC cannot be read.
 */
#include <stdio.h>
#include <stdint.h>
#include "esp_mac.h"
static inline void ot_controller_id(char out[13]) {
    uint8_t mac[6]={0};
    if(esp_read_mac(mac,ESP_MAC_BASE)!=ESP_OK){out[0]=0;return;}
    (void)snprintf(out,13,"%02X%02X%02X%02X%02X%02X",
        mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}
