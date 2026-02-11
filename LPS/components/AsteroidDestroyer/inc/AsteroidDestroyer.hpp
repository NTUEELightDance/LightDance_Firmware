#pragma once

#include "esp_err.h"

class AsteroidDestroyer {
  public:
    AsteroidDestroyer() = default;
    ~AsteroidDestroyer() = default;

    static void DestroyTheAsteroid();
};