#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#define WIDTH 950
#define HEIGHT 950

#define WORKGROUP_SIZE 256

#define PI 3.14159265359
#define TAU 2*PI

#define NUM_SUBSTEPS 5
#define RENDER_FPS 60.f
#define RENDER_DT (1.f / RENDER_FPS)
#define SIM_DT (RENDER_DT / NUM_SUBSTEPS)

#define SHOW_OCTREE 0
#define ENABLE_BLOOM 1