#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "driver/uart.h"
#include "driver/ledc.h"

// ============================
// TAG
// ============================
static const char *TAG = "MAIN";

// ============================================================
// COLAS PARA SEÑALAR NUEVA LISTA
// ============================================================
QueueHandle_t CH1_queue;
QueueHandle_t CH2_queue;
QueueHandle_t CH3_queue;

// ============================================================
// ESTRUCTURA PWM WRAPPER
// ============================================================
typedef struct {
    char name_id[8];
    ledc_timer_config_t  timer;
    ledc_channel_config_t channel;
    QueueHandle_t queue;
} pwm_wrapper_t;

// ============================================================
// SECUENCIA DE VALORES POR CANAL (CICLADO INFINITO)
// ============================================================
#define MAX_VALUES 32

typedef struct {
    int list[MAX_VALUES];
    int count;
} duty_sequence_t;

duty_sequence_t seq_CH1 = { .count = 0 };
duty_sequence_t seq_CH2 = { .count = 0 };
duty_sequence_t seq_CH3 = { .count = 0 };

// ============================================================
// PROTOTIPOS
// ============================================================
void pwm_task(void *pvParameters);
void uart_task(void *pvParameters);
void parse_uart_command(char *cmd);

// ============================================================
// PARSER DEL PROTOCOLO SET (GUARDA LISTA Y REINICIA CICLO)
// ============================================================
void parse_uart_command(char *cmd)
{
    char *token;
    char *channel = NULL;
    char *values  = NULL;

    token = strtok(cmd, " ");
    if (!token || strcmp(token, "SET") != 0) {
        ESP_LOGE("UART_PARSER", "Comando inválido, falta SET");
        return;
    }

    channel = strtok(NULL, " ");
    if (!channel) {
        ESP_LOGE("UART_PARSER", "No se especificó canal");
        return;
    }

    values = strtok(NULL, " ");
    if (!values) {
        ESP_LOGE("UART_PARSER", "No se especificaron valores");
        return;
    }

    duty_sequence_t *seq = NULL;
    QueueHandle_t target_q = NULL;

    if (strcmp(channel, "CH1") == 0) { seq = &seq_CH1; target_q = CH1_queue; }
    else if (strcmp(channel, "CH2") == 0) { seq = &seq_CH2; target_q = CH2_queue; }
    else if (strcmp(channel, "CH3") == 0) { seq = &seq_CH3; target_q = CH3_queue; }
    else {
        ESP_LOGE("UART_PARSER", "Canal inválido: %s", channel);
        return;
    }

    seq->count = 0;

    char *val = strtok(values, ",");

    while (val != NULL && seq->count < MAX_VALUES)
    {
        int duty = atoi(val);

        if (duty >= 0 && duty <= 100) {
            seq->list[seq->count++] = duty;
            ESP_LOGI("UART_PARSER", "%s value[%d] = %d%%",
                     channel, seq->count - 1, duty);
        } else {
            ESP_LOGE("UART_PARSER", "Valor fuera de rango: %d", duty);
        }

        val = strtok(NULL, ",");
    }

    int signal = 1;
    xQueueSend(target_q, &signal, 0);

    ESP_LOGI("UART_PARSER", "%s: Nueva lista de %d valores cargada",
             channel, seq->count);
}

