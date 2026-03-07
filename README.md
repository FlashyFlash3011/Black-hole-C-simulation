# Black Hole Renderer

A real-time Schwarzschild black hole renderer in C++ using OpenGL 4.3 compute shaders.
Every run produces a unique, physically grounded render — randomized physics parameters, no two runs alike.

---

## What It Does

Simulates a non-rotating (Schwarzschild) black hole with a turbulent accretion disk, relativistic jets,
and a lensed star field. Photon paths are traced backward from the camera through curved spacetime using
exact null geodesics. All visual output follows from the physics — nothing is faked.

Real-time interactive window at 3840x2160 with orbit/zoom controls.

---

## Physics

### Spacetime — Schwarzschild Metric

Non-rotating black hole. In geometric units (G = c = M = 1):

- Schwarzschild radius: `Rs = 2M = 2`
- ISCO (inner disk edge): `r = 6M = 3 Rs`
- Photon sphere: `r = 1.5 Rs` (unstable photon orbits — creates the bright ring)

### Ray Tracing — Null Geodesics

Rays are traced backward from the camera (one per pixel). Each follows a Schwarzschild null geodesic,
integrated numerically in the orbital plane using 4th-order Runge-Kutta (RK4):

```
d²r/dλ²   = φ̇² · (r − 1.5 Rs)
d²φ/dλ²   = −(2/r) · ṙ · φ̇         ← conserves angular momentum L = r²φ̇
```

No approximations. Step size adapts to local curvature (smaller near the horizon).

For each pixel:
- Ray escapes to infinity → sample procedural star field (gravitationally lensed)
- Ray hits accretion disk → compute disk emission at that point
- Ray crosses event horizon → black

### Accretion Disk — Novikov-Thorne Temperature Profile

The disk spans from ISCO to a randomized outer radius. Temperature decreases outward:

```
T(r) ∝ (1 − sqrt(r_ISCO / r))^0.25 · r^−0.75
```

Each point emits as a blackbody. Color runs from blue-white near the ISCO to deep red at the outer edge.

### Relativistic Effects on the Disk

- **Doppler beaming**: approaching side dramatically brighter (boost ∝ (1 + v cos θ)³), receding side dimmer
- **Gravitational redshift**: photons climbing out of the gravity well lose energy — `sqrt(1 − Rs/r)`
- **Disk tilt**: the disk plane is randomized each run — produces asymmetric lensing geometry

### Turbulence — MRI Proxy

Real accretion disks are driven turbulent by the magnetorotational instability (MRI).
Modeled via 4-octave FBM (fractal Brownian motion) noise seeded per run, applied in Keplerian co-rotating
coordinates. Produces hot spots, spiral density waves, and flares — different every run.

The disk co-rotates: material follows Keplerian orbits (`Ω = sqrt(Rs / (2r³))`), and turbulent structures
animate with it in real time.

### Relativistic Jets

Jets appear along the disk-normal axis when `jetPower > 0` (~60% of runs). Power follows the
Blandford-Znajek-like scaling rendered as collimated volumetric beams with animated propagating knots.
Jet emission is accumulated volumetrically as the ray passes through the beam region.

### Hot Corona

A diffuse, warm X-ray corona is accumulated near the black hole (`r < 5.2 Rs`), contributing a faint
ambient glow visible even toward the event horizon.

### Star Field

Procedural star field covering the full sky sphere. Stars have physically motivated spectral colors
(O/B/A/F/G/K/M types via a log-uniform brightness distribution). Gravitationally lensed by the
Schwarzschild geometry — multiple images and Einstein rings appear near the photon sphere.

---

## Randomized Parameters (per run)

| Parameter         | Range          | Effect                                              |
|-------------------|----------------|-----------------------------------------------------|
| Disk tilt         | 10° – 55°      | Disk plane orientation — changes lensing geometry   |
| Tilt azimuth      | 0° – 360°      | Rotation of tilt axis                               |
| Disk outer radius | 13 – 28 M      | Disk size                                           |
| Disk temperature  | 0.65 – 1.75    | Color temperature scale (bluer vs. redder disk)     |
| Jet power         | 0 or 0.35–1.0  | Jet intensity (60% chance of jets per run)          |
| Turbulence seed   | random vec2    | MRI hot spot pattern                                |

A seed is printed to the terminal on each run. Pass it as a command-line argument to reproduce any render.

---

## Architecture

```
BlackholeSimC++/
├── src/
│   └── main.cpp          -- entry point: GLFW window, camera, render loop, parameter sampling
├── shaders/
│   ├── trace.comp        -- OpenGL 4.3 compute shader: geodesic integration, disk/jet/star emission
│   ├── quad.vert         -- full-screen triangle (no VBO, gl_VertexID trick)
│   └── quad.frag         -- ACES tone mapping + approximate bloom
└── CMakeLists.txt
```

### Rendering Pipeline

1. CPU samples random physics parameters, sets up camera vectors
2. OpenGL compute shader (`trace.comp`) dispatches 16×16 thread groups — one invocation per pixel
3. Each invocation: generate ray → integrate geodesic (RK4) → evaluate disk/jet/corona/star emission → write HDR pixel
4. Fragment shader blits the HDR texture to screen with ACES filmic tone mapping and approximate bloom
5. GLFW swaps buffers; FPS displayed in window title

### GPU

OpenGL 4.3 compute shaders. Tested on AMD RX 9070 XT under WSL2 (Mesa RADV).
16×16 thread groups. All geodesic integration runs on-GPU, one shader invocation per pixel.

### Resolution

Default: 3840×2160 (4K). Resizable — texture reallocates on window resize.
At 4K: ~8.3M geodesics computed per frame.

---

## Controls

| Input           | Action                                   |
|-----------------|------------------------------------------|
| Left-drag       | Orbit camera around black hole           |
| Scroll          | Zoom in / out                            |
| `R`             | Randomize all parameters (new render)    |
| `ESC`           | Quit                                     |
| Idle (4s)       | Auto-orbit begins                        |

---

## Dependencies

- C++17
- OpenGL 4.3+
- GLFW3
- GLEW
- GLM (header-only)
- CMake 3.18+

Install on Ubuntu / WSL2:

```bash
sudo apt install libglfw3-dev libglew-dev libglm-dev cmake build-essential
```

---

## Build & Run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/blackhole              # random seed each run
./build/blackhole 123456789    # reproduce a specific run by seed
```

Platform notes:
- WSL2: requires a working Vulkan/OpenGL driver visible to WSL. Mesa RADV works for AMD GPUs.
  Verify with `glxinfo | grep "OpenGL version"` — need 4.3+.
- Native Linux: works with any OpenGL 4.3-capable driver.
