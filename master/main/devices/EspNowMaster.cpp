#include "EspNowMaster.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include <string.h> 
#include "nvs_flash.h"

static const char* TAG = "EspNowMaster";


static const uint8_t BROADCAST_MAC[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

EspNowMaster* EspNowMaster::s_instance = nullptr;


EspNowMaster::EspNowMaster() {
    s_instance = this;
}


void EspNowMaster::setOnResultReceived(std::function<void(const uint8_t* mac_addr, const DadosResultado& data)> cb) {
    user_result_callback = cb;
}

esp_err_t EspNowMaster::init() {

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv_cb));


    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, BROADCAST_MAC, ESP_NOW_ETH_ALEN); 
    peer_info.channel = 0; 
    peer_info.encrypt = false;
    
    if (esp_now_add_peer(&peer_info) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao adicionar o peer de Broadcast");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Peer de Broadcast registrado com sucesso.");
    return ESP_OK;
}

esp_err_t EspNowMaster::sendConfig(int numVoltas, bool ok) {
    ConfigEspNow dados_config;
    dados_config.numVoltas = numVoltas;
    dados_config.okParaIniciar = ok;

    
    esp_err_t result = esp_now_send(BROADCAST_MAC, (const uint8_t*)&dados_config, sizeof(dados_config));

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao enfileirar o envio (broadcast) da configuração.");
    }
    return result;
}


void EspNowMaster::on_data_sent_cb(const wifi_tx_info_t* tx_info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "Envio (broadcast) da Configuração SUCESSO");
    } else {
        ESP_LOGE(TAG, "Envio (broadcast) da Configuração FALHOU");
    }
}

void EspNowMaster::on_data_recv_cb(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len) {
    if (s_instance) {
        s_instance->handle_data_recv(recv_info, data, len);
    }
}



void EspNowMaster::handle_data_recv(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len) {
    // Verifica o tamanho
    if (len != sizeof(DadosResultado)) {
        ESP_LOGE(TAG, "Tamanho do pacote de resultado incorreto: %d", len);
        return;
    }
    
    if (user_result_callback) {
        DadosResultado resultados;
        memcpy(&resultados, data, sizeof(resultados));
        user_result_callback(recv_info->src_addr, resultados);
    }
}