#include "ResultWriter.h"
#include "athlete.h"

#include "esp_spiffs.h"

static const char* TAG_WRITER = "ResultWriter";

SpiffsResultWriter::SpiffsResultWriter(const char* basePath_)
    : basePath(basePath_), file(nullptr), mounted(false)
{
}

SpiffsResultWriter::~SpiffsResultWriter()
{
    end();
}

bool SpiffsResultWriter::mountIfNeeded()
{
    if (mounted) {
        return true;
    }

    esp_vfs_spiffs_conf_t conf = {};
    conf.base_path = basePath;
    conf.partition_label = nullptr;
    conf.max_files = 5;
    conf.format_if_mount_failed = true;

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_WRITER, "Erro ao montar SPIFFS (%s)", esp_err_to_name(ret));
        return false;
    }

    mounted = true;
    size_t total = 0, used = 0;
    if (esp_spiffs_info(conf.partition_label, &total, &used) == ESP_OK) {
        ESP_LOGI(TAG_WRITER, "SPIFFS montado. Total: %d, Usado: %d", (int)total, (int)used);
    }

    return true;
}

bool SpiffsResultWriter::begin(const char* filename)
{
    if (!mountIfNeeded()) {
        return false;
    }

    char path[64];
    std::snprintf(path, sizeof(path), "%s/%s", basePath, filename);

    file = std::fopen(path, "w");
    if (!file) {
        ESP_LOGE(TAG_WRITER, "Falha ao abrir arquivo: %s", path);
        return false;
    }

    std::fprintf(file, "Resultados da corrida\r\n");
    std::fprintf(file, "Pos;Numero;Nome;Tempo_total_s\r\n");
    return true;
}

bool SpiffsResultWriter::writeLine(const Athlete& athlete,
                                   const RaceResult& result,
                                   int position)
{
    if (!file) {
        ESP_LOGE(TAG_WRITER, "Arquivo não está aberto para escrita");
        return false;
    }

    std::fprintf(file, "%d;%u;%s;%.3f\r\n",
                 position,
                 (unsigned)athlete.getNumber(),
                 athlete.getName(),
                 result.total_time_s);

    return true;
}

void SpiffsResultWriter::end()
{
    if (file) {
        std::fclose(file);
        file = nullptr;
        ESP_LOGI(TAG_WRITER, "Arquivo de resultados fechado.");
    }
}
