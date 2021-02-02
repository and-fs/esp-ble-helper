// main.cpp file
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#include "ble_server.h" // contains BLE the Server-Class.

#include <string.h> // for memcpy

// -------------------------------------------------------------------------------------------------------------------

extern "C" void app_main(void);

// -------------------------------------------------------------------------------------------------------------------

#define APP_ID 0x55

BLEServer *pServer = nullptr;

static const uint16_t
    uuid_0xffe4 = 0xffe4,
    uuid_0xffe9 = 0xffe9;

static uint8_t
    v_rx[20] = {0},                    // readable value
    v_rx_config[2] = {0x00, 0x00},     // config for rx characteristic (required for notification)
    v_tx[20] = {0},                    // writeable value
    rx_svc_idx = 0,                    // index of service containing the rx characteristic
    rx_char_idx = 0;                   // index of rx characteristic

// -------------------------------------------------------------------------------------------------------------------

static void OnGATTEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    if (pServer)
        pServer->HandleGATTEvent(event, gatts_if, param);
}

static void OnGAPEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    if (pServer)
        pServer->HandleGAPEvent(event, param);
}

// -------------------------------------------------------------------------------------------------------------------

void OnChannelWrite(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    // the characteristic to which this handler belongs to is write only.
    // so the only event we receive here is ESP_GATTS_WRITE_EVT
    assert(event == ESP_GATTS_WRITE_EVT);

    esp_ble_gatts_cb_param_t::gatts_write_evt_param &data = param->write;

    // we expect a command as header (0xAA), command id (1 Byte), footer (0x55)
    if (data.len != 3)
        return;

    // check header and footer
    if (data.value[0] != 0xAA || data.value[2] != 0x55)
        return;

    uint8_t command = data.value[1]; // id of the command
    uint8_t response_length = 0;

    switch(command)
    {
        case 0x01:
            response_length = 5;
            memcpy(v_tx, "Hello", response_length);
            break;
        case 0x02:
            response_length = 3;
            memcpy(v_tx, "Bye", response_length);
            break;
        default:
            response_length = 4;
            memcpy(v_tx, "WTF?", response_length);
            break;
    }

    // Get the handle of the notification characteristic
    uint16_t hdl = pServer->GetHandle(rx_svc_idx, rx_char_idx);
    // Send the notification
    esp_ble_gatts_send_indicate(gatts_if, data.conn_id, hdl, response_length, v_tx, false);
}

static void AddAttributes(BLEServer *pServ)
{
    pServ->AddService(0xffe5);

    pServ->AddCharacteristic(
        &uuid_0xffe9,            // UUID of the characteristic
        &char_prop_write_norsp,  // property definition (write without response)
        ESP_GATT_PERM_WRITE,     // permission flag (write)
        sizeof(v_tx),            // maximum data size
        0,                       // current data size
        v_tx,                    // the data block itself
        "TX-Channel",            // name of the characteristic (user description 0x2901)
        OnChannelWrite           // event handler for the characteristics
    );

    rx_svc_idx = pServ->AddService(0xffe0);

    rx_char_idx = pServ->AddCharacteristic(
        &uuid_0xffe4,
        &char_prop_notify,
        ESP_GATT_PERM_READ,
        20, 0, v_rx,
        "RX-Channel",
        nullptr,
        v_rx_config
    );

}

// -------------------------------------------------------------------------------------------------------------------

void app_main(void)
{
    esp_err_t ret;

    /* Initialize NVS. */
    ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    if ((ret = esp_bt_controller_init(&bt_cfg))) // assignment!
    {
        ESP_LOGE("app", "Initialization failed: %s", esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_BLE))) // assignment!
    {
        ESP_LOGE("app", "Failed to enable controller: %s", esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_init())) // assignment!
    {
        ESP_LOGE("app", "Failed to init bluetooth: %s", esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_enable())) // assignment!
    {
        ESP_LOGE("app", "Failed to enable bluetooth: %s", esp_err_to_name(ret));
        return;
    }

    pServer = new BLEServer("MyDevice");
    AddAttributes(pServer);

    if ((ret = esp_ble_gatts_register_callback(OnGATTEvent)))  // assignment!
    {
        ESP_LOGE("app", "Failed to register GATT event handler, error code = %x", ret);
        return;
    }

    if ((ret = esp_ble_gap_register_callback(OnGAPEvent)))  // assignment!
    {
        ESP_LOGE("app", "Failed to register GAP event handler, error code = %x", ret);
        return;
    }

    if ((ret = esp_ble_gatts_app_register(APP_ID))) // assignment
    {
        ESP_LOGE("app", "Failed to register app, error code = %x", ret);
        return;
    }

    if ((ret = esp_ble_gatt_set_local_mtu(500))) // assignment
    {
        ESP_LOGE("app", "Setting local MTU failed, error code = %x", ret);
    }
}

