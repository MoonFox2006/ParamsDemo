//#define USE_APA106

#include <stdio.h>
#include <esp_err.h>
#include <nvs_flash.h>
#include <esp_sleep.h>
#ifdef USE_APA106
#include <soc/gpio_reg.h>
#include <esp_cpu.h>
#endif
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "Parameters.h"

#ifndef F_CPU
#define F_CPU   (CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1000000U)
#endif

constexpr gpio_num_t LED_GPIO = GPIO_NUM_18;

struct __attribute__((__packed__)) config_t {
    char wifi_ssid[32];
    char wifi_pswd[32];
    char ntp_setver[32];
    int8_t ntp_tz;
    uint16_t ntp_period;
};

Parameters<config_t> config;

#ifdef USE_APA106
static void IRAM_ATTR apaRGB(gpio_num_t pin, uint8_t r, uint8_t g, uint8_t b) {
    constexpr uint32_t APA_RESET = 50; // 50 us
    constexpr uint32_t APA_LONG = (uint64_t)F_CPU * 1360 / 1000000000U; // 1.36 us
    constexpr uint32_t APA_SHORT = (uint64_t)F_CPU * 350 / 1000000000U; // 0.35 us

    uint32_t rgb = (b << 16) | (g << 8) | r;
    uint32_t start;

    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    WRITE_PERI_REG(GPIO_OUT_W1TC_REG, 1 << pin); // gpio_set_level(pin, 0);
    esp_rom_delay_us(APA_RESET);
    vPortEnterCritical();
    for (uint8_t i = 0; i < 24; ++i) {
        start = esp_cpu_get_cycle_count();
        WRITE_PERI_REG(GPIO_OUT_W1TS_REG, 1 << pin); // gpio_set_level(pin, 1);
        if (rgb & 0x01) {
            while (esp_cpu_get_cycle_count() - start < APA_LONG) {}
            start = esp_cpu_get_cycle_count();
            WRITE_PERI_REG(GPIO_OUT_W1TC_REG, 1 << pin); // gpio_set_level(pin, 0);
            while (esp_cpu_get_cycle_count() - start < APA_SHORT) {}
        } else {
            while (esp_cpu_get_cycle_count() - start < APA_SHORT) {}
            start = esp_cpu_get_cycle_count();
            WRITE_PERI_REG(GPIO_OUT_W1TC_REG, 1 << pin); // gpio_set_level(pin, 0);
            while (esp_cpu_get_cycle_count() - start < APA_LONG) {}
        }
        rgb >>= 1;
    }
    vPortExitCritical();
}
#endif

extern "C" void app_main() {
    esp_err_t err;

    err = nvs_flash_init();
    if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    config.onClear([](config_t *cfg) {
        strlcpy(cfg->wifi_ssid, "SSID", sizeof(config_t::wifi_ssid));
        strlcpy(cfg->wifi_pswd, "PSWD", sizeof(config_t::wifi_pswd));
        strlcpy(cfg->ntp_setver, "pool.ntp.org", sizeof(config_t::ntp_setver));
        cfg->ntp_tz = 3;
        cfg->ntp_period = 30;
    });
    if (! config.begin()) {
        printf("Parameters init error!\n");
        fflush(stdout);
        esp_deep_sleep_start();
    }

    printf("WiFi SSID\t\"%s\"\n", config->wifi_ssid);
    printf("WiFi password\t\"%s\"\n", config->wifi_pswd);
    printf("NTP server\t\"%s\"\n", config->ntp_setver);
    printf("NTP timezone\tGMT%s%d\n", config->ntp_tz > 0 ? "+" : "", config->ntp_tz);
    printf("NTP update period\t%u sec.\n", config->ntp_period);

    config->ntp_period += 30;
    if (! config) {
        if (! config.commit())
            printf("Parameters commit error!");
    }

#ifdef USE_APA106
    for (uint16_t r = 0; r <= 255; r = (r << 1) | 1) {
        apaRGB(LED_GPIO, r, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    for (uint16_t g = 0; g <= 255; g = (g << 1) | 1) {
        apaRGB(LED_GPIO, 0, g, 0);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    for (uint16_t b = 0; b <= 255; b = (b << 1) | 1) {
        apaRGB(LED_GPIO, 0, 0, b);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    for (uint16_t w = 0; w <= 255; w = (w << 1) | 1) {
        apaRGB(LED_GPIO, w, w, w);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    apaRGB(LED_GPIO, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
#else
    {
        const gpio_config_t cfg = {
            .pin_bit_mask = BIT(LED_GPIO),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };

        gpio_config(&cfg);
    }
#endif

    for (;;) {
#ifdef USE_APA106
        apaRGB(LED_GPIO, random() % 128, random() % 128, random() % 128);
        vTaskDelay(pdMS_TO_TICKS(500));
#else
        gpio_set_level(LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(500 - 50));
#endif
    }
}
