#ifndef _PAINEL_H_
#define _PAINEL_H_

#include <stdint.h>
#include "driver/gpio.h"   // Para GPIO_NUM_2, GPIO_NUM_5, etc.

#ifdef __cplusplus
extern "C" {
#endif

#define LED_PIN         GPIO_NUM_2      // LED indica se está rodando
#define BUTTON_PIN      GPIO_NUM_5      // Botão Start/Pause
#define I2C_SDA_PIN     GPIO_NUM_21
#define I2C_SCL_PIN     GPIO_NUM_22

// Inicialização de GPIOs
void setup_gpio(void);

// Inicialização do display
void setup_oled(void);

// Desenha o cronômetro no display
void update_display(uint64_t time_us);

// NOVO: Mostra resultado de atleta no display
void show_athlete_result_on_display(uint8_t number,
                                    const char* name,
                                    float total_time_s);

#ifdef __cplusplus
}
#endif

#endif // _PAINEL_H_
