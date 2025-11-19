#ifndef ATHLETE_H
#define ATHLETE_H

#include <stdint.h>
#include <string>

class Athlete
{
public:
    static constexpr size_t NAME_MAX_LEN = 30;

private:
    std::string name;
    uint8_t     number;
    int         age;
    float       bestTime;
    float       totalTime;

public:

    Athlete(const char* athleteName,
            uint8_t athleteNumber,
            int athleteAge,
            float bestTimeRecord);

    void setName(const char* newName);
    void setNumber(uint8_t newNumber);
    void setAge(int newAge);
    void setBestTime(float time);
    void addToTotalTime(float delta);

    const char* getName() const;
    uint8_t getNumber() const;
    int getAge() const;
    float getBestTime() const;
    float getTotalTime() const;
};

#endif // ATHLETE_H
