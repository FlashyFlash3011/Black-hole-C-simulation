# Black Hole Renderer — Project Init

A physically accurate, real-time black hole renderer built in C++.
Every run produces a unique, physically valid black hole system derived entirely from real astrophysics.
No two renders are the same. No artistic shortcuts.

---

## What It Does

Simulates a rotating (Kerr) black hole with a turbulent accretion disk, relativistic jets, and a lensed
star field. All visual output is derived from actual physics parameters — randomized within physically
valid ranges each run. The result is a renderer where every output is a snapshot of a real possible
universe.

---

## Physics

### Spacetime Geometry — Kerr Metric
The foundation. A rotating black hole warps spacetime according to:

    ds² = -(1 - r_s*r/Σ)c²dt² - (2a*r_s*r*sin²θ/Σ)c dt dφ
          + (Σ/Δ)dr² + Σ dθ² + (r² + a² + a²*r_s*r*sin²θ/Σ)*sin²θ dφ²

Where:
- a       = spin parameter (J/Mc), range [0, 0.998M]
- r_s     = Schwarzschild radius = 2GM/c²
- Σ       = r² + a²cos²θ
- Δ       = r² - r_s*r + a²

Key radii derived from these:
- Event horizon:     r+ = M + sqrt(M² - a²)
- Photon sphere:     r_ph (where photons orbit — creates the bright ring)
- ISCO (innermost stable circular orbit): inner edge of the accretion disk

These are not approximations. They are exact solutions to Einstein's field equations.

### Ray Tracing — Null Geodesics
Rays (photons) are traced *backward* from the camera into the scene. Each ray follows a null geodesic:

    d²x^μ/dλ² + Γ^μ_αβ (dx^α/dλ)(dx^β/dλ) = 0

Where Γ^μ_αβ are the Christoffel symbols computed analytically from the Kerr metric.
Integrated numerically using 4th-order Runge-Kutta (RK4) per ray.

For each pixel:
- Ray escapes to infinity → sample the background star field (lensed)
- Ray hits the accretion disk → compute disk emission at that point
- Ray crosses the event horizon → black

### Accretion Disk — Novikov-Thorne Model
The disk extends from ISCO to an outer radius. Temperature profile:

    T(r) ∝ (M_dot / r³)^(1/4) * f(r)

Where f(r) is the Novikov-Thorne correction factor.
Each annulus emits as a blackbody. Color = blackbody radiation at that temperature (blue/white
near center, orange/red at outer edge).

### Relativistic Effects Applied to the Disk
- **Doppler beaming**: approaching side of disk is dramatically brighter (blueshifted), receding side dimmer
- **Gravitational redshift**: photons climbing out of the gravity well lose energy, shift red
- **Aberration**: the disk appears asymmetric — this is correct, not an error
- **Time delay**: photons from the far side of the disk travel longer paths, causing ghost images

### Turbulence — Magnetorotational Instability (MRI)
Real accretion disks are not smooth. MRI drives turbulence via magnetic field instabilities.
Simulated using layered Perlin noise seeded by the physics parameters, modulating local density and
temperature. This produces hot spots, spiral arms, and flares — different every run.

### Relativistic Jets
Jets emerge along the spin axis when spin is high and accretion rate is sufficient. Their power:

    P_jet ∝ a² * M_dot   (Blandford-Znajek mechanism)

Rendered as collimated plasma beams with inverse-Compton scattering emission (bright core, dim halo).

### Background Star Field
Stars distributed according to a procedural galaxy model (disk + bulge density profile). Each star
gets a spectral type (O/B/A/F/G/K/M) with physically correct color. Stars are then lensed through
the Kerr geometry — multiple images, Einstein rings, and streaking near the photon sphere.

---

## Randomized Physics Parameters (per run)

Every run samples these from physically valid ranges:

| Parameter              | Range                    | Effect                                       |
|------------------------|--------------------------|----------------------------------------------|
| Spin `a`               | [0.0, 0.998] M           | Geometry, ISCO radius, jet power             |
| Black hole mass `M`    | [1, 100] solar masses    | Scale of everything                          |
| Accretion rate `M_dot` | [0.01, 1.0] Eddington    | Disk brightness, temperature, jet power      |
| Observer inclination   | [0°, 85°]                | Edge-on vs pole-on — completely changes look |
| Observer distance      | [5, 50] r_s              | FOV, lensing intensity                       |
| Disk outer radius      | [10, 30] r_s             | Disk size                                    |
| Turbulence seed        | random uint64            | MRI hot spot pattern                         |
| Star field seed        | random uint64            | Background star distribution                 |
| Dust opacity           | [0.0, 0.5]               | Obscuration of far disk                      |

A summary of the exact parameters used prints to the terminal on each run, so the render is
reproducible if desired (pass the seed back in as an argument).

---

## Architecture

```
blackhole/
├── src/
│   ├── main.cpp              -- entry point, parameter sampling, render loop
│   ├── kerr.cpp/h            -- Kerr metric, Christoffel symbols, key radii
│   ├── geodesic.cpp/h        -- RK4 null geodesic integrator
│   ├── disk.cpp/h            -- Novikov-Thorne disk, turbulence, emission
│   ├── jets.cpp/h            -- relativistic jet rendering
│   ├── starfield.cpp/h       -- procedural star field + lensing
│   ├── camera.cpp/h          -- observer setup, ray generation
│   ├── renderer.cpp/h        -- orchestrates GPU dispatch, framebuffer
│   └── params.cpp/h          -- physics parameter sampling and seeding
├── shaders/
│   └── geodesic.comp         -- Vulkan compute shader (one thread per pixel)
├── CMakeLists.txt
└── CLAUDE.md
```

### Rendering Pipeline
1. CPU samples physics parameters, computes Kerr geometry constants
2. GPU (Vulkan compute) launches one thread per pixel
3. Each thread: generate ray → integrate geodesic → evaluate disk/star/jet emission → write pixel
4. CPU reads framebuffer → display via SDL2 window or save PNG

### GPU
Vulkan compute shaders. AMD RX 9070 XT on WSL2 supports Vulkan via AMDVLK or Mesa RADV.
One shader invocation per pixel. The geodesic integration loop runs entirely on-GPU.

### Resolution
Default: 1280×720. Each pixel is independent — trivially parallelizable.
At this resolution: ~920k geodesics computed per frame.

---

## Output

- Real-time window via SDL2 (if interactive mode)
- PNG export with embedded parameter metadata in EXIF/comment block
- Terminal printout of all physics parameters used (so any render can be reproduced)

---

## Dependencies

- C++17
- Vulkan SDK
- SDL2
- glm (math)
- stb_image_write (PNG export)
- nlohmann/json (parameter serialization)

All installable via apt on WSL2 Ubuntu.

---

## Build

```bash
sudo apt install vulkan-sdk libsdl2-dev libglm-dev cmake
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/blackhole
```

---

## CLAUDE.md Notes (fill in after setup)

- Platform: WSL2, AMD RX 9070 XT
- Vulkan backend: Mesa RADV (verify with `vulkaninfo`)
- GPU compute via Vulkan compute shaders (not CUDA, not OpenCL)
- All physics constants in `src/params.h` — never hardcode
- Geodesic step size: adaptive based on local metric curvature (smaller near horizon)
- Do not use `std::rand()` — use `<random>` with mt19937_64 seeded from hardware entropy
