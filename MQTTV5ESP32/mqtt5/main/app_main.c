#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "esp_netif.h"
#include "esp_crt_bundle.h"
#include "lwip/dns.h"

#define WIFI_SSID      "TP-Link_68CC"     // Tu red Wi-Fi 2.4 GHz
#define WIFI_PASS      "36415892"         // Contraseña

// ---------- Configuración de Adafruit IO ----------
#define MQTT_BROKER_URI "mqtts://io.adafruit.com:8883"
#define MQTT_USERNAME   "JEV2"
#define MQTT_PASSWORD   "YOUR_ADAFRUIT_IO_KEY" // 🔒 reemplazar al compilar
//#define MQTT_PASSWORD   "aio_..."

// ---------- Feeds ----------
#define MQTT_TOPIC_TEMP "JEV2/feeds/temperatura"
#define MQTT_TOPIC_GPS  "JEV2/feeds/location"

static const char *TAG = "MQTT_APP";
static esp_mqtt_client_handle_t client = NULL;

/* ================== Wi-Fi ================== */
static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    ESP_LOGI(TAG, "Conectando a la red Wi-Fi: %s ...", WIFI_SSID);
}

/* ================== MQTT Event Handler ================== */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    client = event->client;

    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "✅ Conectado a Adafruit IO (MQTT 3.1.1)");
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "⚠️ Desconectado del broker. Intentando reconectar...");
            esp_mqtt_client_reconnect(client);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "❌ Error MQTT");
            break;

        default:
            break;
    }
}

/* ================== MQTT Start ================== */
static void mqtt_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
        .credentials.client_id = "ESP32_JEV2",
        .session.protocol_ver = MQTT_PROTOCOL_V_3_1_1,  // Usar versión 3.1.1
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

/* ================== Publicador ================== */
static void publisher_task(void *pvParameters)
{
    float temp = 27.0;
    int point = 0;

    // Coordenadas simuladas (Guadalajara)
    float lat[] = {20.6736, 20.6748, 20.6761, 20.6774, 20.6802, 20.6840};
    float lon[] = {-103.3440, -103.3525, -103.3612, -103.3689, -103.3755, -103.3801};
    int total_points = sizeof(lat) / sizeof(lat[0]);

    while (1)
    {
        // Incrementar temperatura de forma cíclica
        temp += 0.1f;
        if (temp > 30.0f)
            temp = 27.0f;

        // Publicar temperatura
        char temp_str[16];
        snprintf(temp_str, sizeof(temp_str), "%.2f", temp);
        esp_mqtt_client_publish(client, MQTT_TOPIC_TEMP, temp_str, 0, 1, 0);
        ESP_LOGI(TAG, "🌡️ Publicando temperatura: %s °C", temp_str);

        // Publicar ubicación en formato JSON compatible con el mapa de Adafruit IO
        char gps_json[128];
        snprintf(gps_json, sizeof(gps_json),
                 "{\"lat\": %.4f, \"lon\": %.4f, \"ele\": 0, \"accuracy\": 1}",
                 lat[point], lon[point]);
        esp_mqtt_client_publish(client, MQTT_TOPIC_GPS, gps_json, 0, 1, 0);
        ESP_LOGI(TAG, "📍 Publicando ubicación JSON: %s", gps_json);

        point = (point + 1) % total_points;
        vTaskDelay(pdMS_TO_TICKS(5000)); // Cada 5 segundos
    }
}

/* ================== MAIN ================== */
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();

    // Configurar DNS (Google + Cloudflare)
    ip_addr_t dns1, dns2;
    IP_ADDR4(&dns1, 8, 8, 8, 8);
    IP_ADDR4(&dns2, 1, 1, 1, 1);
    dns_setserver(0, &dns1);
    dns_setserver(1, &dns2);

    mqtt_start();
    xTaskCreate(publisher_task, "publisher_task", 4096, NULL, 5, NULL);
}
