#pragma once

#include <algorithm>
#include <random>
#include <stdint.h>
#include <time.h>

struct Backoff {
    int64_t minAmount;
    int64_t maxAmount;
    int64_t current;
    int fails;
    std::mt19937_64 randGenerator;
    std::uniform_real_distribution<> randDistribution;

    double rand01() { return randDistribution(randGenerator); }

    Backoff(int64_t min, int64_t max)
      : minAmount(min)
      , maxAmount(max)
      , current(min)
      , fails(0)
      , randGenerator((uint64_t)time(0))
    {
    }

    void reset()
    {
        fails = 0;
        current = minAmount;
    }

    int64_t nextDelay()
    {
        ++fails;
        int64_t delay = (int64_t)((double)current * 2.0 * rand01());
        current = std::min(current + delay, maxAmount);
        return current;
    }
};
