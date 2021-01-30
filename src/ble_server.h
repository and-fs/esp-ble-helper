# pragma once
// ------------------------------------------------------------------------------------------
/*
Lightweight C++ BLE Server implementation for use in ESP-IDF Projects.
*/
// ------------------------------------------------------------------------------------------
# include <esp_gatt_defs.h>
# include <esp_gap_ble_api.h>
# include <esp_gatts_api.h>
# include <vector>
# include <map>
# include <memory>
# include <string>
// ------------------------------------------------------------------------------------------
static const uint16_t primary_service_uuid         = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid   = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint16_t character_client_descr_uuid  = ESP_GATT_UUID_CHAR_DESCRIPTION;
static const uint8_t char_prop_notify              = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_read                = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_write               = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_read_write          = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_read_notify         = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_read_write_notify   = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_write_norsp         = ESP_GATT_CHAR_PROP_BIT_WRITE|ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
// ------------------------------------------------------------------------------------------
typedef std::vector<esp_gatts_attr_db_t> AttrVector;
// ------------------------------------------------------------------------------------------
class BLEService
{
public:
    typedef uint8_t size_type;
    typedef std::shared_ptr<BLEService> ptr;

    /// Creates a new instance with the given \a uuid using \a service_id as ID for this service.
    /// The returned value is a \c shared_pointer to this instance.
    static ptr Create(uint16_t uuid, uint8_t service_id);

    /// Adds an attribute defined by \a attr.
    /// \returns ID of attribute value currently added (or \c npos in case of error)
    size_type AddAttributeDB(const esp_gatts_attr_db_t& attr);

    /// Adds an attribute defined by \a attr, response type is AUTO.
    /// \returns ID of attribute value currently added (or \c npos in case of error)
    size_type AddAttribute(const esp_attr_desc_t& attr, uint8_t response=ESP_GATT_AUTO_RSP);

    /// Adds a description attribute (0x2901).
    /// \returns ID of attribute value currently added (or \c npos in case of error)
    size_type AddNameDescription(const char* description);

    /// Adds a configuration description attribute (0x2902).
    /// \returns ID of attribute value currently added (or \c npos in case of error)
    size_type AddConfigDescription(uint8_t* config_descr);

    /// Sets a new characteristic followed by its value.
    /// \param uuid Pointer to a static 2-Byte UUID of the value.
    /// \param properties Pointer to the static property flags (e.g. ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ)
    /// \param permissions Permissions for value access (R, W, R+W)
    /// \param max_length Maximum allowed value buffer length
    /// \param length Current length of buffer pointer to by \a value
    /// \param value Pointer to the static buffer of current value.
    /// \returns ID of characteristic value currently added (or \c npos in case of error)
    size_type AddCharacteristic(
        const uint16_t* uuid, const uint8_t* properties,
        uint16_t permissions,
        uint16_t max_length, uint16_t length, uint8_t* value,
        uint8_t response=ESP_GATT_AUTO_RSP
    );

    /// Returns a pointer to the GATT structure.
    //const esp_gatts_attr_db_t* GetAttributes(void) const;

    /// Returns the primary UUID of this attribute table.
    uint16_t GetUUID(void) const { return m_uuid; }

    /// Current count of all attributes.
    size_type GetCount(void) const { return (size_type)m_gatt_db.size(); }

    /// ID of service
    uint8_t GetID(void) const { return m_service_id; }

    /// Value for "not found"
    static const size_type npos = (size_type)-1;

    void RegisterAttributes(esp_gatt_if_t gatts_if);

    uint16_t GetHandle(uint8_t index);
    void SetHandles(uint16_t* handles, uint8_t count);

protected:
    /// Service-ID this instance is using
    uint16_t m_uuid;
    uint8_t m_service_id;
    AttrVector m_gatt_db;
    std::vector<uint16_t> m_handles;
    BLEService(uint16_t uuid, uint8_t service_id);
};
// ------------------------------------------------------------------------------------------
/// Type of handler function for read access to an attribute.
/// Params are interface number and read parameter.
typedef void (*event_handler_func)(esp_gatts_cb_event_t, esp_gatt_if_t, esp_ble_gatts_cb_param_t*);
// ------------------------------------------------------------------------------------------
typedef std::vector<BLEService::ptr> ServiceVector;
// ------------------------------------------------------------------------------------------
class BLEServer
{
protected:
    /// Mapping of event handler functions.
    /// Initially when registering attributes, the key is constructed from
    /// the service instance id and the attribute index within this service.
    /// As soon as the OnRegisterAttributes() method was successfully called,
    /// the key is the global handle of the attribute.
    std::map<uint16_t, event_handler_func> m_event_handlers;

    /// Vector of service pointers.
    ServiceVector m_services;

    /// Name of the device which this server represents.
    std::string m_device_name;

    uint8_t m_adv_config_done = 0;

    /// Maximum transfer unit size. Data blocks should not
    /// exceed this.
    uint16_t m_mtu = 500;

    /// Global advertising parameters. 
    static esp_ble_adv_params_t adv_params;

    /// Interface for this instance.
    esp_gatt_if_t m_gatts_if;

    void OnAttributesTableCreated(esp_ble_gatts_cb_param_t *param);
    void OnRegisterAttributes(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param);
    void OnConnect(esp_ble_gatts_cb_param_t* param);
    void OnEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
    void OnExecWrite(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
    void OnPrepareWrite(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
public:
    /// Create a server instance with given device name.
    /// \param mtu The initial maximum transfer unit set. May change when a client is connected,
    ///            so always use GetMTU().
    BLEServer(const char* device_name, uint16_t mtu = 500);

    /// Adds a new characteristic followed by its value and an optional \a description.
    /// \param uuid Pointer to a static 2-Byte UUID of the value.
    /// \param properties Pointer to the static property flags (e.g. ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ)
    /// \param permissions Permissions for value access (R, W, R+W)
    /// \param max_length Maximum allowed value buffer length
    /// \param length Current length of buffer pointer to by \a value
    /// \param value Pointer to the static buffer of current value.
    /// \param description Optional description string.
    /// \param on_event Optional pointer to a function to be called when an event for this attribute is raised.
    /// \param config_descr Optional configuration description value. If set, this adds an 0x2902 Attrbi
    /// \returns ID of characteristic value (!) currently added (or \c npos in case of error)
    ///     This is not the ID of the UUID charactistic, but this of its value.
    BLEService::size_type AddCharacteristic(
        const uint16_t* uuid, const uint8_t* properties,
        uint16_t permissions,
        uint16_t max_length, uint16_t length, uint8_t* value,
        const char* description = nullptr,
        event_handler_func on_event = nullptr,
        uint8_t* config_descr = nullptr,
        uint8_t response = ESP_GATT_AUTO_RSP
    );

    /// Adds a service \a uuid.
    /// All characteristics added using AddCharacteristic() are for this service now, until
    /// a new one follows.
    uint8_t AddService(uint16_t uuid);

    /// Gets handle of attribute with index \a attribute_index registered at service \a service_id.
    uint16_t GetHandle(uint8_t service_id, uint8_t attribute_index);

    /// Returns the current maximum transfer unit. After beeing connected to a client this value
    /// may be changed.
    uint16_t GetMTU(void) const { return m_mtu; }

    /// Event handler to be called for GATT events.
    void HandleGATTEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

    /// Event handler to be called for GAP events.
    void HandleGAPEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
};