# N-Body Simulation

## Physics

### Integration

Leapfrog (kick-drift-kick) split across two compute shaders per substep:

- **Drift** (`nbodyPositions.glsl`): advance positions by `vel * dt`
- **Kick** (mode-dependent velocity shader): update velocities by `acc * dt * 0.5`

`NUM_SUBSTEPS` substeps are run per rendered frame. The tree (in GPU BH mode) is rebuilt every substep.

### Force Computation Modes

#### `BH_NONE`:  Brute-Force N²

Every particle computes the gravitational force from every other particle directly.
O(N²)

#### `BH_CPU`: Barnes-Hut with CPU Octree

An octree is rebuilt on the CPU each frame from the readback particle positions, flattened to a GPU buffer, and traversed on the GPU with a stack-based BH criterion (`theta`).

O(N log N), but has a CPU–GPU readback stall every substep.

#### `BH_GPU`: Full GPU Barnes-Hut (Karras 2012)

No CPU readback. The tree is built entirely on the GPU each substep in four stages:

### 1. Morton Codes

* **Goal:** Assign each particle a Z-order curve key so that spatially nearby particles are adjacent after sorting.
* **Shader:** `mortonCodes.glsl`
* **Method:** Each particle's position is normalized to `[0, 1]³` within `±WORLD_HALF_SIZE`, then its x/y/z components are quantized to 10 bits each and bit-interleaved into a 30-bit Morton code. The sorted-index array is initialized to the identity permutation `[0, 1, ..., N-1]`. Padding slots (up to `sortedN = nextPow2(N)`) are filled with sentinel code `0xFFFFFFFF` so they sort to the end.
* **Output:** `mortonBuffer` (uint per slot), `sortedIndicesBuffer` (int per slot).

### 2. Radix Sort

* **Goal:** Sort the `(mortonCode, particleIndex)` pairs so Morton codes are in ascending order.
* **Shaders:** `radixHistogram.glsl`, `radixPrefixSum.glsl`, `radixScatter.glsl`
* **Method:** 4-pass LSD radix sort over all 32 bits (8 bits per pass). All 32 bits are required because the sentinel `0xFFFFFFFF` must sort after the maximum real Morton code `0x3FFFFFFF`, which share identical lower 30 bits. Each pass:
    1. **Histogram**: Each workgroup counts 8-bit digit occurrences for its block of elements using warp-private histograms (8 warps × 256 bins) to minimize atomic contention.
    2. **Prefix sum**: A single workgroup runs a Blelloch exclusive scan over 256 digit totals to produce global scatter offsets. Per-workgroup offsets are written back into the histogram buffer.
    3. **Scatter**: Workgroups stably scatter elements to their output positions using the precomputed offsets. Two ping-pong buffer pairs (`mortonBuffer`/`mortonBufferAlt`, `sortedIndicesBuffer`/`sortedIndicesBufferAlt`) alternate as source and destination each pass.
* **Output:** Morton codes sorted ascending. `sortedIndicesBuffer` holds the corresponding particle indices.

### 3. Binary Radix Tree (Karras 2012)

* **Goal:** Build a binary tree over the sorted Morton codes where each internal node covers a contiguous range of leaves.
* **Shader:** `buildRadixTree.glsl`
* **Method:** One GPU thread per internal node (N−1 total). Each thread determines the leaf range `[first, last]` it covers using longest-common-prefix comparisons (`clz` of Morton code XORs), locates the split position via binary search, and assigns left/right children. Internal nodes self-initialize their fields (`atomicCounter = 0`, zero CoM, sentinel bbox) before construction. Duplicate Morton codes are broken by index XOR for stable tiebreaking.
* **Output:** `treeBuffer`: `2N−1` `GpuTreeNode` entries (N−1 internal + N leaves). Children and parent pointers are set. CoM/bbox fields are zero until propagation.

### 4. Center-of-Mass Propagation

