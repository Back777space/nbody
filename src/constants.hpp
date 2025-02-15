#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#define WIDTH 950
#define HEIGHT 950

#define WORKGROUP_SIZE 512

#define NUM_SUBSTEPS 8
#define RENDER_FPS 60.f
#define RENDER_DT (1.f / RENDER_FPS)
#define SIM_DT (RENDER_DT / NUM_SUBSTEPS)