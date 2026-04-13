cmake_minimum_required(VERSION 3.20)

project(asteroids LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

find_package(Vulkan REQUIRED)
find_package(Threads REQUIRED)
find_package(glfw3 CONFIG QUIET)

if(NOT TARGET glfw AND NOT TARGET glfw3::glfw)
  find_package(glfw3 REQUIRED)
endif()

set(ASTEROIDS_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${ASTEROIDS_SHADER_OUTPUT_DIR}")

find_program(GLSLC_EXECUTABLE glslc)
find_program(GLSLANG_VALIDATOR_EXECUTABLE glslangValidator)

if(GLSLC_EXECUTABLE)
  set(SHADER_COMPILER "${GLSLC_EXECUTABLE}")
  set(SHADER_STAGE_VERT_ARGS)
  set(SHADER_STAGE_FRAG_ARGS)
elseif(GLSLANG_VALIDATOR_EXECUTABLE)
  set(SHADER_COMPILER "${GLSLANG_VALIDATOR_EXECUTABLE}")
  set(SHADER_STAGE_VERT_ARGS -S vert)
  set(SHADER_STAGE_FRAG_ARGS -S frag)
else()
  message(FATAL_ERROR "Could not find glslc or glslangValidator for shader compilation.")
endif()

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/star.vert.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_STAGE_VERT_ARGS} -V
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/star.vert"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/star.vert.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/star.vert"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/star.frag.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_STAGE_FRAG_ARGS} -V
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/star.frag"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/star.frag.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/star.frag"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/asteroid.vert.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_STAGE_VERT_ARGS} -V
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/asteroid.vert"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/asteroid.vert.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/asteroid.vert"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/asteroid.frag.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_STAGE_FRAG_ARGS} -V
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/asteroid.frag"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/asteroid.frag.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/asteroid.frag"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/ship.vert.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_STAGE_VERT_ARGS} -V
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/ship.vert"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/ship.vert.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/ship.vert"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/ship.frag.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_STAGE_FRAG_ARGS} -V
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/ship.frag"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/ship.frag.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/ship.frag"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle.vert.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_STAGE_VERT_ARGS} -V
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.vert"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle.vert.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.vert"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle.frag.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_STAGE_FRAG_ARGS} -V
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.frag"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle.frag.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.frag"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle_light.frag.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_STAGE_FRAG_ARGS} -V
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle_light.frag"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle_light.frag.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle_light.frag"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle_light.vert.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_STAGE_VERT_ARGS} -V
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle_light.vert"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle_light.vert.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle_light.vert"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/fullscreen.vert.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_STAGE_VERT_ARGS} -V
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/fullscreen.vert"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/fullscreen.vert.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/fullscreen.vert"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/blur.frag.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_STAGE_FRAG_ARGS} -V
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/blur.frag"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/blur.frag.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/blur.frag"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/composite.frag.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_STAGE_FRAG_ARGS} -V
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/composite.frag"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/composite.frag.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/composite.frag"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/hud.vert.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_STAGE_VERT_ARGS} -V
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/hud.vert"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/hud.vert.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/hud.vert"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/hud.frag.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_STAGE_FRAG_ARGS} -V
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/hud.frag"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/hud.frag.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/hud.frag"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/menu_hud.frag.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_STAGE_FRAG_ARGS} -V
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/menu_hud.frag"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/menu_hud.frag.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/menu_hud.frag"
  VERBATIM
)

add_custom_target(asteroids_shaders ALL
  DEPENDS
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/ship.vert.spv"
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/star.vert.spv"
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/star.frag.spv"
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/asteroid.vert.spv"
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/asteroid.frag.spv"
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/ship.frag.spv"
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle.vert.spv"
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle.frag.spv"
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle_light.vert.spv"
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle_light.frag.spv"
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/fullscreen.vert.spv"
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/blur.frag.spv"
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/composite.frag.spv"
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/hud.vert.spv"
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/hud.frag.spv"
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/menu_hud.frag.spv"
)

add_executable(asteroids
  src/audio_engine.cpp
  src/main.cpp
  src/app.cpp
  src/game_state.cpp
  src/menu_asteroids.cpp
  src/vulkan_renderer.cpp
)

add_dependencies(asteroids asteroids_shaders)

target_compile_definitions(asteroids PRIVATE
  GLFW_INCLUDE_VULKAN
  ASTEROIDS_SHADER_DIR="${ASTEROIDS_SHADER_OUTPUT_DIR}"
)

target_include_directories(asteroids PRIVATE src)
target_include_directories(asteroids SYSTEM PRIVATE third_party/miniaudio)

set(ASTEROIDS_AUDIO_LIBS Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX AND NOT APPLE)
  list(APPEND ASTEROIDS_AUDIO_LIBS m)
endif()

if(TARGET glfw)
  target_link_libraries(asteroids PRIVATE glfw Vulkan::Vulkan ${ASTEROIDS_AUDIO_LIBS})
elseif(TARGET glfw3::glfw)
  target_link_libraries(asteroids PRIVATE glfw3::glfw Vulkan::Vulkan ${ASTEROIDS_AUDIO_LIBS})
else()
  message(FATAL_ERROR "Could not find a usable GLFW target.")
endif()

if(MSVC)
  target_compile_options(asteroids PRIVATE /W4 /permissive-)
else()
  target_compile_options(asteroids PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
  add_executable(audio_engine_tests
    tests/audio_engine_tests.cpp
    src/audio_engine.cpp
  )

  target_include_directories(audio_engine_tests PRIVATE src)
  target_include_directories(audio_engine_tests SYSTEM PRIVATE third_party/miniaudio)
  target_link_libraries(audio_engine_tests PRIVATE ${ASTEROIDS_AUDIO_LIBS})

  if(MSVC)
    target_compile_options(audio_engine_tests PRIVATE /W4 /permissive-)
  else()
    target_compile_options(audio_engine_tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  add_test(NAME audio_engine_tests COMMAND audio_engine_tests)

  add_executable(game_state_tests
    tests/game_state_tests.cpp
    src/game_state.cpp
  )

  target_include_directories(game_state_tests PRIVATE src)

  if(MSVC)
    target_compile_options(game_state_tests PRIVATE /W4 /permissive-)
  else()
    target_compile_options(game_state_tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  add_test(NAME game_state_tests COMMAND game_state_tests)
endif()
