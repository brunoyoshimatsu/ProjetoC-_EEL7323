#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "athlete.h"
#include "painel.h"
#include "u8g2_esp32_hal.h"

#include "EspNowMaster.h"     
#include "EspNowProtocol.h"   

#include "ResultWriter.h"     

static const char *TAG = "master_stopwatch";

static const uint8_t SLAVE_RAIA_1[6] = { 0x48, 0xE7, 0x29, 0x9A, 0x29, 0xE4 };

static const uint8_t SLAVE_RAIA_2[6] = { 0x68, 0xB6, 0xB3, 0x3E, 0x28, 0x48 };

static const int MAX_ATHLETES = 8;

Athlete* athletes[MAX_ATHLETES] = { nullptr };
int num_athletes_registered = 0;

int option_main  = 0;
int option_config = 0;
int num_athletes = 0;
int num_laps     = 0;

typedef struct {
    uint8_t  mac[6];
    Athlete* athlete;
    int      athlete_index;   
} SlaveAthleteBinding;

static SlaveAthleteBinding bindings[MAX_ATHLETES];
static int num_bindings = 0;

static bool mac_equals(const uint8_t* a, const uint8_t* b)
{
    return (memcmp(a, b, 6) == 0);
}

static Athlete* find_athlete_by_mac(const uint8_t* mac)
{
    for (int i = 0; i < num_bindings; ++i) {
        if (mac_equals(mac, bindings[i].mac)) {
            return bindings[i].athlete;
        }
    }
    return nullptr; 
}

static RaceResult race_results[MAX_ATHLETES];

#define LED_PIN         GPIO_NUM_2      
#define BUTTON_PIN      GPIO_NUM_5      
#define I2C_SDA_PIN     GPIO_NUM_21     
#define I2C_SCL_PIN     GPIO_NUM_22     

static bool     timerRunning              = false;  
static uint64_t elapsed_time_at_pause_us  = 0;      
static uint64_t start_time_us             = 0;      

static bool raceFinished = false;                          
static int  results_received_count = 0;                    
static bool binding_finished[MAX_ATHLETES] = { false };    

void show_athlete_result_on_display(uint8_t number, const char* name, float total_time_s);

EspNowMaster espMaster;

static void stop_race_due_to_all_results()
{
    if (raceFinished) {
        return;
    }

    raceFinished = true;

    if (timerRunning) {
        elapsed_time_at_pause_us = esp_timer_get_time() - start_time_us;
        timerRunning = false;
    }

    gpio_set_level(LED_PIN, 0);

    ESP_LOGI(TAG,
             "Todos os atletas finalizaram. Cronômetro travado em %.3f s",
             (double)elapsed_time_at_pause_us / 1000000.0);
}

