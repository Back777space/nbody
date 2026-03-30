# Testing & Diagnostics

All diagnostics are gated on `#define BH_DIAGNOSTICS 1` in `constants.hpp`. When enabled, verification runs once at startup and energy tracking runs throughout the simulation. Set to 0 to skip for benchmarking.

## GPU BH Startup Verification (`verifyGPUBH`)

**File:** `src/debug/gpuBHDebug.hpp`
**Triggered:** Once at startup when `BH_MODE == BH_GPU` and `BH_DIAGNOSTICS == 1`.

Reads all relevant GPU buffers and runs five checks:

### Check 1: Morton codes sorted
Verifies `mortonBuffer` is in non-decreasing order and reports the number of sentinel values (`0xFFFFFFFF`) used as padding.

**Pass:** Fully sorted, sentinel count matches `sortedN - N`.

### Check 2: Index permutation valid
Verifies `sortedIndicesBuffer[0..N-1]` forms a valid permutation of `[0, N-1]` with no duplicates or out-of-range entries. Also cross-validates each Morton code against the CPU-computed value for the corresponding particle.

**Pass:** Valid permutation, zero code mismatches.

### Check 3: All leaves reachable
DFS from root through `treeBuffer`, following `leftChild`/`rightChild` pointers. Counts reachable leaf nodes.

**Pass:** All N leaves reachable.

### Check 4: Root mass and zero-mass nodes
Compares the root node's total mass against the sum of all particle masses (`positions[i].w`). Also counts internal nodes with zero or negative mass.

**Pass:** Mass error < 0.1%, zero zero-mass internal nodes.

### Check 5: Force accuracy
For four mid-octant particles (`N/8, 3N/8, 5N/8, 7N/8`), computes three force magnitudes:
- **N²**: brute-force CPU reference
- **Tree**: CPU traversal of the GPU tree data with the same BH criterion
- **GPU**: value read directly from `accBuffer`

Reports the maximum relative error for both tree and GPU against the N² reference. Near-zero-force particles (< 5% of mean) are skipped to avoid meaningless relative errors.

**Pass:** Max error < 2% for both tree and GPU.

### Example output

```
=== GPU BH Verification (N=50000) ===
[1] Morton sorted:    OK  (sentinels=15536)
[2] Indices valid:    OK  code match: OK
[3] Leaves reachable: OK  (50000/50000)
[4] Root mass:        OK  (50000.0000 vs 50000.0000)
    Zero-mass nodes:  OK  (0/49999)
[5] Force error:      tree=0.8312% OK  gpu=1.1047% OK
```

## Extended Diagnostics

**File:** `src/debug/gpuBHDebugExtended.hpp`

Four additional functions for isolating tree corruption. These are not called by default: uncomment in `simulator.hpp` when debugging a specific failure.

| Function | What it checks |
|---|---|
| `diagnoseRadixSortOutput` | Sort order, index bounds, duplicate indices |
| `diagnoseTreeStructurePreCoM` | Parent pointer initialization, child pointer validity, cycle detection |
| `diagnoseCoMPropagationState` | `atomicCounter` histogram (should all be 2), zero-mass internal nodes |
| `diagnoseGPUTreeIssues` | Full 5-part check: multiple parents, orphaned nodes, counter states, zero-mass nodes, leaf initialization |

## Energy Conservation Tracking (`EnergyTracker`)

**File:** `src/debug/gpuBHDebug.hpp`
**Triggered:** Every `ENERGY_SNAPSHOT_INTERVAL` simulated seconds when `BH_MODE == BH_GPU` and `BH_DIAGNOSTICS == 1`.

Computes kinetic and potential energy from the current GPU state:

- **KE** = 0.5 × Σ(m × |v|²)
- **PE** = −Σᵢ<ⱼ (mᵢ × mⱼ / √(|rᵢⱼ|² + ε²))
- For N > 10000, PE is estimated by sampling 500 random particles and scaling: `PE ≈ peSum × N / SAMPLE / 2`
- **Drift** = (E − E₀) / |E₀|

Energy snapshots are printed at program exit via `printEnergyReport()`.

| Drift | Rating |
|---|---|
| < 1% | OK |
| 1–5% | WARN |
| > 5% | FAIL |

Expected drift sources: BH approximation error (theta), finite timestep, floating-point accumulation, close encounters.

### Example output

```
=== Energy Report ===
      Time            KE            PE         Total     Drift(%)
     0.000000  1.234568e+04 -3.456789e+04 -2.222221e+04   0.000000
   100.000000  1.198432e+04 -3.420654e+04 -2.222222e+04   0.000005
   200.000000  1.267891e+04 -3.490113e+04 -2.222222e+04   0.000004

Drift: 0.0001%  [OK]
```

## Pipeline Profiler (`profilePipeline`)

**File:** `src/simulation/nbody.hpp`
**Triggered:** Once at startup alongside `verifyGPUBH` when `BH_DIAGNOSTICS == 1`.

Uses `GL_TIME_ELAPSED` queries to time each pipeline stage for one full substep:

```
=== GPU Pipeline Profile (1 substep) ===
  Morton codes : 0.312 ms
  Radix sort   : 1.847 ms
  Build tree   : 0.623 ms
  Propagate CoM: 0.891 ms
  Velocity     : 4.215 ms
  Total (1x)    : 7.888 ms
  Proj. FPS (10x substeps): 12.7 FPS
```
