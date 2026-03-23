#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#define WIDTH  1280
#define HEIGHT 720

#define N_PARTICLES 50000

#define WORKGROUP_SIZE       256  // must match layout(local_size_x=) in shaders
#define RADIX_BLOCKS_PER_WG  16  // blocks each workgroup processes in radix sort passes

#define PI  3.14159265359
#define TAU (2*PI)

#define NUM_SUBSTEPS  10
#define SUBSTEP_CAP   10    // max physics steps per frame before dropping
#define RENDER_FPS    144.f
#define RENDER_DT     (1.f / RENDER_FPS)
#define TIME_SCALE    0.25f
#define SIM_DT        (RENDER_DT / NUM_SUBSTEPS * TIME_SCALE)

#define SHOW_OCTREE  0
#define ENABLE_BLOOM 0

// BH_THETA: Barnes-Hut opening angle. Lower = more accurate, slower. 0.5 typical, 0.9 fast.
// EPSILON_SQRD: force softening; must match the constant in force shaders.
#define BH_THETA     0.8f
#define EPSILON_SQRD 0.95f

#define BH_NONE 0
#define BH_CPU  1
#define BH_GPU  2
#define BH_MODE BH_GPU

// Particles must stay within ±WORLD_HALF_SIZE or Morton codes degrade
#define WORLD_HALF_SIZE 500.0f

#define ENERGY_SNAPSHOT_INTERVAL 100.0  // sim seconds between energy snapshots

// Set to 1 to enable startup diagnostics (verifyGPUBH, energy tracking)
#define BH_DIAGNOSTICS 1