// ============================================================
// PWM TASK – CICLADO INFINITO
// ============================================================
void pwm_task(void *pvParameters)
{
    pwm_wrapper_t *conf = (pwm_wrapper_t *)pvParameters;

    int gpio_num = 0;
    int channel_id = 0;

    if (strcmp(conf->name_id, "CH1") == 0) { gpio_num = 12; channel_id = LEDC_CHANNEL_0; }
    if (strcmp(conf->name_id, "CH2") == 0) { gpio_num = 13; channel_id = LEDC_CHANNEL_1; }
    if (strcmp(conf->name_id, "CH3") == 0) { gpio_num = 14; channel_id = LEDC_CHANNEL_2; }

    conf->timer.speed_mode      = LEDC_HIGH_SPEED_MODE;
    conf->timer.duty_resolution = LEDC_TIMER_13_BIT;
    conf->timer.timer_num       = LEDC_TIMER_0;
    conf->timer.freq_hz         = 5000;
    conf->timer.clk_cfg         = LEDC_AUTO_CLK;
    ledc_timer_config(&conf->timer);

    conf->channel.speed_mode = LEDC_HIGH_SPEED_MODE;
    conf->channel.channel    = channel_id;
    conf->channel.gpio_num   = gpio_num;
    conf->channel.timer_sel  = LEDC_TIMER_0;
    conf->channel.intr_type  = LEDC_INTR_DISABLE;
    conf->channel.duty       = 0;
    conf->channel.hpoint     = 0;

    ledc_channel_config(&conf->channel);

    ESP_LOGI("PWM_TASK", "PWM configurado para %s en GPIO %d", conf->name_id, gpio_num);

    int signal;
    int index = 0;

    while (1)
    {
        duty_sequence_t *seq =
            strcmp(conf->name_id, "CH1") == 0 ? &seq_CH1 :
            strcmp(conf->name_id, "CH2") == 0 ? &seq_CH2 :
                                                &seq_CH3;

        if (xQueueReceive(conf->queue, &signal, 0)) {
            ESP_LOGI("PWM_TASK", "%s: Nueva lista recibida, reiniciando ciclo.", conf->name_id);
            index = 0;
        }

        if (seq->count == 0) {
            vTaskDelay(50 / portTICK_PERIOD_MS);
            continue;
        }

        int duty_percent = seq->list[index];

        uint32_t max_duty = (1 << 13);
        uint32_t duty_counts = (duty_percent * max_duty) / 100;

        ledc_set_duty(LEDC_HIGH_SPEED_MODE, conf->channel.channel, duty_counts);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, conf->channel.channel);

        ESP_LOGI("PWM_TASK", "%s → %d%% (%d counts)", conf->name_id, duty_percent, duty_counts);

        index++;
        if (index >= seq->count) index = 0;

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// ============================================================
// UART TASK
// ============================================================
void uart_task(void *pvParameters)
{
    const int RX_BUF = 256;

    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_param_config(UART_NUM_0, &cfg);
    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_0, RX_BUF, 0, 0, NULL, 0);

    ESP_LOGI("UART_TASK", "UART inicializada a 115200");

    char command[256];
    int idx = 0;
    uint8_t data;

    while (1)
    {
        int len = uart_read_bytes(UART_NUM_0, &data, 1, 20 / portTICK_PERIOD_MS);

        if (len > 0)
        {
            char c = (char)data;

            if (c == '\n' || c == '\r')
            {
                command[idx] = '\0';

                if (idx > 0) {
                    ESP_LOGI("UART_TASK", "Comando recibido: %s", command);
                    parse_uart_command(command);
                }

                idx = 0;
            }
            else if (idx < 255)
                command[idx++] = c;
        }
    }
}

// ============================================================
// MAIN
// ============================================================
void app_main(void)
{
    ESP_LOGI(TAG, "Inicializando sistema...");

    static pwm_wrapper_t CH1 = { .name_id = "CH1" };
    static pwm_wrapper_t CH2 = { .name_id = "CH2" };
    static pwm_wrapper_t CH3 = { .name_id = "CH3" };

    CH1_queue = xQueueCreate(4, sizeof(int));
    CH2_queue = xQueueCreate(4, sizeof(int));
    CH3_queue = xQueueCreate(4, sizeof(int));

    CH1.queue = CH1_queue;
    CH2.queue = CH2_queue;
    CH3.queue = CH3_queue;

    xTaskCreate(pwm_task, "PWM_CH1", 4096, &CH1, 2, NULL);
    xTaskCreate(pwm_task, "PWM_CH2", 4096, &CH2, 2, NULL);
    xTaskCreate(pwm_task, "PWM_CH3", 4096, &CH3, 2, NULL);

    xTaskCreate(uart_task, "UART_Task", 4096, NULL, 3, NULL);
}
