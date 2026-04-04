#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <random>

// ─────────────────────────────────────────────────────────────────────────────
// Physics constants  (geometric units: G = c = M = 1)
//   Rs   = 2M = 2        (Schwarzschild radius)
//   ISCO = 6M = 6        (innermost stable circular orbit, inner disk edge)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr float RS        = 2.0f;
static constexpr float DISK_INNER= 6.0f;          // ISCO = 3Rs
static constexpr float DLAMBDA   = 0.075f;
static constexpr int   MAX_STEPS = 4500;
static constexpr float FOV       = 0.50f;          // vertical half-FOV (~28.6°)

// ─────────────────────────────────────────────────────────────────────────────
// Per-run randomised parameters
// ─────────────────────────────────────────────────────────────────────────────
struct RunParams {
    uint64_t  seed;
    float     diskTiltDeg;    // [10, 55]  tilt of disk plane away from horizontal
    float     diskTiltAz;     // [0, 360]  azimuth of the tilt axis
    glm::vec3 diskNormal;     // computed unit normal to disk
    float     diskOuter;      // [13M, 28M]
    float     diskTemp;       // [0.65, 1.75]  temperature scale
    float     jetPower;       // [0, 1]  0 = no jets
    glm::vec2 turbSeed;       // random turbulence offset
};

static RunParams sampleParams(uint64_t seed = 0) {
    if (seed == 0) seed = std::random_device{}();
    std::mt19937_64 rng(seed);
    auto rF = [&](float lo, float hi) {
        return std::uniform_real_distribution<float>(lo, hi)(rng);
    };

    RunParams p;
    p.seed        = seed;
    p.diskTiltDeg = rF(10.0f, 55.0f);
    p.diskTiltAz  = rF(0.0f, 360.0f);
    p.diskOuter   = rF(13.0f, 28.0f);
    p.diskTemp    = rF(0.65f, 1.75f);
    // ~60 % chance of visible jets
    p.jetPower    = std::bernoulli_distribution(0.60)(rng) ? rF(0.35f, 1.0f) : 0.0f;
    p.turbSeed    = glm::vec2(rF(-100.0f, 100.0f), rF(-100.0f, 100.0f));

    // Compute disk normal: rotate (0,1,0) by diskTiltDeg around axis in the xz-plane
    float tiltRad  = glm::radians(p.diskTiltDeg);
    float azRad    = glm::radians(p.diskTiltAz);
    glm::vec3 axis = glm::vec3(cosf(azRad), 0.0f, sinf(azRad));
    glm::mat4 rot  = glm::rotate(glm::mat4(1.0f), tiltRad, axis);
    p.diskNormal   = glm::normalize(glm::vec3(rot * glm::vec4(0,1,0,0)));

    return p;
}

static void printParams(const RunParams& p) {
    std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "  Schwarzschild Black Hole  (seed " << p.seed << ")\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Disk tilt      : " << p.diskTiltDeg  << "° (az " << p.diskTiltAz << "°)\n";
    std::cout << "  Disk outer     : " << p.diskOuter    << " M\n";
    std::cout << "  Disk temp scale: " << p.diskTemp     << "\n";
    std::cout << "  Jet power      : " << p.jetPower
              << (p.jetPower < 0.05f ? "  (no jets this run)\n" : "\n");
    std::cout << "  Turb seed      : (" << p.turbSeed.x << ", " << p.turbSeed.y << ")\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "  Controls: left-drag=orbit  scroll=zoom  R=randomize  ESC=quit\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Window & camera
// ─────────────────────────────────────────────────────────────────────────────
static int    g_width     = 3840;
static int    g_height    = 2160;
static float  g_azimuth   = 0.0f;
static float  g_elevation = 0.42f;   // ~24° above equatorial
static float  g_dist      = 26.0f;
static bool   g_dragging  = false;
static double g_lastX     = 0.0, g_lastY = 0.0;
static double g_lastInput = 0.0;      // timestamp of last user interaction

static RunParams g_params;            // current run parameters

// ─────────────────────────────────────────────────────────────────────────────
// Shader utilities
// ─────────────────────────────────────────────────────────────────────────────
static std::string loadFile(const char* path) {
    std::ifstream f(path);
    if (!f) { std::cerr << "[error] Cannot open: " << path << "\n"; return ""; }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint compileShader(GLenum type, const std::string& src, const char* name) {
    GLuint id  = glCreateShader(type);
    const char* ptr = src.c_str();
    glShaderSource(id, 1, &ptr, nullptr);
    glCompileShader(id);
    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[8192]; glGetShaderInfoLog(id, sizeof(log), nullptr, log);
        std::cerr << "[error] " << name << " compile:\n" << log << "\n";
    }
    return id;
}

static GLuint linkProgram(std::initializer_list<GLuint> shaders, const char* name) {
    GLuint prog = glCreateProgram();
    for (GLuint s : shaders) glAttachShader(prog, s);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[8192]; glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::cerr << "[error] " << name << " link:\n" << log << "\n";
    }
    for (GLuint s : shaders) { glDetachShader(prog, s); glDeleteShader(s); }
    return prog;
}