void handle_results_received(const uint8_t* mac_addr, const DadosResultado& data)
{
    ESP_LOGW(TAG,
             "DADOS DE RESULTADO RECEBIDOS DO SLAVE: %02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5]);

    Athlete* athlete = find_athlete_by_mac(mac_addr);

    if (athlete != nullptr) {
        ESP_LOGI(TAG, "Resultado do atleta '%s' (número %d)",
                 athlete->getName(),
                 athlete->getNumber());
    } else {
        ESP_LOGW(TAG, "Nenhum atleta associado a esse slave!");
    }

    ESP_LOGI(TAG, "--- RESULTADO (Slave %02X:%02X) ---", mac_addr[4], mac_addr[5]);
    ESP_LOGI(TAG, "Tempo Total: %lld us (%.2f s)",
             data.tempoTotal_us, (double)data.tempoTotal_us / 1000000.0);

    for (int i = 0; i < data.numVoltas && i < 20; i++) {
        ESP_LOGI(TAG, "  Parcial %d: %lld us (%.2f s)",
                 i + 1, data.temposParciais_us[i],
                 (double)data.temposParciais_us[i] / 1000000.0);
    }
    ESP_LOGI(TAG, "------------------------------------");
    ESP_LOGI(TAG, "\nPronto para a próxima. Pressione Enter para ver o menu.");
    fflush(stdout); 

    for (int i = 0; i < num_bindings; ++i) {
        if (mac_equals(mac_addr, bindings[i].mac)) {

            int idx = bindings[i].athlete_index;
            if (idx >= 0 && idx < MAX_ATHLETES) {
                float total_s = (float)data.tempoTotal_us / 1000000.0f;
                race_results[idx].hasResult    = true;
                race_results[idx].total_time_s = total_s;
                ESP_LOGI(TAG,
                         "Guardado tempo do atleta idx=%d -> %.3f s",
                         idx, (double)total_s);
            }

            if (!binding_finished[i]) {
                binding_finished[i] = true;
                results_received_count++;

                ESP_LOGI(TAG,
                         "Resultados recebidos: %d/%d",
                         results_received_count, num_bindings);

                if (results_received_count >= num_bindings && num_bindings > 0) {
                    stop_race_due_to_all_results();
                }
            } else {
                ESP_LOGW(TAG,
                         "Resultado repetido do mesmo slave, ignorando para contagem.");
            }

            break;
        }
    }
}

static bool read_int_from_stdin(const char* prompt, int* out_value)
{
    char line[64];

    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }

    while (1) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (sscanf(line, "%d", out_value) == 1) {
            return true;
        }

        printf("Entrada inválida. Digite novamente.\n");
        if (prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }
    }

    return false;
}

static bool read_line_from_stdin(const char* prompt, char* buffer, size_t buflen)
{
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }

    size_t idx = 0;
    buffer[0] = '\0';

    while (1) {
        int c = getchar();

        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (c == '\n' || c == '\r') {
            if (idx == 0) {
                printf("Entrada vazia. Digite novamente.\n");
                if (prompt) printf("%s", prompt);
                fflush(stdout);
                continue;
            }
            buffer[idx] = '\0';
            return true;
        }

        if (idx < buflen - 1) {
            buffer[idx++] = (char)c;
        }
    }
}

