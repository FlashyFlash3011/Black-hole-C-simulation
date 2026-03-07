# Black Hole Renderer

A real-time Schwarzschild black hole renderer using OpenGL 4.3 compute shaders. Photon paths are traced through curved spacetime one per pixel, entirely on the GPU. Every run randomizes the physics parameters and produces a unique result.

![black hole render](https://i.imgur.com/placeholder.png)
<!-- Replace with an actual screenshot before publishing -->

---

## Features

- Exact Schwarzschild null geodesics integrated via RK4, one per pixel on the GPU
- Novikov-Thorne accretion disk with Keplerian rotation, Doppler beaming, and gravitational redshift
- FBM turbulence (MRI proxy) animating the disk in real time — hot spots, spiral density waves, flares
- Relativistic jets with animated propagating knots (~60% of runs)
- Hot X-ray corona accumulated volumetrically near the event horizon
- Gravitationally lensed procedural star field with physically accurate spectral colors
- All parameters randomized per run within physically valid ranges; any render is reproducible by seed

---

## Physics

### Schwarzschild Geometry

Non-rotating black hole in geometric units (G = c = M = 1):

| Radius | Value | Significance |
|---|---|---|
| Event horizon | `r = 2M` | Ray crosses → black |
| Photon sphere | `r = 3M` | Unstable photon orbits → bright ring |
| ISCO | `r = 6M` | Inner edge of accretion disk |

### Geodesic Integration

Rays are cast backward from the camera. Each follows the exact Schwarzschild null geodesic in the orbital plane:

```
d²r/dλ²  = φ̇² (r − 3M)      ← photon sphere at r = 3M, no approximation
d²φ/dλ²  = −(2/r) ṙ φ̇       ← conserves angular momentum L = r²φ̇
```

Integrated with RK4. Step size adapts to curvature — finer near the horizon, coarser far away.

### Disk Emission

Temperature follows the Novikov-Thorne profile:

```
T(r) ∝ (1 − √(r_ISCO / r))^0.25 · r^−0.75
```

Color runs blue-white at the ISCO to deep red at the outer edge. Modulated by FBM turbulence (±40% temperature fluctuation) in Keplerian co-rotating coordinates.

Relativistic corrections applied per disk hit:
- **Doppler beaming** — approaching side boosted by `(1 + v cos θ)³`, receding side dimmed
- **Gravitational redshift** — `√(1 − 2M/r)` applied to emitted energy

### Randomized Parameters

| Parameter | Range | Effect |
|---|---|---|
| Disk tilt | 10° – 55° | Disk plane orientation and lensing geometry |
| Tilt azimuth | 0° – 360° | Axis of tilt |
| Disk outer radius | 13 – 28 M | Disk extent |
| Temperature scale | 0.65 – 1.75 | Overall color temperature |
| Jet power | 0 or 0.35 – 1.0 | Jet intensity (60% chance per run) |
| Turbulence seed | random | MRI hot spot pattern |

The seed is printed to the terminal on every run. Pass it as a command-line argument to reproduce any render exactly.

---

## Requirements

- Windows 10/11
- [MSYS2](https://www.msys2.org/) with the UCRT64 toolchain
- A GPU with OpenGL 4.3 support (any modern discrete GPU)

---

## Building

Open the **MSYS2 UCRT64** terminal (not MINGW64, not the default MSYS terminal).

**Install dependencies** (one time):

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-ninja \
          mingw-w64-ucrt-x86_64-glfw \
          mingw-w64-ucrt-x86_64-glew \
          mingw-w64-ucrt-x86_64-glm
```

**Build:**

```bash
cd "C:/Users/yourname/path/to/BlackholeSimC++"
cmake -B build-win -DCMAKE_BUILD_TYPE=Release
cmake --build build-win
```

**Run:**

```bash
./build-win/blackhole.exe            # random seed
./build-win/blackhole.exe 123456789  # reproduce a specific run
```

Or double-click `blackhole.exe` in Explorer — note the terminal window (showing the seed and parameters) will open alongside it.

---

## Controls

| Input | Action |
|---|---|
| Left-drag | Orbit camera around the black hole |
| Scroll wheel | Zoom in / out |
| `R` | Randomize all parameters (new render) |
| `ESC` | Quit |

After 4 seconds of no input, the camera begins auto-orbiting slowly.

---

## Project Structure

```
BlackholeSimC++/
├── src/
│   └── main.cpp        -- window, camera, parameter sampling, render loop
├── shaders/
│   ├── trace.comp      -- OpenGL 4.3 compute shader: geodesic + disk/jet/star emission
│   ├── quad.vert       -- full-screen triangle (gl_VertexID trick, no VBO)
│   └── quad.frag       -- ACES filmic tone mapping + approximate bloom
└── CMakeLists.txt
```

The compute shader dispatches 16×16 thread groups — one invocation per pixel. The entire geodesic integration loop runs on-GPU. The CPU only sets uniforms and swaps buffers.

---

## License

MIT
