/**
 * energy.c — Sonar energy pool implementation
 */

#include "sonar/energy.h"

static float s_energy;

void energy_init(void)
{
    s_energy = ENERGY_MAX;
}

int energy_spend(float amount)
{
    if (s_energy < amount)
        return 0;
    s_energy -= amount;
    if (s_energy < 0.0f) s_energy = 0.0f;
    return 1;
}

void energy_recharge(float dt)
{
    s_energy += ENERGY_RECHARGE_RATE * dt;
    if (s_energy > ENERGY_MAX)
        s_energy = ENERGY_MAX;
}

float energy_get(void)
{
    return s_energy;
}

float energy_get_fraction(void)
{
    return s_energy / ENERGY_MAX;
}

void energy_shutdown(void)
{
    s_energy = 0.0f;
}