* **Goal:** Compute the center of mass and bounding box for every internal node bottom-up.
* **Shader:** `propagateCoM.glsl`
* **Method:** One thread per leaf. Each thread:
    1. Initializes its leaf node's CoM and bbox from the particle position (via `sortedIndices`).
    2. Walks up the tree via parent pointers. At each internal node, an `atomicAdd` on its counter determines arrival order: the first-arriving thread stops. the second-arriving thread reads both children, computes the weighted CoM and combined bbox, writes the result, then continues upward. The walk is capped at 64 iterations as a GPU hang guard.
* **Barrier note:** `memoryBarrierBuffer()` is called before each atomic to ensure sibling writes are visible. The `coherent` buffer qualifier guarantees cross-workgroup visibility.
* **Output:** All internal nodes have valid `centerOfMass` (xyz = CoM, w = total mass) and `bboxMin`/`bboxMax`.

### 5. Force Traversal

* **Goal:** Compute the gravitational acceleration on each particle using the tree.
* **Shader:** `nbodyVelocitiesGPUBH.glsl`
* **Method:** One thread per particle, dispatched in Morton order so spatially adjacent threads traverse similar subtrees (fewer cache misses). Each thread traverses the tree with a 16-element register stack. The Barnes-Hut criterion `s² < θ² × d²` (where `s` = max bbox extent, `d² = |r|² + ε²`) decides whether a node is approximated as a point mass or expanded. When `top ≥ 14`, approximation is forced to guarantee room for two child pushes without overflow.
* **Output:** `velocitiesBuffer` updated (half-kick: `vel += acc * dt * 0.5`), `accBuffer` written.

## Rendering

### Point Rendering
Particles are rendered as `GL_POINTS`. Point size is set per-vertex in the vertex shader based on distance to camera.

### Bloom Post-Processing (`ENABLE_BLOOM`)
The scene is rendered to a HDR framebuffer (`GL_RGBA16F`). A two-pass Gaussian blur is applied, then additively blended back onto the final output.

### Octree Visualization (`SHOW_OCTREE`)
CPU octree cells are drawn as wireframe AABBs. Only active in `BH_CPU` mode.

## Configuration (`constants.hpp`)

| Constant | Default | Description |
|---|---|---|
| `WIDTH` / `HEIGHT` | 1280 / 720 | Window resolution |
| `N_PARTICLES` | 50000 | Particle count |
| `WORKGROUP_SIZE` | 256 | Compute shader local size (must match `layout(local_size_x=)`) |
| `RADIX_BLOCKS_PER_WG` | 16 | Blocks each workgroup processes per radix sort pass |
| `NUM_SUBSTEPS` | 10 | Physics substeps per rendered frame |
| `SUBSTEP_CAP` | 10 | Max substeps per frame before dropping (prevents spiral of death) |
| `RENDER_FPS` | 144 | Target frame rate used for `RENDER_DT` |
| `BH_MODE` | `BH_GPU` | Force mode: `BH_NONE`, `BH_CPU`, `BH_GPU` |
| `BH_THETA` | 0.9 | Barnes-Hut opening angle (lower = more accurate, slower) |
| `EPSILON_SQRD` | 1.0 | Force softening (must match constant in force shaders) |
| `WORLD_HALF_SIZE` | 500.0 | Morton code normalization extent |
| `ENABLE_BLOOM` | 0 | Toggle bloom post-processing |
| `SHOW_OCTREE` | 0 | Toggle octree wireframe (BH_CPU only) |
| `BH_DIAGNOSTICS` | 1 | Enable startup verification and energy tracking |
| `ENERGY_SNAPSHOT_INTERVAL` | 100.0 | Sim seconds between energy snapshots |

## Build

- **Platform**: Windows, Visual Studio 2022, OpenGL 4.5
- **Dependencies**: GLFW (via vcpkg), GLAD (bundled), GLM (bundled), ImGui (bundled)
- **Build system**: CMake with `CMakePresets.json` (`x64-Debug` / `x64-Release`)
- **Build script**: `build.bat [Debug|Release]`
