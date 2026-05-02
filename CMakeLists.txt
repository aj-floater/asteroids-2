cmake_minimum_required(VERSION 3.20)

project(starshard_05 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CTest)

set(STARSHARD_BINARY_NAME "starshard-05")
set(STARSHARD_DATA_DIR_NAME "starshard-05")
set(STARSHARD_APP_ID "io.github.aj_floater.starshard_05")
set(STARSHARD_DESKTOP_FILE "${STARSHARD_APP_ID}.desktop")
set(STARSHARD_METAINFO_FILE "${STARSHARD_APP_ID}.metainfo.xml")
set(STARSHARD_ICON_FILE "${STARSHARD_APP_ID}.svg")
set(STARSHARD_INSTALL_SHADER_DIR
    "${CMAKE_INSTALL_FULL_DATADIR}/${STARSHARD_DATA_DIR_NAME}/shaders")

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
  set(SHADER_COMMON_ARGS)
  set(SHADER_STAGE_VERT_ARGS -fshader-stage=vert)
  set(SHADER_STAGE_FRAG_ARGS -fshader-stage=frag)
elseif(GLSLANG_VALIDATOR_EXECUTABLE)
  set(SHADER_COMPILER "${GLSLANG_VALIDATOR_EXECUTABLE}")
  set(SHADER_COMMON_ARGS -V)
  set(SHADER_STAGE_VERT_ARGS -S vert)
  set(SHADER_STAGE_FRAG_ARGS -S frag)
else()
  message(FATAL_ERROR "Could not find glslc or glslangValidator for shader compilation.")
endif()

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/star.vert.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_COMMON_ARGS} ${SHADER_STAGE_VERT_ARGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/star.vert"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/star.vert.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/star.vert"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/star.frag.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_COMMON_ARGS} ${SHADER_STAGE_FRAG_ARGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/star.frag"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/star.frag.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/star.frag"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/asteroid.vert.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_COMMON_ARGS} ${SHADER_STAGE_VERT_ARGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/asteroid.vert"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/asteroid.vert.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/asteroid.vert"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/asteroid.frag.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_COMMON_ARGS} ${SHADER_STAGE_FRAG_ARGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/asteroid.frag"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/asteroid.frag.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/asteroid.frag"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/ship.vert.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_COMMON_ARGS} ${SHADER_STAGE_VERT_ARGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/ship.vert"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/ship.vert.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/ship.vert"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/ship.frag.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_COMMON_ARGS} ${SHADER_STAGE_FRAG_ARGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/ship.frag"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/ship.frag.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/ship.frag"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle.vert.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_COMMON_ARGS} ${SHADER_STAGE_VERT_ARGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.vert"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle.vert.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.vert"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle.frag.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_COMMON_ARGS} ${SHADER_STAGE_FRAG_ARGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.frag"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle.frag.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.frag"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle_light.frag.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_COMMON_ARGS} ${SHADER_STAGE_FRAG_ARGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle_light.frag"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle_light.frag.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle_light.frag"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle_light.vert.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_COMMON_ARGS} ${SHADER_STAGE_VERT_ARGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle_light.vert"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/particle_light.vert.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle_light.vert"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/fullscreen.vert.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_COMMON_ARGS} ${SHADER_STAGE_VERT_ARGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/fullscreen.vert"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/fullscreen.vert.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/fullscreen.vert"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/blur.frag.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_COMMON_ARGS} ${SHADER_STAGE_FRAG_ARGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/blur.frag"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/blur.frag.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/blur.frag"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/composite.frag.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_COMMON_ARGS} ${SHADER_STAGE_FRAG_ARGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/composite.frag"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/composite.frag.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/composite.frag"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/hud.vert.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_COMMON_ARGS} ${SHADER_STAGE_VERT_ARGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/hud.vert"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/hud.vert.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/hud.vert"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/hud.frag.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_COMMON_ARGS} ${SHADER_STAGE_FRAG_ARGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/hud.frag"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/hud.frag.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/hud.frag"
  VERBATIM
)

