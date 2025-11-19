#include "painel.h"
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

// Headers C da U8g2 / HAL
extern "C" {
    #include "u8g2_esp32_hal.h"
    #include "u8g2.h"
    #include "u8x8.h"
}

// Handle global para o display
u8g2_t u8g2;

static const char *TAG = "stopwatch";

// =====================================================
//  DISPLAY OLED - INICIALIZAÇÃO
// =====================================================
void setup_oled()
{
    u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
    
    // Ajusta os pinos de I2C
    u8g2_esp32_hal.bus.i2c.sda = I2C_SDA_PIN;
    u8g2_esp32_hal.bus.i2c.scl = I2C_SCL_PIN;
    
    // Inicializa HAL da U8g2
    u8g2_esp32_hal_init(u8g2_esp32_hal);

    // Configura o SSD1306 128x64 via I2C
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &u8g2,
        U8G2_R0,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb
    );

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);

    ESP_LOGI(TAG, "Display OLED inicializado.");
}

// =====================================================
//  GPIO (LED + BOTÃO)
// =====================================================
void setup_gpio() 
{
    // LED como saída
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

    // Botão como entrada com pull-up
    gpio_reset_pin(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);

    ESP_LOGI(TAG, "GPIO configurados.");
}

// =====================================================
//  UPDATE DO CRONÔMETRO NA TELA
// =====================================================
void update_display(uint64_t time_us) 
{
    char display_buffer[20];

    uint32_t total_millis   = time_us / 1000;
    uint32_t minutes        = total_millis / 60000;
    uint32_t seconds        = (total_millis / 1000) % 60;
    uint32_t centiseconds   = (total_millis / 10) % 100;
    
    sprintf(display_buffer, "%02lu:%02lu:%02lu",
            (unsigned long)minutes,
            (unsigned long)seconds,
            (unsigned long)centiseconds);

    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_logisoso16_tr);
    u8g2_DrawStr(&u8g2, 22, 40, display_buffer);
    u8g2_SendBuffer(&u8g2);
}

// =====================================================
//  NOVO: MOSTRAR RESULTADO DO ATLETA NO DISPLAY
// =====================================================
void show_athlete_result_on_display(uint8_t number,
                                    const char* name,
                                    float total_time_s)
{
    char linha1[32];
    char linha2[32];

    snprintf(linha1, sizeof(linha1),
             "N:%d  T:%.2fs",
             number,
             (double)total_time_s);

    snprintf(linha2, sizeof(linha2),
             "Nome: %.14s",
             (name != NULL ? name : ""));

    u8g2_ClearBuffer(&u8g2);

    // Título
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&u8g2, 0, 12, "RESULTADO ATLETA");

    // Tempo + número
    u8g2_DrawStr(&u8g2, 0, 30, linha1);

    // Nome
    u8g2_DrawStr(&u8g2, 0, 48, linha2);

    u8g2_SendBuffer(&u8g2);

    ESP_LOGI(TAG,
             "Mostrando resultado: N=%d Nome=%s Tempo=%.2f",
             number, name, (double)total_time_s);
}

