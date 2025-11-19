#pragma once

#include "esp_now.h"
#include "esp_err.h"
#include <functional>
#include "EspNowProtocol.h" 

class EspNowMaster {
public:
    EspNowMaster();
    esp_err_t init();
    void setOnResultReceived(std::function<void(const uint8_t* mac_addr, const DadosResultado& data)> cb);

    esp_err_t sendConfig(int numVoltas, bool ok);

private:
    std::function<void(const uint8_t* mac_addr, const DadosResultado& data)> user_result_callback;
    static EspNowMaster* s_instance;
    static void on_data_sent_cb(const wifi_tx_info_t* tx_info, esp_now_send_status_t status);
    static void on_data_recv_cb(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len);
    void handle_data_recv(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len);
};