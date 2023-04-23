//
// Created by David Archuleta on 4/16/23.
//

#ifndef FLYIO_CHALLENGES_VECTORCLOCK_H
#define FLYIO_CHALLENGES_VECTORCLOCK_H

#include <iostream>
#include <map>
#include <string>
#include <vector>

class VectorClock {
private:
    std::map<std::string, int> vclock;
public:
    void increment(const std::string &process);
    void update(const VectorClock &other);
    [[nodiscard]] bool isConcurrent(const VectorClock &other) const;
    [[nodiscard]] int getTime(const std::string &process) const;
    bool isLessThan(const VectorClock &other) const;
    bool isGreaterThan(const VectorClock &other) const;
    bool isEqual(const VectorClock &other) const;
    friend std::ostream &operator<<(std::ostream &os, const VectorClock &vc);
};

#endif //FLYIO_CHALLENGES_VECTORCLOCK_H
