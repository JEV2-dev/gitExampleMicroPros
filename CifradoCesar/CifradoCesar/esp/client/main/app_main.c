#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

// ====================================================================
// =================== CONFIGURACIÓN DEL USUARIO ======================
// ====================================================================
// Wi-Fi
#define WIFI_SSID      "S25 Ultra de Carlos"
#define WIFI_PASSWORD  "carlos21"

// Servidor TCP (tu PC)
#define SERVER_IP      "10.91.121.104" // <-- ¡IMPORTANTE: PON LA IP DE TU PC AQUÍ!
#define SERVER_PORT    13000
// ====================================================================

static const char* TAG = "CLIENTE_TCP";
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi desconectado, reintentando conectar...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP obtenida: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finalizado.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Conectado al SSID: %s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Fallo al conectar al SSID: %s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "EVENTO INESPERADO");
    }
}

void tcp_client_task(void *pvParameters) {
    char payload[100];
    int sock;

    // Variables de prueba que irán cambiando
    float velocidad_buscada = 2500.0;
    float velocidad_real = 2495.5;
    float voltaje = 4.8;

    while (1) {
        // --- 1. Crear el Socket ---
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(SERVER_PORT);
        
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "No se pudo crear el socket: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(5000)); // Esperar 5 segundos antes de reintentar
            continue;
        }
        ESP_LOGI(TAG, "Socket creado, conectando a %s:%d", SERVER_IP, SERVER_PORT);

        // --- 2. Conectar al servidor ---
        if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in)) != 0) {
            ESP_LOGE(TAG, "Fallo al conectar: errno %d", errno);
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(5000)); // Esperar 5 segundos antes de reintentar
            continue;
        }
        ESP_LOGI(TAG, "Conectado exitosamente al servidor");

        // --- 3. Bucle de envío de datos ---
        while (1) {
            // Formatear la cadena con los datos: "buscada,real,voltaje\n"
            int len = snprintf(payload, sizeof(payload), "%.2f,%.2f,%.2f\n",
                               velocidad_buscada, velocidad_real, voltaje);

            // Enviar los datos
            int err = send(sock, payload, len, 0);
            if (err < 0) {
                ESP_LOGE(TAG, "Error al enviar datos: errno %d", errno);
                break; // Salir del bucle de envío para intentar reconectar
            }
            ESP_LOGI(TAG, "Datos enviados: %s", payload);

            // Simular un cambio en los valores para la próxima iteración
            velocidad_real += 0.5f;
            if (velocidad_real > 2510.0f) velocidad_real = 2495.0f;
            voltaje += 0.01f;
            if (voltaje > 5.0f) voltaje = 4.8f;

            // Esperar 2 segundos antes de enviar el siguiente paquete
            vTaskDelay(pdMS_TO_TICKS(2000));
        }

        // --- 4. Cerrar conexión si hay error ---
        if (sock != -1) {
            ESP_LOGE(TAG, "Cerrando socket y reintentando conexión...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

void app_main(void) {
    // Inicializar NVS (necesario para el Wi-Fi)
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // Iniciar la conexión Wi-Fi
    wifi_init_sta();

    // Crear la tarea que manejará el cliente TCP
    xTaskCreate(tcp_client_task, "tcp_client", 4096, NULL, 5, NULL);
}