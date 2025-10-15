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

// Kconfig esperados:
// CONFIG_WIFI_SSID (string)
// CONFIG_WIFI_PASSWORD (string)
// CONFIG_TCP_SERVER_PORT (int)

static const char* TAG = "SERVER";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static inline char inv_alpha(char c, int inv) {
    if (c >= 'a' && c <= 'z') return (char)((((c - 'a') + inv) % 26) + 'a');
    if (c >= 'A' && c <= 'Z') return (char)((((c - 'A') + inv) % 26) + 'A');
    return c;
}
static inline char inv_digit(char c, int invd) {
    if (c >= '0' && c <= '9') return (char)((((c - '0') + invd) % 10) + '0');
    return c;
}

static void wifi_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi desconectado, reintentando...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* e = (ip_event_got_ip_t*)data;
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&e->ip_info.ip));
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

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wc = {0};
    strncpy((char*)wc.sta.ssid, CONFIG_WIFI_SSID, sizeof(wc.sta.ssid) - 1);
    strncpy((char*)wc.sta.password, CONFIG_WIFI_PASSWORD, sizeof(wc.sta.password) - 1);
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wc.sta.sae_pwe_h2e = WPA3_SAE_PWE_UNSPECIFIED;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());

    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(TAG, "Conectado a SSID:%s", CONFIG_WIFI_SSID);
}

static void server_task(void* pv) {
    // Espera Wi-Fi
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    // socket, bind, listen
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { ESP_LOGE(TAG, "socket() errno=%d", errno); vTaskDelete(NULL); return; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(CONFIG_TCP_SERVER_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        ESP_LOGE(TAG, "bind() errno=%d", errno);
        close(srv); vTaskDelete(NULL); return;
    }
    if (listen(srv, 1) != 0) {
        ESP_LOGE(TAG, "listen() errno=%d", errno);
        close(srv); vTaskDelete(NULL); return;
    }
    ESP_LOGI(TAG, "Escuchando en :%d ...", CONFIG_TCP_SERVER_PORT);

    for (;;) {
        struct sockaddr_in cli; socklen_t cl = sizeof(cli);
        int cfd = accept(srv, (struct sockaddr*)&cli, &cl);
        if (cfd < 0) { ESP_LOGW(TAG, "accept errno=%d", errno); continue; }

        char cip[16]; inet_ntop(AF_INET, &cli.sin_addr, cip, sizeof(cip));
        ESP_LOGI(TAG, "Conexión de %s:%d", cip, (int)ntohs(cli.sin_port));

        uint8_t buf[256];
        ssize_t r = recv(cfd, buf, sizeof(buf), 0);
        if (r <= 0) {
            ESP_LOGW(TAG, "recv=%d errno=%d", (int)r, errno);
            close(cfd); continue;
        }

        // Descifrar: buf[0]=shift; resto=texto
        uint8_t sh = buf[0] % 26;
        int inv   = (26 - sh) % 26;
        int invd  = (10 - (sh % 10)) % 10;

        char out[256]; size_t w = 0;
        for (ssize_t i = 1; i < r && w < sizeof(out)-1; ++i) {
            char c = (char)buf[i];
            if      (c >= 'a' && c <= 'z') c = inv_alpha(c, inv);
            else if (c >= 'A' && c <= 'Z') c = inv_alpha(c, inv);
            else if (c >= '0' && c <= '9') c = inv_digit(c, invd);
            out[w++] = c;
        }
        out[w] = '\0';
        ESP_LOGI(TAG, "shift=%u, descifrado=\"%s\"", (unsigned)sh, out);

        // Eco: reenvía el mismo paquete recibido
        send(cfd, buf, r, 0);
        close(cfd);
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_sta();
    xTaskCreate(server_task, "server_task", 4096, NULL, 5, NULL);
}
