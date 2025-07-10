#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "mesh/ble_mesh.h"

static const char *TAG = "mesh_root";

// Provisioning complete callback
typedef void (*prov_complete_cb_t)(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index);
static void prov_complete_cb(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index) {
    ESP_LOGI(TAG, "Provisioning complete: net_idx=%u, addr=0x%04x", net_idx, addr);
}

// BLE Mesh provisioning configuration
static const struct ble_mesh_prov prov = {
    .uuid             = {0x11,0x22,0x33,0x44, 0x55,0x66,0x77,0x88, 0x99,0xAA,0xBB,0xCC, 0xDD,0xEE,0xFF,0x00},
    .output_size      = 0,
    .output_actions   = 0,
    .complete_cb      = prov_complete_cb,
};

// Publication context (unused in root)
static struct ble_mesh_model_pub root_pub = {};

// Elements and models: Configuration & Health Server
static struct ble_mesh_elem root_elem = {
    .loc       = 0,
    .models    = (struct ble_mesh_model[]){
        BLE_MESH_MODEL_CFG_SRV(&root_pub),
        BLE_MESH_MODEL_HEALTH_SRV(&root_pub, &(struct ble_mesh_health_srv_cb){}),
        BLE_MESH_MODEL_NONE
    }
};

static const struct ble_mesh_comp comp = {
    .cid        = 0x05F1,    // Vendor Company ID
    .pid        = 0x0001,
    .vid        = 0x0059,
    .elem       = &root_elem,
    .elem_count = 1,
};

// Host reset callback
static void ble_mesh_on_reset(int reason) {
    ESP_LOGW(TAG, "BLE Host Reset - reason %d", reason);
}

// Host sync callback: init mesh
static void ble_mesh_on_sync(void) {
    esp_err_t err;
    ESP_LOGI(TAG, "BLE Host Sync");
    err = ble_mesh_init(&prov, &comp);
    if (err) {
        ESP_LOGE(TAG, "Mesh init failed: %d", err);
        return;
    }
    ESP_LOGI(TAG, "Mesh initialized as Root Node");
    // Auto-provision self as primary
    ble_mesh_provision(1, NULL, 0, 0, 0);
}

// NimBLE host task
static void host_task(void *param) {
    nimble_port_run();
}

void app_main(void) {
    esp_err_t ret;
    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Init NimBLE HCI & controller
    ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());
    nimble_port_init();

    // Set Host callbacks
    ble_hs_cfg.reset_cb = ble_mesh_on_reset;
    ble_hs_cfg.sync_cb  = ble_mesh_on_sync;

    // Start host thread
    nimble_port_freertos_init(host_task);

    ESP_LOGI(TAG, "ESP32 BLE Mesh Root Node starting...");
}
