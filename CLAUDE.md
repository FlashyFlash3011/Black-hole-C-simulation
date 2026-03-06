# CLAUDE.md — Black Hole Renderer

Physically accurate Kerr black hole renderer. Every run generates a unique, physically valid
black hole system from randomized astrophysical parameters. No artistic shortcuts — all visuals
derive from the math.

## Environment
- C++17, CMake build system
- Platform: WSL2 (AMD RX 9070 XT)
- GPU: Vulkan compute shaders (not CUDA, not OpenCL) — AMD supported via Mesa RADV
- Build: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)`
- Run:  `./build/blackhole [seed]`  — seed=0 (default) uses hardware entropy

## Units and Conventions
- Geometric units throughout: G = c = 1
- Black hole mass M = 1 sets the length/time scale (all coordinates in units of M)
- Boyer-Lindquist coordinates: x^μ = (t, r, θ, φ), indices 0,1,2,3
- Metric signature: (-, +, +, +)
- Spin parameter a: range [0, 0.998 M]. a=0 → Schwarzschild, a≈M → maximally rotating Kerr

## Architecture

```
src/
  params.h/cpp     -- PhysicsParams struct, random sampling, printing
  kerr.h/cpp       -- Kerr metric, inverse metric, analytic derivatives, key radii
  geodesic.h/cpp   -- Hamiltonian null geodesic integrator (RK4)
  camera.h/cpp     -- [TODO] ZAMO observer frame, ray generation per pixel
  disk.h/cpp       -- [TODO] Novikov-Thorne disk, MRI turbulence, emission
  starfield.h/cpp  -- [TODO] Procedural star field + lensing lookup
  jets.h/cpp       -- [TODO] Relativistic jet rendering
  renderer.h/cpp   -- [TODO] Framebuffer, Vulkan dispatch, PNG export
  main.cpp         -- Entry point
shaders/
  geodesic.comp    -- [TODO] Vulkan compute shader (one thread per pixel)
```

## Physics Notes

### Geodesic Integration (Hamiltonian formulation)
Two quantities are conserved exactly by Kerr symmetry — do NOT integrate them:
  E = -p_t   (energy)
  L =  p_φ   (angular momentum)

State vector: (r, θ, p_r, p_θ) — 4 equations only.
φ is integrated alongside for disk hit detection.

Hamilton's equations:
  dr/dλ  = g^rr p_r  =  (Δ/Σ) p_r
  dθ/dλ  = g^θθ p_θ  =  (1/Σ) p_θ
  dp_r/dλ  = (1/2)(∂g_αβ/∂r)  p^α p^β
  dp_θ/dλ  = (1/2)(∂g_αβ/∂θ) p^α p^β

Where p^μ = g^μν p_ν (raised with inverse metric).

Null constraint check: H = (1/2)g^μν p_μ p_ν = 0 throughout. Monitor this for drift.

### Ray Tracing
Rays are traced BACKWARD from the camera (reverse time). Use negative dλ.
Step size is adaptive: smaller near horizon, larger far away (see geodesic_stepsize).

Termination:
  r <= r+ * 1.001           → EventHorizon (black pixel)
  r >= r_max (~1000 M)      → Escaped (sample star field)
  θ crosses π/2 in [r_isco, r_out]  → DiskHit (sample disk emission)

### Key Radii (exact formulas — never approximate)
  r+ = M + sqrt(M²-a²)                            (event horizon)
  r_ph_pro = 2M(1 + cos(2/3 * arccos(-a/M)))      (prograde photon sphere)
  r_isco = Bardeen formula (see kerr.cpp)          (inner disk edge)

### Disk Emission (TODO: disk.cpp)
  T(r) ∝ (mdot/r³)^(1/4) * f_NT(r)   [Novikov-Thorne temperature]
  Emission = blackbody at T(r), corrected for:
    - Gravitational redshift: multiply frequency by sqrt(-g_tt - 2*g_tφ*Ω - g_φφ*Ω²)
    - Doppler beaming: boost approaching side
  Turbulence via seeded Perlin noise modulating local temperature ±30%

### Camera / ZAMO Frame (TODO: camera.cpp)
  Observer is a ZAMO (zero angular momentum observer) at (r_obs, θ_obs).
  Local orthonormal tetrad → pixel (i,j) → coordinate-basis ray direction → initial p_μ.
  Set p_t and p_φ from tetrad projection; solve H=0 for p_r.

### Vulkan Compute (TODO: shaders/geodesic.comp + renderer.cpp)
  One workgroup invocation per pixel. All geodesic math ported to GLSL.
  Analytic metric components inline in the shader (no UBO needed for the geometry).
  Push constants: observer position, BH spin, disk radii, turbulence seed.

## What NOT to do
- Never use nn.TransformerEncoder or any ML framework — this is pure physics + graphics
- Never use std::rand() — use <random> with mt19937_64
- Never hardcode physical constants (all go in params.h or kerr.h)
- Never use approximate photon sphere / ISCO formulas — use the exact closed forms
- pin_memory is irrelevant here (no PyTorch)
