/**
 * energy.h — Sonar energy pool with recharge
 *
 * Manages the energy resource that gates sonar usage.
 */
#pragma once

#define ENERGY_MAX          100.0f
#define ENERGY_RECHARGE_RATE 12.0f

/** Initialize energy to full. */
void energy_init(void);

/**
 * Try to spend energy.
 *
 * @param amount  Energy to spend
 * @return 1 if enough energy was available (and spent), 0 if not
 */
int energy_spend(float amount);

/**
 * Recharge energy over time. Call each frame when not firing.
 *
 * @param dt  Delta time in seconds
 */
void energy_recharge(float dt);

/** @return current energy level [0, ENERGY_MAX]. */
float energy_get(void);

/** @return energy as fraction [0, 1]. */
float energy_get_fraction(void);

/** Shutdown (no-op, for symmetry). */
void energy_shutdown(void);
