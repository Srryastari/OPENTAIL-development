#pragma once
/* OpenTail CORE private particle utility for 1D effect backends.
 * NOT a separate effect, UI mode, WLED firmware dependency or BLE opcode.
 * A lightweight original implementation; not full WLED particle physics parity.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "wled_fx_compat.h"
#define OT_PARTICLES_1D_MAX 64u
typedef enum {OT_PARTICLE_KILL=0,OT_PARTICLE_WRAP,OT_PARTICLE_BOUNCE} ot_particle_edge_t;
typedef struct {
    int32_t x_q8;         /* signed subpixel position (256 units / pixel) */
    int32_t v_q8;         /* signed velocity units / second */
    uint16_t age_ms,life_ms;
    ot_wled_rgb_t color;
    bool alive;
} ot_particle_1d_t;
typedef struct {
    ot_particle_1d_t particles[OT_PARTICLES_1D_MAX];
    uint32_t seed;
} ot_particles_1d_t;
void ot_particles_1d_reset(ot_particles_1d_t *system,uint32_t seed);
size_t ot_particles_1d_count(const ot_particles_1d_t *system);
bool ot_particles_1d_emit(ot_particles_1d_t *system,int32_t x_q8,int32_t v_q8,
                          uint16_t life_ms,ot_wled_rgb_t color);
void ot_particles_1d_step(ot_particles_1d_t *system,uint16_t dt_ms,
                          uint16_t led_count,int32_t acceleration_q8,
                          ot_particle_edge_t edge);
void ot_particles_1d_render(const ot_particles_1d_t *system,
                            ot_wled_rgb_t *pixels,size_t led_count,
                            uint8_t brightness,bool reverse);
