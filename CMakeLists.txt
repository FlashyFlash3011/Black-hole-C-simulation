cmake_minimum_required(VERSION 3.18)
project(blackhole CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")
set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0")

# --- Dependencies ---
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 REQUIRED glfw3)
pkg_check_modules(GLEW  REQUIRED glew)

# GLM (header-only)
find_path(GLM_INCLUDE_DIR glm/glm.hpp
    HINTS /usr/include /usr/local/include)
if(NOT GLM_INCLUDE_DIR)
    message(FATAL_ERROR "GLM not found. Install: sudo apt install libglm-dev")
endif()

# --- Executable ---
add_executable(blackhole shaders/main.cpp)

target_include_directories(blackhole PRIVATE
    ${GLFW3_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIR}
)

target_link_libraries(blackhole
    OpenGL::GL
    ${GLFW3_LIBRARIES}
    ${GLEW_LIBRARIES}
)

# Bake shader directory into the binary so it can load .glsl files at runtime
target_compile_definitions(blackhole PRIVATE
    SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders"
)

message(STATUS "OpenGL:  ${OPENGL_gl_LIBRARY}")
message(STATUS "GLFW3:   ${GLFW3_LIBRARIES}")
message(STATUS "GLEW:    ${GLEW_LIBRARIES}")
message(STATUS "GLM:     ${GLM_INCLUDE_DIR}")
message(STATUS "Shaders: ${CMAKE_SOURCE_DIR}/shaders")