add_custom_command(
  OUTPUT "${ASTEROIDS_SHADER_OUTPUT_DIR}/menu_hud.frag.spv"
  COMMAND "${SHADER_COMPILER}" ${SHADER_COMMON_ARGS} ${SHADER_STAGE_FRAG_ARGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders/menu_hud.frag"
          -o "${ASTEROIDS_SHADER_OUTPUT_DIR}/menu_hud.frag.spv"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/menu_hud.frag"
  VERBATIM
)

set(STARSHARD_COMPILED_SHADER_FILES
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

add_custom_target(asteroids_shaders ALL
  DEPENDS ${STARSHARD_COMPILED_SHADER_FILES}
)

add_executable(starshard_05
  src/audio_engine.cpp
  src/main.cpp
  src/app.cpp
  src/game_state.cpp
  src/menu_asteroids.cpp
  src/profile_store.cpp
  src/settings_store.cpp
  src/vulkan_renderer.cpp
)

set_target_properties(starshard_05 PROPERTIES OUTPUT_NAME "${STARSHARD_BINARY_NAME}")

add_dependencies(starshard_05 asteroids_shaders)

target_compile_definitions(starshard_05 PRIVATE
  GLFW_INCLUDE_VULKAN
  STARSHARD_SHADER_BUILD_DIR="${ASTEROIDS_SHADER_OUTPUT_DIR}"
  STARSHARD_SHADER_INSTALL_DIR="${STARSHARD_INSTALL_SHADER_DIR}"
)

target_include_directories(starshard_05 PRIVATE src)
target_include_directories(starshard_05 SYSTEM PRIVATE third_party/miniaudio)

set(STARSHARD_AUDIO_LIBS Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX AND NOT APPLE)
  list(APPEND STARSHARD_AUDIO_LIBS m)
endif()

if(TARGET glfw)
  target_link_libraries(starshard_05 PRIVATE glfw Vulkan::Vulkan ${STARSHARD_AUDIO_LIBS})
elseif(TARGET glfw3::glfw)
  target_link_libraries(starshard_05 PRIVATE glfw3::glfw Vulkan::Vulkan ${STARSHARD_AUDIO_LIBS})
else()
  message(FATAL_ERROR "Could not find a usable GLFW target.")
endif()

if(MSVC)
  target_compile_options(starshard_05 PRIVATE /W4 /permissive-)
else()
  target_compile_options(starshard_05 PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS starshard_05
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(FILES ${STARSHARD_COMPILED_SHADER_FILES}
  DESTINATION "${CMAKE_INSTALL_DATADIR}/${STARSHARD_DATA_DIR_NAME}/shaders"
)

install(FILES
  "packaging/${STARSHARD_DESKTOP_FILE}"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
)

install(FILES
  "packaging/${STARSHARD_METAINFO_FILE}"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo"
)

install(FILES
  "packaging/${STARSHARD_ICON_FILE}"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
)

install(FILES COPYING
  DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/${STARSHARD_APP_ID}"
)

if(BUILD_TESTING)
  add_executable(audio_engine_tests
    tests/audio_engine_tests.cpp
    src/audio_engine.cpp
  )

  target_include_directories(audio_engine_tests PRIVATE src)
  target_include_directories(audio_engine_tests SYSTEM PRIVATE third_party/miniaudio)
  target_link_libraries(audio_engine_tests PRIVATE ${STARSHARD_AUDIO_LIBS})

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

  add_executable(settings_store_tests
    tests/settings_store_tests.cpp
    src/settings_store.cpp
  )

  target_include_directories(settings_store_tests PRIVATE src)

  if(MSVC)
    target_compile_options(settings_store_tests PRIVATE /W4 /permissive-)
  else()
    target_compile_options(settings_store_tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  add_test(NAME settings_store_tests COMMAND settings_store_tests)

  add_executable(profile_store_tests
    tests/profile_store_tests.cpp
    src/profile_store.cpp
  )

  target_include_directories(profile_store_tests PRIVATE src)

  if(MSVC)
    target_compile_options(profile_store_tests PRIVATE /W4 /permissive-)
  else()
    target_compile_options(profile_store_tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  add_test(NAME profile_store_tests COMMAND profile_store_tests)
endif()
