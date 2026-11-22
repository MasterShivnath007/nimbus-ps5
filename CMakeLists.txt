cmake_minimum_required(VERSION 3.20)
project(nimbus LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "")
endif()

find_package(OpenGL REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

set(NIMBUS_SOURCES
  src/platform/desktop.cpp
  src/app/app.cpp
  src/app/screens.cpp
  src/app/screens_settings.cpp
  src/app/camera.cpp
  src/app/astro.cpp
  src/app/settings.cpp
  src/data/weather.cpp
  src/data/json.cpp
  src/data/net.cpp
  src/data/places.cpp
  src/data/hub.cpp
  third_party/sgp4/TLE.c
  third_party/sgp4/SGP4.c
  src/gfx/shader.cpp
  src/gfx/mesh.cpp
  src/gfx/texture.cpp
  src/gfx/font.cpp
  src/gfx/ui.cpp
  src/gfx/sky.cpp
  src/gfx/earth.cpp
  src/gfx/post.cpp
)

add_library(nimbus_core STATIC ${NIMBUS_SOURCES})
target_include_directories(nimbus_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sgp4
)
set_source_files_properties(
  third_party/sgp4/TLE.c
  third_party/sgp4/SGP4.c
  PROPERTIES COMPILE_OPTIONS "-Wno-unused-variable;-Wno-unused-but-set-variable"
)
target_compile_options(nimbus_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(nimbus_core PUBLIC PkgConfig::SDL2 OpenGL::GL CURL::libcurl Threads::Threads)

add_executable(nimbus src/main.cpp)
target_link_libraries(nimbus PRIVATE nimbus_core)

# A small host test binary keeps the math and data parsers honest.
enable_testing()
add_executable(nimbus_tests tests/tests.cpp ${NIMBUS_SOURCES})
target_include_directories(nimbus_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sgp4)
target_compile_options(nimbus_tests PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(nimbus_tests PRIVATE PkgConfig::SDL2 OpenGL::GL CURL::libcurl Threads::Threads)
add_test(NAME nimbus_tests COMMAND nimbus_tests)
