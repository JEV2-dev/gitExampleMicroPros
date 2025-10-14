// app_main.c — ESP32 TCP client + Cifrado César (payload: [shift][ciphertext])

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

// ---- Kconfig ----
// CONFIG_WIFI_SSID           (string)
// CONFIG_WIFI_PASSWORD       (string)
// CONFIG_TCP_SERVER_IP       (string)
// CONFIG_TCP_SERVER_PORT     (int)
// CONFIG_CAESAR_SHIFT        (int 0..25)
// CONFIG_CAESAR_TEXT         (string)

static const char* TAG = "CLIENT";

// ---- Wi-Fi event bits ----
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// ---- Caesar helpers (A-Z/a-z rot 26; 0-9 rot 10; resto = igual) ----
static inline char rot_alpha(char c, int shift) {
    if (c >= 'a' && c <= 'z') return (char)((((c - 'a') + shift) % 26) + 'a');
    if (c >= 'A' && c <= 'Z') return (char)((((c - 'A') + shift) % 26) + 'A');
    return c;
}
static inline char rot_digit(char c, int shift) {
    if (c >= '0' && c <= '9') return (char)((((c - '0') + (shift % 10)) % 10) + '0');
    return c;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi desconectado, reintentando...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP obtenida: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                    IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = { 0 };
    // Copia segura de SSID/PASS desde Kconfig
    strncpy((char*)wifi_config.sta.ssid, CONFIG_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, CONFIG_WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_UNSPECIFIED;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Espera conexión
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Conectado a SSID:%s", CONFIG_WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "No se pudo conectar a Wi-Fi");
    }
}

// Envía payload [shift][ciphertext]; recibe eco y lo descifra para mostrarlo
static void client_task(void* pv) {
    // 1) Esperar Wi-Fi listo (por si la tarea arranca antes)
    if (s_wifi_event_group) {
        xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    }

    // 2) Crear socket TCP
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        ESP_LOGE(TAG, "socket() falló: errno=%d", errno);
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in dest = {0};
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(CONFIG_TCP_SERVER_PORT);
    if (inet_aton(CONFIG_TCP_SERVER_IP, &dest.sin_addr) == 0) {
        ESP_LOGE(TAG, "IP inválida en CONFIG_TCP_SERVER_IP: %s", CONFIG_TCP_SERVER_IP);
        close(s);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Conectando a %s:%d ...", CONFIG_TCP_SERVER_IP, CONFIG_TCP_SERVER_PORT);
    if (connect(s, (struct sockaddr*)&dest, sizeof(dest)) != 0) {
        ESP_LOGE(TAG, "connect() falló: errno=%d", errno);
        close(s);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Conectado.");

    // 3) Armar payload [shift][ciphertext]
    const char* plain = CONFIG_CAESAR_TEXT;
    uint8_t shift = (uint8_t)(CONFIG_CAESAR_SHIFT % 26);

    uint8_t cipher[128];
    char    echo[128];

    size_t max_payload = sizeof(cipher) - 1;   // 1 byte reservado para shift
    size_t n = strlen(plain);
    if (n > max_payload) n = max_payload;

    cipher[0] = shift;
    for (size_t i = 0; i < n; ++i) {
        char c = plain[i];
        if      (c >= 'a' && c <= 'z') c = rot_alpha(c, shift);
        else if (c >= 'A' && c <= 'Z') c = rot_alpha(c, shift);
        else if (c >= '0' && c <= '9') c = rot_digit(c, shift);
        // resto igual
        cipher[1 + i] = (uint8_t)c;
    }
    size_t to_send = 1 + n;

    // 4) Enviar (maneja envíos parciales)
    size_t sent = 0;
    while (sent < to_send) {
        ssize_t w = send(s, cipher + sent, to_send - sent, 0);
        if (w < 0) {
            ESP_LOGE(TAG, "send() falló: errno=%d", errno);
            close(s);
            vTaskDelete(NULL);
            return;
        }
        sent += (size_t)w;
    }
    ESP_LOGI(TAG, "Enviados %u bytes (shift=%u, texto='%.*s')",
             (unsigned)to_send, (unsigned)shift, (int)n, plain);

    // 5) Recibir eco (opcional) y descifrar para mostrar
    uint8_t rxbuf[128];
    ssize_t r = recv(s, rxbuf, sizeof(rxbuf), 0);
    if (r > 0) {
        // Descifrar eco usando el 1er byte (shift)
        uint8_t sh = rxbuf[0] % 26;
        int inv   = (26 - sh) % 26;
        int inv_d = (10 - (sh % 10)) % 10;

        size_t outcap = sizeof(echo) - 1;
        size_t w = 0;
        for (ssize_t i = 1; i < r && w < outcap; ++i) {
            char c = (char)rxbuf[i];
            if      (c >= 'a' && c <= 'z') c = (char)((((c-'a') + inv) % 26) + 'a');
            else if (c >= 'A' && c <= 'Z') c = (char)((((c-'A') + inv) % 26) + 'A');
            else if (c >= '0' && c <= '9') c = (char)((((c-'0') + inv_d) % 10) + '0');
            echo[w++] = c;
        }
        echo[w] = '\0';
        ESP_LOGI(TAG, "Eco descifrado: %s", echo);
    } else if (r == 0) {
        ESP_LOGW(TAG, "Servidor cerró conexión.");
    } else {
        ESP_LOGE(TAG, "recv() falló: errno=%d", errno);
    }

    close(s);
    ESP_LOGI(TAG, "Socket cerrado. Tarea finalizada.");
    vTaskDelete(NULL);
}

void app_main(void) {
    // NVS para Wi-Fi
    ESP_ERROR_CHECK(nvs_flash_init());
    // En caso de páginas NVS antiguas:
    // if (nvs_flash_init() == ESP_ERR_NVS_NO_FREE_PAGES || nvs_flash_init() == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //     ESP_ERROR_CHECK(nvs_flash_erase());
    //     ESP_ERROR_CHECK(nvs_flash_init());
    // }

    wifi_init_sta();

    // Lanza la tarea cliente
    xTaskCreate(client_task, "client_task", 4096, NULL, 5, NULL);
}