static GLint u(GLuint prog, const char* name) {
    return glGetUniformLocation(prog, name);
}

// ─────────────────────────────────────────────────────────────────────────────
// GLFW callbacks
// ─────────────────────────────────────────────────────────────────────────────
static void cbSize(GLFWwindow*, int w, int h) {
    g_width = w; g_height = h;
    glViewport(0, 0, w, h);
}
static void cbBtn(GLFWwindow*, int btn, int action, int) {
    if (btn == GLFW_MOUSE_BUTTON_LEFT) {
        g_dragging = (action == GLFW_PRESS);
        g_lastInput = glfwGetTime();
    }
}
static void cbMove(GLFWwindow*, double x, double y) {
    if (g_dragging) {
        g_azimuth   -= static_cast<float>(x - g_lastX) * 0.005f;
        g_elevation  = std::clamp(g_elevation
                                  - static_cast<float>(y - g_lastY)*0.005f,
                                  -1.45f, 1.45f);
        g_lastInput  = glfwGetTime();
    }
    g_lastX = x; g_lastY = y;
}
static void cbScroll(GLFWwindow*, double, double dy) {
    g_dist      = std::clamp(g_dist * (dy > 0 ? 0.88f : 1.14f),
                              RS*2.4f, RS*250.0f);
    g_lastInput = glfwGetTime();
}
static void cbKey(GLFWwindow* win, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    g_lastInput = glfwGetTime();
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(win, GLFW_TRUE);
    if (key == GLFW_KEY_R) {
        g_params = sampleParams();
        printParams(g_params);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Resize output texture
// ─────────────────────────────────────────────────────────────────────────────
static void resizeTex(GLuint tex, int w, int h) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    // Optional seed argument
    uint64_t seed = (argc > 1) ? std::stoull(argv[1]) : 0;
    g_params = sampleParams(seed);
    printParams(g_params);

    // ── GLFW init ──────────────────────────────────────────────────────
    if (!glfwInit()) { std::cerr << "[error] glfwInit\n"; return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(g_width, g_height,
                                       "Schwarzschild Black Hole", nullptr, nullptr);
    if (!win) { std::cerr << "[error] Window creation failed\n"; glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    glfwSetFramebufferSizeCallback(win, cbSize);
    glfwSetMouseButtonCallback(win,    cbBtn);
    glfwSetCursorPosCallback(win,      cbMove);
    glfwSetScrollCallback(win,         cbScroll);
    glfwSetKeyCallback(win,            cbKey);
    g_lastInput = glfwGetTime();

    // ── GLEW ───────────────────────────────────────────────────────────
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "[error] glewInit\n"; return 1; }
    std::cout << "[info] Renderer: " << glGetString(GL_RENDERER)
              << "  OpenGL: " << glGetString(GL_VERSION) << "\n";

    // ── Shaders ────────────────────────────────────────────────────────
    GLuint compProg = linkProgram(
        { compileShader(GL_COMPUTE_SHADER, loadFile(SHADER_DIR"/trace.comp"), "trace.comp") },
        "compute"
    );
    GLuint quadProg = linkProgram(
        { compileShader(GL_VERTEX_SHADER,   loadFile(SHADER_DIR"/quad.vert"), "quad.vert"),
          compileShader(GL_FRAGMENT_SHADER, loadFile(SHADER_DIR"/quad.frag"), "quad.frag") },
        "quad"
    );

    // ── Output texture (rgba32f HDR) ───────────────────────────────────
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
    resizeTex(tex, g_width, g_height);

    // Dummy VAO required by core profile
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);

    int    prevW   = g_width, prevH = g_height;
    double prevFPS = glfwGetTime();
    int    frames  = 0;

    // ── Render loop ────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();

        double now = glfwGetTime();

        // Resize texture on window resize
        if (g_width != prevW || g_height != prevH) {
            resizeTex(tex, g_width, g_height);
            prevW = g_width; prevH = g_height;
        }

        // Auto-orbit when idle (after 4 s of no interaction)
        if (now - g_lastInput > 4.0) {
            g_azimuth += 0.0003f;   // ~1° per second
        }

        // ── Camera vectors ─────────────────────────────────────────────
        float ce = cosf(g_elevation), se = sinf(g_elevation);
        float ca = cosf(g_azimuth),   sa = sinf(g_azimuth);
        glm::vec3 camPos(ce*ca*g_dist, se*g_dist, ce*sa*g_dist);
        glm::vec3 camFwd = glm::normalize(-camPos);

        glm::vec3 up(0,1,0);
        if (std::abs(glm::dot(camFwd, up)) > 0.999f) up = glm::vec3(1,0,0);
        glm::vec3 eR      = glm::normalize(camPos);            // away from BH
        glm::vec3 camRight= glm::normalize(glm::cross(up, eR));
        glm::vec3 camUp   = glm::cross(eR, camRight);

        // ── Dispatch compute ──────────────────────────────────────────
        glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glUseProgram(compProg);

        glUniform2i (u(compProg,"uResolution"),  g_width, g_height);
        glUniform3fv(u(compProg,"uCamPos"),    1, glm::value_ptr(camPos));
        glUniform3fv(u(compProg,"uCamFwd"),    1, glm::value_ptr(camFwd));
        glUniform3fv(u(compProg,"uCamRight"),  1, glm::value_ptr(camRight));
        glUniform3fv(u(compProg,"uCamUp"),     1, glm::value_ptr(camUp));
        glUniform1f (u(compProg,"uFov"),        FOV);
        glUniform1f (u(compProg,"uRs"),         RS);
        glUniform1f (u(compProg,"uDiskInner"),  DISK_INNER);
        glUniform1f (u(compProg,"uDiskOuter"),  g_params.diskOuter);
        glUniform1f (u(compProg,"uDlambda"),    DLAMBDA);
        glUniform1i (u(compProg,"uMaxSteps"),   MAX_STEPS);
        glUniform3fv(u(compProg,"uDiskNormal"), 1, glm::value_ptr(g_params.diskNormal));
        glUniform1f (u(compProg,"uDiskTemp"),   g_params.diskTemp);
        glUniform1f (u(compProg,"uJetPower"),   g_params.jetPower);
        glUniform2fv(u(compProg,"uTurbSeed"),   1, glm::value_ptr(g_params.turbSeed));
        glUniform1f (u(compProg,"uTime"),       static_cast<float>(now));

        int gx = (g_width  + 15) / 16;
        int gy = (g_height + 15) / 16;
        glDispatchCompute(gx, gy, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                        GL_TEXTURE_FETCH_BARRIER_BIT);

        // ── Full-screen blit (tone map + bloom in frag shader) ────────
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(quadProg);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(u(quadProg,"uTex"), 0);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        glfwSwapBuffers(win);

        // ── FPS in title ───────────────────────────────────────────────
        ++frames;
        if (now - prevFPS >= 1.0) {
            double fps = frames / (now - prevFPS);
            std::string title = "Schwarzschild Black Hole  |  seed " +
                                std::to_string(g_params.seed) +
                                "  |  " + std::to_string(static_cast<int>(fps)) + " fps";
            glfwSetWindowTitle(win, title.c_str());
            prevFPS = now;
            frames  = 0;
        }
    }

    glDeleteTextures(1, &tex);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(compProg);
    glDeleteProgram(quadProg);
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
