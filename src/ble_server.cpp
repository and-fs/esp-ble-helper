# include "ble_server.h"
# include <esp_log.h>
# include <cstring>
# include "main.h"
// -------------------------------------------------------------------------------------------------------------------
//# define BUILD_WITH_LOGS
// -------------------------------------------------------------------------------------------------------------------
# ifdef BUILD_WITH_LOGS
#  define LOGD(...) ESP_LOGD(__VA_ARGS__) 
#  define LOGI(...) ESP_LOGI(__VA_ARGS__) 
#  define LOGW(...) ESP_LOGW(__VA_ARGS__) 
#  define LOGE(...) ESP_LOGE(__VA_ARGS__) 
#  define LOGDUMP(...) ESP_LOG_BUFFER_HEXDUMP(__VA_ARGS__)
# else
#  define LOGD(...) {}
#  define LOGI(...) {}
#  define LOGW(...) {}
#  define LOGE(...) {}
#  define LOGDUMP(...) {}
# endif
// -------------------------------------------------------------------------------------------------------------------
const uint8_t ADV_CONFIG_FLAG = (1 << 0);
const uint8_t SCAN_RSP_CONFIG_FLAG = (1 << 1);
// -------------------------------------------------------------------------------------------------------------------
#define SVCID_FROM_KEY(key) ((key >> 8) & 0x7F)
#define IDX_FROM_KEY(key) (key & 0xFF)
uint16_t CreateHandlerKey(uint8_t service_id, uint8_t idx)
{
    uint16_t key = (service_id << 8) | idx;
    assert((key & 0x8000) == 0); // first bit should not be set
    // setting the first allows us to use both handles and this
    // key type to be used within the same map, because neither
    // an attribute handle nor a service id 
    // will exceed this value (currently)
    key |= 0x8000;
    return key;
}
// -------------------------------------------------------------------------------------------------------------------
BLEService::BLEService(uint16_t uuid, uint8_t service_id)
: m_uuid(uuid)
, m_service_id(service_id)
{
    AddAttribute(
        {
            ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
            sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&m_uuid
        }
    );
}
// -------------------------------------------------------------------------------------------------------------------
void BLEService::RegisterAttributes(esp_gatt_if_t gatts_if)
{
    uint8_t count = GetCount();
    LOGI("SVC", "Adding %d attributes for service %d with uuid=%04x", count, m_service_id, m_uuid);
    esp_err_t ec = esp_ble_gatts_create_attr_tab(m_gatt_db.data(), gatts_if, count, m_service_id);
    if (ec)
        LOGE(
            "SVC", "Adding attribute table for service %d with uuid=%04x failed, error code=%d",
            m_service_id, m_uuid, ec
        );
}
// -------------------------------------------------------------------------------------------------------------------
BLEService::size_type BLEService::AddAttributeDB(const esp_gatts_attr_db_t& attr)
{
    size_t result = m_gatt_db.size();
    m_gatt_db.push_back(attr);
    return result;
}
// -------------------------------------------------------------------------------------------------------------------
BLEService::size_type BLEService::AddAttribute(const esp_attr_desc_t& attr, uint8_t response)
{
    return AddAttributeDB({{response}, attr});
}
// -------------------------------------------------------------------------------------------------------------------
BLEService::size_type BLEService::AddNameDescription(const char* description)
{
    assert(description);
    
    LOGD(
        "SVC",
        "Adding name description '%s' (%04x), max_length=%d, length=%d",
        description, character_client_descr_uuid, strlen(description), strlen(description)
    );

    return AddAttribute({
            ESP_UUID_LEN_16, (uint8_t *)&character_client_descr_uuid, ESP_GATT_PERM_READ,
            (uint16_t)strlen(description), (uint16_t)strlen(description), (uint8_t *)description
    });
}
// -------------------------------------------------------------------------------------------------------------------
BLEService::size_type BLEService::AddConfigDescription(uint8_t *config_descr)
{
    assert(config_descr);
    LOGD(
        "SVC",
        "Adding config description (%04x) = %02x%02x.",
        character_client_config_uuid, config_descr[0], config_descr[1]
    );
    return AddAttribute({
        ESP_UUID_LEN_16, (uint8_t*)&character_client_config_uuid,
        ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE, 2, 2, config_descr
    });
}
// -------------------------------------------------------------------------------------------------------------------
BLEService::size_type BLEService::AddCharacteristic(
    const uint16_t* uuid, const uint8_t* properties,
    uint16_t permissions,
    uint16_t max_length, uint16_t length, uint8_t* value,
    uint8_t response
)
{
    assert(uuid);
    assert(properties);

    LOGI("SVC", "Adding attribute uuid=%04x, max_length=%d, length=%d, pos=%d", *uuid, max_length, length, GetCount());

    if (npos == AddAttribute({ ESP_UUID_LEN_16,
                               (uint8_t*)&character_declaration_uuid, ESP_GATT_PERM_READ,
                               (uint16_t)sizeof(uint8_t), (uint16_t)sizeof(uint8_t), (uint8_t*)properties
                            })
        )
        return npos;

    return AddAttribute({ESP_UUID_LEN_16, (uint8_t*)uuid, permissions, max_length, length, value}, response);
}
// -------------------------------------------------------------------------------------------------------------------
uint16_t BLEService::GetHandle(uint8_t index)
{
    if (index < m_handles.size())
        return m_handles[index];
    
    assert(0);
    return 0;
}
// -------------------------------------------------------------------------------------------------------------------
void BLEService::SetHandles(uint16_t* handles, uint8_t count)
{
    assert (count == GetCount());
    m_handles.resize(count);
    memcpy(m_handles.data(), handles, count * sizeof(uint16_t));
}
// -------------------------------------------------------------------------------------------------------------------
BLEService::ptr BLEService::Create(uint16_t uuid, uint8_t service_id)
{
    return ptr(new BLEService(uuid, service_id));
}
// -------------------------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------------------------
BLEServer::BLEServer(const char* device_name, uint16_t mtu)
:m_device_name(device_name)
,m_mtu(mtu)
{
}
// -------------------------------------------------------------------------------------------------------------------
uint8_t BLEServer::AddService(uint16_t uuid)
{
    LOGI(m_device_name.c_str(), "Adding service %04x.", uuid);
    uint8_t service_id = (uint8_t)m_services.size();
    if (service_id > 32) // GATT_MAX_SR_PROFILES)
    {
        LOGE(
            m_device_name.c_str(),
            "Cannot add more then %d services! Change GATT_MAX_SR_PROFILES in bt_target.h to be able to add more services.",
            32
        );
    }
    m_services.push_back(BLEService::Create(uuid, service_id));
    return service_id;
}
// -------------------------------------------------------------------------------------------------------------------
BLEService::size_type BLEServer::AddCharacteristic(
        const uint16_t* uuid, const uint8_t* properties,
        uint16_t permissions,
        uint16_t max_length, uint16_t length, uint8_t* value,
        const char* description,
        event_handler_func on_event,
        uint8_t* config_descr,
        uint8_t response
)
{
    if (m_services.empty())
        return BLEService::npos;

    uint8_t service_id = (uint8_t)m_services.size() - 1;

    BLEService::ptr service = m_services[service_id];

    BLEService::size_type idx_attr = service->AddCharacteristic(
        uuid, properties, permissions, max_length, length, value, response
    );

    BLEService::size_type idx_config_descr = config_descr ? service->AddConfigDescription(config_descr) : BLEService::npos;

    if (description)
    {
        service->AddNameDescription(description);
    }

    if (on_event)
    {
        if (idx_attr != BLEService::npos)
        {
            m_event_handlers[CreateHandlerKey(service_id, idx_attr)] = on_event;
        }
        if (idx_config_descr != BLEService::npos)
        {
            m_event_handlers[CreateHandlerKey(service_id, idx_config_descr)] = on_event;
        }
    }

    return idx_attr;
}
// -------------------------------------------------------------------------------------------------------------------
uint16_t BLEServer::GetHandle(uint8_t service_id, uint8_t attribute_index)
{
    if (service_id < m_services.size())
    {
        return m_services[service_id]->GetHandle(attribute_index);
    }
    assert(false);
    return 0;
}
// -------------------------------------------------------------------------------------------------------------------
uint8_t* CreatePassiveAdvertisingData(uint16_t uuid, uint8_t& required_bytes, const std::string& device_name)
{
    required_bytes = 12; // flags = 3, tx power = 3, len + type of Primary UUID = 4, len + type for device_name = 2
    uint8_t dn_size = std::min((uint8_t)device_name.size(), (uint8_t)(31 - required_bytes));
    required_bytes += dn_size;
    uint8_t i = 0;
    uint8_t *raw_adv_data = (uint8_t*)malloc(required_bytes);
    LOGI(device_name.c_str(), "Allocated %d bytes for advertisment data.", required_bytes);
    // flags (3 Byte): 0x02 0x02 FLAGS
    raw_adv_data[i++] = 0x02; raw_adv_data[i++] = 0x01; raw_adv_data[i++] = 0x06; 
    // tx power (3 Byte): 0x02 0x0a TX-VALUE
    raw_adv_data[i++] = 0x02; raw_adv_data[i++] = 0x0a; raw_adv_data[i++] = 0xeb;
    // primary UUID
    raw_adv_data[i++] = 0x03; raw_adv_data[i++] = 0x03;
    raw_adv_data[i++] = (uint8_t)uuid & 0xFF; // LO-Byte
    raw_adv_data[i++] = (uint8_t)(uuid >> 8) & 0xFF; // HI-Byte
    // device name
    raw_adv_data[i++] = dn_size + 1;
    raw_adv_data[i++] = dn_size == device_name.size() ? 0x09 : 0x08; // 9 = full length device name, 8 = shortened device name
    for (uint8_t j = 0; j < dn_size; ++j)
    {
        if (j == device_name.size())
            break;
        raw_adv_data[i++] = device_name[j];
    }
    assert(i == required_bytes);
    return raw_adv_data;
}
// -------------------------------------------------------------------------------------------------------------------
uint8_t* CreateScanAdvertisingData(const ServiceVector& services, uint8_t& required_bytes)
{
    assert(services.size() > 1);
    uint8_t uuid_cnt = (uint8_t)services.size() - 1;

    required_bytes = 2; // len + type of Secondary UUIDs = 2
    uuid_cnt = std::min((uint8_t)14, uuid_cnt);
    required_bytes += uuid_cnt * 2;

    uint8_t i = 0;
    uint8_t *raw_adv_data = (uint8_t*)malloc(required_bytes);
    LOGI("SVC", "Allocated %d bytes for active scan advertisment data.", required_bytes);
    // list of uuids
    raw_adv_data[i++] = 1 + 2 * uuid_cnt; // length bit, 1=flag + 2 byte per UUID
    raw_adv_data[i++] = 0x03; // flag

    for (uint8_t j = 1; j <= uuid_cnt; ++j)
    {
        uint16_t uuid = services[j]->GetUUID();
        raw_adv_data[i++] = (uint8_t)uuid & 0xFF; // LO-Byte
        raw_adv_data[i++] = (uint8_t)(uuid >> 8) & 0xFF; // HI-Byte
    }
    return raw_adv_data;
}
// -------------------------------------------------------------------------------------------------------------------
void BLEServer::HandleGATTEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    LOGI(m_device_name.c_str(), "GATT profile event=%d, gatts_if=%d", event, gatts_if);
    if (gatts_if == m_gatts_if || gatts_if == ESP_GATT_IF_NONE || event == ESP_GATTS_REG_EVT)
    {
        switch (event)
        {
            case ESP_GATTS_CREAT_ATTR_TAB_EVT:
                OnAttributesTableCreated(param);
                break;
            case ESP_GATTS_CONF_EVT:
            case ESP_GATTS_RESPONSE_EVT:
            case ESP_GATTS_READ_EVT:
            case ESP_GATTS_WRITE_EVT:
                OnEvent(event, gatts_if, param);
                break;
            case ESP_GATTS_MTU_EVT:
                m_mtu = param->mtu.mtu; 
                break;
            case ESP_GATTS_CONNECT_EVT:
                OnConnect(param);
                break;
            case ESP_GATTS_DISCONNECT_EVT:
                esp_ble_gap_start_advertising(&adv_params);
                break;
            case ESP_GATTS_REG_EVT:
                LOGI(m_device_name.c_str(), "ESP_GATTS_REG_EVT, gatts_if = %d", gatts_if);
                if (gatts_if != ESP_GATT_IF_NONE)
                {
                    m_gatts_if = gatts_if;
                }
                OnRegisterAttributes(gatts_if, param);
                break;
            case ESP_GATTS_UNREG_EVT:
                LOGI(m_device_name.c_str(), "ESP_GATTS_UNREG_EVT, gatts_if = %d", gatts_if);
                m_gatts_if = ESP_GATT_IF_NONE;
                break;
            case ESP_GATTS_EXEC_WRITE_EVT:
            case ESP_GATTS_START_EVT:
            case ESP_GATTS_STOP_EVT:
            case ESP_GATTS_OPEN_EVT:
            case ESP_GATTS_CANCEL_OPEN_EVT:
            case ESP_GATTS_CLOSE_EVT:
            case ESP_GATTS_LISTEN_EVT:
            case ESP_GATTS_CONGEST_EVT:
            case ESP_GATTS_DELETE_EVT:
            default:
                break;
        }
    }
}
// -------------------------------------------------------------------------------------------------------------------
void BLEServer::OnAttributesTableCreated(esp_ble_gatts_cb_param_t *param)
{
    if (param->add_attr_tab.status != ESP_GATT_OK)
    {
        LOGE(m_device_name.c_str(), "Attribute table creation failed, error code=0x%x", param->add_attr_tab.status);
        return;
    }

    uint8_t service_id = param->add_attr_tab.svc_inst_id;
    if (service_id >= m_services.size())
    {
        LOGW(m_device_name.c_str(), "Received attribute table creation event for unknown service id %d, ignored.", service_id);
        return;
    }

    BLEService::ptr service = m_services[service_id];
    
    if (param->add_attr_tab.num_handle != service->GetCount())
    {
        LOGE(
            m_device_name.c_str(),
            "Attribute table created abnormally for service %d, got %d handles, expected %d",
            service_id, param->add_attr_tab.num_handle, service->GetCount()
        );
        return;
    }

    service->SetHandles(param->add_attr_tab.handles, param->add_attr_tab.num_handle);

    LOGI(m_device_name.c_str(), "Attribute table successfully created for service %d, handles=%d", service_id, param->add_attr_tab.num_handle);

    for (uint16_t i = 0; i < param->add_attr_tab.num_handle; ++i)
    {
        uint16_t map_index = CreateHandlerKey(service_id, i);
        uint16_t hdl = param->add_attr_tab.handles[i];
        
        
        auto it = m_event_handlers.find(map_index);
        if (it != m_event_handlers.end())
        {
            // yes, remove the old index and store it with handle as key

            LOGI(m_device_name.c_str(), "Exchanged event handler for service %d / attribute %d by handle %d", service_id, i, hdl);
            m_event_handlers[hdl] = it->second;
            m_event_handlers.erase(it);
        }
    }

    // at least start the service
    LOGI(m_device_name.c_str(), "Starting service %d with uuid=%04x", service_id, service->GetUUID());
    esp_ble_gatts_start_service(*param->add_attr_tab.handles);
}
// -------------------------------------------------------------------------------------------------------------------
void BLEServer::OnRegisterAttributes(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param)
{
    esp_err_t ec = esp_ble_gap_set_device_name(m_device_name.c_str());
    if (ec)
        LOGE(m_device_name.c_str(), "Setting device name failed, error code=%d", ec);
    
    if (m_services.empty())
    {
        LOGE(m_device_name.c_str(), "No services registered!");
        return;
    }

    uint8_t adv_data_size = 0;
    uint8_t* raw_adv_data = CreatePassiveAdvertisingData(m_services[0]->GetUUID(), adv_data_size, m_device_name);
    assert(raw_adv_data);
    assert(adv_data_size);

    LOGI(m_device_name.c_str(), "Advertisment (passive) with size %u created:", adv_data_size);
    LOGDUMP(m_device_name.c_str(), raw_adv_data, adv_data_size, ESP_LOG_DEBUG);

    ec = esp_ble_gap_config_adv_data_raw(raw_adv_data, adv_data_size);
    if (ec)
        LOGE(m_device_name.c_str(), "Failed to set advertisment data config, error code=%d", ec);
    free(raw_adv_data);

    m_adv_config_done |= ADV_CONFIG_FLAG;

    if (m_services.size() > 1)
    {
        adv_data_size = 0;
        uint8_t *raw_adv_scan_data = CreateScanAdvertisingData(m_services, adv_data_size);
        assert(raw_adv_scan_data);
        assert(adv_data_size);

        LOGD(m_device_name.c_str(), "Advertisment (scan response) with size %u created:", adv_data_size);
        LOGDUMP(m_device_name.c_str(), raw_adv_scan_data, adv_data_size, ESP_LOG_DEBUG);

        ec = esp_ble_gap_config_scan_rsp_data_raw(raw_adv_scan_data, adv_data_size);
        if (ec)
            LOGE(m_device_name.c_str(), "Failed to set scan response data config, error code=%d", ec);

        m_adv_config_done |= SCAN_RSP_CONFIG_FLAG;
        free(raw_adv_scan_data);
    }

    for (auto service:m_services)
    {
        service->RegisterAttributes(gatts_if);
    }
}
// -------------------------------------------------------------------------------------------------------------------
void BLEServer::OnConnect(esp_ble_gatts_cb_param_t* param)
{
    LOGI(m_device_name.c_str(), "New device connected, conn_id=%d:", param->connect.conn_id);
    LOGDUMP(m_device_name.c_str(), param->connect.remote_bda, sizeof(param->connect.remote_bda), ESP_LOG_DEBUG);
    esp_ble_conn_update_params_t conn_params;
    memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
    conn_params.latency = 0;
    conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
    conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
    conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
    //start sent the update connection parameters to the peer device.
    esp_ble_gap_update_conn_params(&conn_params);
}
// -------------------------------------------------------------------------------------------------------------------
void BLEServer::OnEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    uint16_t handle = 0;
    switch (event)
    {
        case ESP_GATTS_READ_EVT:
            handle = param->read.handle;
            break;
        case ESP_GATTS_WRITE_EVT:
            handle = param->write.handle;
            LOGI(m_device_name.c_str(), "Received from peer:");
            LOGDUMP(m_device_name.c_str(), param->write.value, param->write.len, ESP_LOG_INFO);
            break;
        case ESP_GATTS_CONF_EVT:
            handle = param->conf.handle;
            break;
        case ESP_GATTS_RESPONSE_EVT:
            handle = param->rsp.handle;
            break;
        default:
            return;
    }

    assert(handle);

    LOGI(m_device_name.c_str(), "OnEvent %d for handle=%d.", event, handle);

    auto itp = m_event_handlers.find(handle);
    if (itp != m_event_handlers.end())
    {
        LOGI(m_device_name.c_str(), "Calling event handler for handle = %d", handle);
        itp->second(event, gatts_if, param);
    }
}
// -------------------------------------------------------------------------------------------------------------------
esp_ble_adv_params_t BLEServer::adv_params = {
    .adv_int_min         = 0x20,
    .adv_int_max         = 0x40,
    .adv_type            = ADV_TYPE_IND,
    .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr           = {0},
    .peer_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map         = ADV_CHNL_ALL,
    .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};
// -------------------------------------------------------------------------------------------------------------------
void BLEServer::HandleGAPEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    LOGI(m_device_name.c_str(), "GAPEvent=%d", event);
    switch (event)
    {
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
            m_adv_config_done &= (~ADV_CONFIG_FLAG);
            if (m_adv_config_done == 0)
            {
                LOGI(m_device_name.c_str(), "Start advertising");
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
            m_adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
            if (m_adv_config_done == 0)
            {
                LOGI(m_device_name.c_str(), "Start advertising (scan response)");
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            /* advertising start complete event to indicate advertising start successfully or failed */
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                LOGE(m_device_name.c_str(), "Advertising start failed.");
            }else{
                LOGI(m_device_name.c_str(), "Advertising successfully started.");
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                LOGE(m_device_name.c_str(), "Advertising stop failed");
            }
            else {
                LOGI(m_device_name.c_str(), "Stop adv successfully\n");
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            LOGI(m_device_name.c_str(), "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
            break;
        default:
            break;
    }
}
// -------------------------------------------------------------------------------------------------------------------
