#ifndef RESULT_WRITER_H
#define RESULT_WRITER_H

#include <cstddef>
#include <cstdio>
#include "esp_log.h"

class Athlete;

struct RaceResult {
    bool  hasResult;
    float total_time_s;
};

// =========================
// Interface base (polimorfismo)
// =========================
class IResultWriter {
public:
    virtual ~IResultWriter() = default;
    virtual bool begin(const char* filename) = 0;
    virtual bool writeLine(const Athlete& athlete, const RaceResult& result, int position) = 0;
    virtual void end() = 0;
};

// =========================
// Implementação em SPIFFS
// (herança de IResultWriter)
// =========================
class SpiffsResultWriter : public IResultWriter {
public:
    explicit SpiffsResultWriter(const char* basePath = "/spiffs");
    ~SpiffsResultWriter() override;
    bool begin(const char* filename) override;
    bool writeLine(const Athlete& athlete, const RaceResult& result, int position) override;
    void end() override;

private:
    const char* basePath;
    FILE*      file;
    bool       mounted;

    bool mountIfNeeded();
};

// =========================
// Função template que percorre
// o vetor de atletas/resultados
// e chama o writer polimórfico
// =========================
template<std::size_t N>
bool write_all_results(IResultWriter& writer,  const char* filename, Athlete* const (&athletes)[N], const RaceResult (&results)[N], int num_athletes_registered)
{
    if (!writer.begin(filename))
    {
        return false;
    }

    int position = 1;

    for (int i = 0; i < num_athletes_registered && i < static_cast<int>(N); ++i) {
        if (athletes[i] == nullptr) continue;
        if (!results[i].hasResult) continue;

        if (!writer.writeLine(*athletes[i], results[i], position)) 
        {
            writer.end();
            return false;
        }
        ++position;
    }

    writer.end();
    return true;
}

#endif // RESULT_WRITER_H