static void run_menu()
{
    ESP_LOGI(TAG, "Entrando no MENU (configuração da corrida)");

    while (1)
    {
        printf("\n=== MENU PRINCIPAL ===\n");
        printf("[1] Configurar parâmetros da corrida\n");
        printf("[2] Inscrever atletas na corrida\n");
        printf("[3] Iniciar corrida\n");

        read_int_from_stdin("Escolha uma opção: ", &option_main);

        switch (option_main)
        {
            case 1:
            {
                bool sair_config = false;

                while (!sair_config) {
                    printf("\n=== CONFIGURAÇÃO DA CORRIDA ===\n");
                    printf("[1] Selecionar o número de atletas (atual: %d)\n", num_athletes);
                    printf("[2] Selecionar o número de voltas da corrida (atual: %d)\n", num_laps);
                    printf("[3] Voltar ao menu principal\n");

                    read_int_from_stdin("Escolha uma opção: ", &option_config);

                    switch (option_config)
                    {
                        case 1:
                            read_int_from_stdin(
                                "Digite o número de atletas: ",
                                &num_athletes
                            );

                            if (num_athletes > MAX_ATHLETES) {
                                printf("Limitando para %d atletas.\n", MAX_ATHLETES);
                                num_athletes = MAX_ATHLETES;
                            }
                            if (num_athletes < 0) {
                                num_athletes = 0;
                            }

                            printf("Número de atletas configurado: %d\n", num_athletes);
                            break;

                        case 2:
                            read_int_from_stdin(
                                "Digite o número de voltas: ",
                                &num_laps
                            );
                            if (num_laps < 0) num_laps = 0;
                            printf("Número de voltas configurado: %d\n", num_laps);
                            break;

                        case 3:
                            sair_config = true;
                            break;

                        default:
                            printf("Opção inválida.\n");
                            break;
                    }

                    vTaskDelay(pdMS_TO_TICKS(10));
                }

                break;
            }

            case 2:
            {
                if (num_athletes <= 0)
                {
                    printf("Configure primeiro o número de atletas (opção 1 -> 1).\n");
                    break;
                }

                printf("\n=== INSCRIÇÃO DE ATLETAS ===\n");
                num_athletes_registered = 0;
                num_bindings = 0;
                memset(race_results, 0, sizeof(race_results));

                for (int i = 0; i < num_athletes; ++i) {
                    char nameBuf[Athlete::NAME_MAX_LEN];
                    int number = 0;

                    printf("\n---- Cadastro do Atleta %d ----\n", i + 1);

                    char perguntaNome[64];
                    snprintf(perguntaNome, sizeof(perguntaNome),
                             "Escreva o nome do %dº atleta: ", i + 1);

                    if (!read_line_from_stdin(perguntaNome, nameBuf, sizeof(nameBuf))) {
                        printf("Erro ao ler nome. Pulando atleta.\n");
                        continue;
                    }

                    char perguntaNumero[64];
                    snprintf(perguntaNumero, sizeof(perguntaNumero),
                             "Escreva o número do %dº atleta: ", i + 1);

                    read_int_from_stdin(perguntaNumero, &number);

                    if (athletes[i] != nullptr) {
                        delete athletes[i];
                    }

                    athletes[i] = new Athlete(nameBuf, (uint8_t)number, 0, 0.0f);
                    ++num_athletes_registered;

                    printf("Atleta cadastrado: nome='%s', número=%d\n",
                           athletes[i]->getName(),
                           athletes[i]->getNumber());

                    if (i < MAX_ATHLETES && num_bindings < MAX_ATHLETES) {
                        SlaveAthleteBinding* b = &bindings[num_bindings];

                        switch (i) {
                            case 0:
                                memcpy(b->mac, SLAVE_RAIA_1, 6);
                                break;
                            case 1:
                                memcpy(b->mac, SLAVE_RAIA_2, 6);
                                break;
                            default:
                                memset(b->mac, 0, 6);
                                break;
                        }

                        b->athlete       = athletes[i];
                        b->athlete_index = i;
                        num_bindings++;

                        ESP_LOGI(TAG,
                                 "Binding criado: atleta '%s' (número %d) -> binding[%d]",
                                 athletes[i]->getName(),
                                 athletes[i]->getNumber(),
                                 num_bindings - 1);
                    }
                }

                printf("\nTotal de atletas inscritos: %d\n", num_athletes_registered);
                break;
            }

            case 3:
            {
                if (num_athletes_registered <= 0) {
                    printf("Nenhum atleta inscrito. Use a opção 2 primeiro.\n");
                    break;
                }
                if (num_laps <= 0) {
                    printf("Número de voltas não configurado. Use a opção 1 -> 2.\n");
                    break;
                }

                printf("Iniciando corrida...\n");
                printf("As slaves podem ser armadas agora. Botão físico/sinal externo vai disparar o cronômetro.\n");

                espMaster.sendConfig(num_laps, true);

                return;
            }

            default:
                printf("Opção inválida.\n");
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void run_stopwatch()
{
    ESP_LOGI(TAG, "Iniciando cronômetro / painel");

    setup_gpio();
    setup_oled();

    raceFinished = false;
    results_received_count = 0;
    memset(binding_finished, 0, sizeof(binding_finished));
    memset(race_results, 0, sizeof(race_results));

    timerRunning = false;
    elapsed_time_at_pause_us = 0;
    start_time_us = 0;

    update_display(0);
    ESP_LOGI(TAG, "Cronômetro pronto. Pressione o botão para iniciar/pausar.");

    bool     button_was_pressed       = false;
    uint64_t current_display_time_us  = 0;

    bool     results_loop_mode      = false;
    uint64_t results_start_time_us  = 0;  
    uint64_t last_result_switch_us  = 0;  
    int      valid_results_count    = 0;  
    int      result_loop_index      = 0;  

    while (true) {
        int button_state = gpio_get_level(BUTTON_PIN);

        if (button_state == 0 && !button_was_pressed) {
            vTaskDelay(pdMS_TO_TICKS(10));

            if (gpio_get_level(BUTTON_PIN) == 0) {

                if (raceFinished) {
                    ESP_LOGI(TAG,
                             "Botão pressionado, mas a corrida já foi finalizada. Ignorando.");
                } else {
                    timerRunning = !timerRunning;

                    if (timerRunning) {
                        ESP_LOGI(TAG, "START");
                        start_time_us = esp_timer_get_time() - elapsed_time_at_pause_us;
                        gpio_set_level(LED_PIN, 1);
                    } else {
                        ESP_LOGI(TAG, "PAUSE");
                        elapsed_time_at_pause_us = esp_timer_get_time() - start_time_us;
                        gpio_set_level(LED_PIN, 0);
                    }
                }

                button_was_pressed = true;
            }
        }
        else if (button_state == 1 && button_was_pressed) {
            button_was_pressed = false;
        }

        if (timerRunning) {
            current_display_time_us = esp_timer_get_time() - start_time_us;
        } else {
            current_display_time_us = elapsed_time_at_pause_us;
        }

        uint64_t now_us = esp_timer_get_time();

        if (raceFinished && !results_loop_mode) {
            results_loop_mode     = true;
            results_start_time_us = now_us;
            last_result_switch_us = now_us;
            ESP_LOGI(TAG, "Entrando no modo de loop de resultados.");
        }

        if (!results_loop_mode) {
            update_display(current_display_time_us);
        } else {

            if (now_us - results_start_time_us < 2000000ULL) {
            } else {

                if (valid_results_count == 0) {
                    for (int i = 0; i <= num_athletes_registered - 1; ++i) {
                        if (i >= MAX_ATHLETES) break;
                        if (race_results[i].hasResult && athletes[i] != nullptr) {
                            valid_results_count++;
                        }
                    }
                    ESP_LOGI(TAG, "Total de atletas com resultado: %d",
                             valid_results_count);
                    result_loop_index = 0;
                }

                if (valid_results_count > 0) {
                    if (now_us - last_result_switch_us >= 2000000ULL) {
                        int current_valid_index = 0;
                        for (int i = 0; i < num_athletes_registered; ++i) {
                            if (i >= MAX_ATHLETES) break;
                            if (!race_results[i].hasResult || athletes[i] == nullptr) {
                                continue;
                            }

                            if (current_valid_index == result_loop_index) {
                                uint8_t number      = athletes[i]->getNumber();
                                const char* name    = athletes[i]->getName();
                                float total_s       = race_results[i].total_time_s;

                                ESP_LOGI(TAG,
                                         "Mostrando resultado em loop: idx=%d N=%d Nome=%s Tempo=%.2f",
                                         i, number, name, (double)total_s);

                                show_athlete_result_on_display(number, name, total_s);

                                break;
                            }

                            current_valid_index++;
                        }

                        result_loop_index++;
                        if (result_loop_index >= valid_results_count) {
                            result_loop_index = 0;
                        }

                        last_result_switch_us = now_us;
                    }
                }
            }
        }

        static bool results_saved = false;
        if (raceFinished && !results_saved) {
            results_saved = true;

            SpiffsResultWriter writer("/spiffs");
            bool ok = write_all_results(writer,
                                        "resultados.txt",
                                        athletes,
                                        race_results,
                                        num_athletes_registered);

            if (ok) {
                ESP_LOGI(TAG,
                         "Resultados salvos em /spiffs/resultados.txt");
            } else {
                ESP_LOGE(TAG,
                         "Falha ao salvar resultados em /spiffs/resultados.txt");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Master iniciada");

    setvbuf(stdin,  NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    espMaster.init();

    espMaster.setOnResultReceived(handle_results_received);

    run_menu();

    run_stopwatch();
}
