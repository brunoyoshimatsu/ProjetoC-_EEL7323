#include "athlete.h"

// ====================== CONSTRUTOR ======================
Athlete::Athlete(const char* athleteName,
                 uint8_t athleteNumber,
                 int athleteAge,
                 float bestTimeRecord)
    : name(""),
      number(athleteNumber),
      age(athleteAge),
      bestTime(bestTimeRecord),
      totalTime(0.0f)
{
    setName(athleteName);
}

// ====================== SETTERS ======================

void Athlete::setName(const char* newName)
{
    if (!newName) {
        name.clear();
    } else {
        name = newName;   // std::string cuida do tamanho
    }
}

void Athlete::setNumber(uint8_t newNumber)
{
    number = newNumber;
}

void Athlete::setAge(int newAge)
{
    age = newAge;
}

void Athlete::setBestTime(float time)
{
    bestTime = time;
}

void Athlete::addToTotalTime(float delta)
{
    totalTime += delta;
}

// ====================== GETTERS ======================

const char* Athlete::getName() const
{
    return name.c_str();
}

uint8_t Athlete::getNumber() const
{
    return number;
}

int Athlete::getAge() const
{
    return age;
}

float Athlete::getBestTime() const
{
    return bestTime;
}

float Athlete::getTotalTime() const
{
    return totalTime;
}
