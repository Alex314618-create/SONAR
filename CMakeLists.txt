cmake_minimum_required(VERSION 3.20)
project(sonar C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# --- Compiler warnings ---
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- Third-party dependencies ---

# SDL2 (shared library via add_subdirectory)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/SDL2 EXCLUDE_FROM_ALL)

# cglm (header-only)
set(CGLM_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/cglm/include)

# glad (GL 3.3 Core loader, compiled as part of the project)
set(GLAD_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/third_party/glad/include)

# OpenAL Soft (built from source)
set(ALSOFT_EXAMPLES  OFF CACHE BOOL "" FORCE)
set(ALSOFT_UTILS     OFF CACHE BOOL "" FORCE)
set(ALSOFT_TESTS     OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL   OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/openal-soft EXCLUDE_FROM_ALL)

# dr_wav + stb_vorbis include paths
set(DR_LIBS_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/dr_libs)
set(STB_INCLUDE_DIR     ${CMAKE_SOURCE_DIR}/third_party/stb)

# --- Source files ---
set(SONAR_SOURCES
    src/main.c
    src/core/window.c
    src/core/input.c
    src/core/timer.c
    src/render/shader.c
    src/render/renderer.c
    src/render/camera.c
    src/render/mesh.c
    src/render/model.c
    src/render/sonar_fx.c
    src/render/hud.c
    src/render/vfx.c
    src/world/map.c
    src/world/entity.c
    src/world/trigger.c
    src/world/stalker.c
    src/world/physics.c
    src/render/vfx_particles.c
    src/sonar/raycast.c
    src/sonar/sonar.c
    src/sonar/energy.c
    src/audio/audio.c
    src/audio/sound.c
    src/audio/spatial.c
    src/third_party/glad/src/gl.c
)

add_executable(sonar ${SONAR_SOURCES})

target_include_directories(sonar PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${GLAD_INCLUDE_DIR}
    ${CGLM_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cgltf
    ${DR_LIBS_INCLUDE_DIR}
    ${STB_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/openal-soft/include
)

target_link_libraries(sonar PRIVATE
    SDL2::SDL2
    SDL2::SDL2main
    opengl32
    OpenAL::OpenAL
)

# --- Copy shaders to build directory ---
add_custom_command(TARGET sonar POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:sonar>/shaders
    COMMENT "Copying shaders to build directory"
)

# --- Copy SDL2.dll next to executable ---
# Debug builds produce SDL2d.dll; copy it as both names so the exe finds it.
add_custom_command(TARGET sonar POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2::SDL2>
        $<TARGET_FILE_DIR:sonar>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2::SDL2>
        $<TARGET_FILE_DIR:sonar>/SDL2.dll
    COMMENT "Copying SDL2.dll to build directory"
)

# --- Copy OpenAL.dll next to executable ---
add_custom_command(TARGET sonar POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:OpenAL>
        $<TARGET_FILE_DIR:sonar>
    COMMENT "Copying OpenAL.dll to build directory"
)

# --- Copy assets to build directory ---
add_custom_command(TARGET sonar POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:sonar>/assets
    COMMENT "Copying assets to build directory"
)
