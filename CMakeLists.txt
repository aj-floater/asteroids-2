cmake_minimum_required(VERSION 3.20)

project(asteroids LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)
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

add_custom_target(asteroids_shaders ALL
  DEPENDS
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/ship.vert.spv"
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/ship.frag.spv"
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle.vert.spv"
    "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle.frag.spv"
)

add_executable(asteroids
  src/main.cpp
  src/app.cpp
  src/game_state.cpp
  src/vulkan_renderer.cpp
)

add_dependencies(asteroids asteroids_shaders)

target_compile_definitions(asteroids PRIVATE
  GLFW_INCLUDE_VULKAN
  ASTEROIDS_SHADER_DIR="${ASTEROIDS_SHADER_OUTPUT_DIR}"
)

target_include_directories(asteroids PRIVATE src)

if(TARGET glfw)
  target_link_libraries(asteroids PRIVATE glfw Vulkan::Vulkan)
elseif(TARGET glfw3::glfw)
  target_link_libraries(asteroids PRIVATE glfw3::glfw Vulkan::Vulkan)
else()
  message(FATAL_ERROR "Could not find a usable GLFW target.")
endif()

if(MSVC)
  target_compile_options(asteroids PRIVATE /W4 /permissive-)
else()
  target_compile_options(asteroids PRIVATE -Wall -Wextra -Wpedantic)
endif()
