//
// Created by David Archuleta on 4/16/23.
//

#include "VectorClock.h"

std::ostream &operator<<(std::ostream &os, const VectorClock &vc) {
    os << "{";
    for (auto it = vc.vclock.begin(); it != vc.vclock.end(); ++it) {
        if (it != vc.vclock.begin()) {
            os << ", ";
        }
        os << it->first << ": " << it->second;
    }
    os << "}";
    return os;
}

void VectorClock::increment(const std::string &process) {
    if (vclock.find(process) == vclock.end()) {
        vclock[process] = 1;
    } else {
        vclock[process]++;
    }
}

void VectorClock::update(const VectorClock &other) {
    for (const auto &entry : other.vclock) {
        const std::string &process = entry.first;
        int other_time = entry.second;
        if (vclock.find(process) == vclock.end() || vclock[process] < other_time) {
            vclock[process] = other_time;
        }
    }
}

bool VectorClock::isConcurrent(const VectorClock &other) const {
    bool hasGreater = false;
    bool hasLesser = false;
    for (const auto &entry : vclock) {
        const std::string &process = entry.first;
        int our_time = entry.second;
        int other_time = other.getTime(process);

        if (our_time > other_time) {
            hasGreater = true;
        } else if (our_time < other_time) {
            hasLesser = true;
        }

        if (hasGreater && hasLesser) {
            return true;
        }
    }
    return false;
}

int VectorClock::getTime(const std::string &process) const {
    if (vclock.find(process) == vclock.end()) {
        return 0;
    }
    return vclock.at(process);
}

bool VectorClock::isLessThan(const VectorClock &other) const {
    bool hasLesser = false;
    for (const auto &entry : vclock) {
        const std::string &process = entry.first;
        int our_time = entry.second;
        int other_time = other.getTime(process);

        if (our_time > other_time) {
            return false;
        } else if (our_time < other_time) {
            hasLesser = true;
        }
    }
    return hasLesser;
}

bool VectorClock::isGreaterThan(const VectorClock &other) const {
    bool hasGreater = false;
    for (const auto &entry : vclock) {
        const std::string &process = entry.first;
        int our_time = entry.second;
        int other_time = other.getTime(process);

        if (our_time < other_time) {
            return false;
        } else if (our_time > other_time) {
            hasGreater = true;
        }
    }
    return hasGreater;
}

bool VectorClock::isEqual(const VectorClock &other) const {
    return !isLessThan(other) && !isGreaterThan(other) && !isConcurrent(other);
}
