#pragma once
/* Local HTTP OTA, BENCH ONLY: WPA2 AP and same-origin /dev confirmation.
 * The device never downloads unauthenticated GitHub URLs on its own.
 * OTA updates require an already installed two-slot OTA partition table.
 */
#include <stdbool.h>
#include "esp_http_server.h"
bool ot_ota_available(void);
esp_err_t ot_ota_status_get(httpd_req_t *r);
esp_err_t ot_ota_upload_post(httpd_req_t *r);
/* True when validation succeeded or the running image needs no confirmation. */
bool ot_ota_confirm_boot(bool outputs_ready);
