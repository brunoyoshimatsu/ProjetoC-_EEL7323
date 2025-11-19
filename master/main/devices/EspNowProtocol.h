#pragma once

#include <cstdint> 


typedef struct ConfigEspNow {
    int numVoltas;
    bool okParaIniciar;
} ConfigEspNow;


typedef struct DadosResultado {
    int64_t tempoTotal_us;
    int numVoltas;
    int64_t temposParciais_us[20];
} DadosResultado;