#pragma once

#include <string.h>
#include <functional>
#include <esp_log.h>
#include <nvs.h>

template<typename T>
class Parameters {
public:
    typedef std::function<void(T*)> clearcb_t;

    Parameters() : _clearcb(nullptr), _nvs(0) {}
    ~Parameters() {
        end();
    }

    bool begin();
    void end();
    bool check() const;
    operator bool() const {
        return check();
    }
    T* operator ->() {
        return &_blob.data;
    }
    void clear();
    bool commit();
    void onClear(clearcb_t cb) {
        _clearcb = cb;
    }

protected:
    static constexpr uint16_t SIGN = 0xA3C5;

    clearcb_t _clearcb;
    nvs_handle_t _nvs;
    struct __attribute__((__packed__)) paramblob_t {
        uint16_t sign;
        uint16_t crc;
        T data;
    } _blob;
};

#define NVS_NAMESPACE   "Parameters"
#define NVS_BLOBNAME    "Data"

static const char TAG[] = NVS_NAMESPACE;

static uint16_t crc16(uint8_t value, uint16_t crc = 0xFFFF) {
    crc ^= value;
    for (uint8_t i = 0; i < 8; ++i) {
        if (crc & 0x01)
            crc = (crc >> 1) ^ 0xA001;
        else
            crc >>= 1;
    }
    return crc;
}

static uint16_t crc16(const uint8_t *data, uint16_t size, uint16_t crc = 0xFFFF) {
    while (size--) {
        crc = crc16(*data++, crc);
    }
    return crc;
}

template<typename T>
bool Parameters<T>::begin() {
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &_nvs);
    if (err == ESP_OK) {
        size_t size = sizeof(paramblob_t);

        err = nvs_get_blob(_nvs, NVS_BLOBNAME, &_blob, &size);
        if ((err == ESP_OK) || (err == ESP_ERR_NVS_NOT_FOUND)) {
            if ((err == ESP_ERR_NVS_NOT_FOUND) || (! check()))
                clear();
            return true;
        } else
            ESP_LOGE(TAG, "NVS read error %d!", err);
        nvs_close(_nvs);
    } else
        ESP_LOGE(TAG, "NVS open error %d!", err);
    _nvs = 0;
    return false;
}

template<typename T>
void Parameters<T>::end() {
    if (_nvs) {
        if (! check())
            commit();
        nvs_close(_nvs);
        _nvs = 0;
    }
}

template<typename T>
bool Parameters<T>::check() const {
    return (_blob.sign == SIGN) && (_blob.crc == crc16((uint8_t*)&_blob.data, sizeof(T)));
}

template<typename T>
void Parameters<T>::clear() {
    memset(&_blob.data, 0, sizeof(T));
    if (_clearcb)
        _clearcb(&_blob.data);
}

template<typename T>
bool Parameters<T>::commit() {
    if (_nvs) {
        esp_err_t err;

        _blob.sign = SIGN;
        _blob.crc = crc16((uint8_t*)&_blob.data, sizeof(T));
        err = nvs_set_blob(_nvs, NVS_BLOBNAME, &_blob, sizeof(paramblob_t));
        if (err != ESP_OK)
            ESP_LOGE(TAG, "NVS write error %d!", err);
        else
            return true;
    }
    return false;
}
