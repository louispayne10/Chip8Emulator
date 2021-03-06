#pragma once

#include <random>

class RandomNumberGenerator {
public:
    RandomNumberGenerator()
        : gen(rd()),
          distrib(0, 255) {
    }

    uint8_t next() {
        return static_cast<uint8_t>(distrib(gen));
    }

private:
    std::random_device rd;
    std::mt19937 gen;

    // for some reason unsigned char is not a defined template parameter
    std::uniform_int_distribution<uint16_t> distrib;
};
