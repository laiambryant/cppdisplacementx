cmake_minimum_required(VERSION 3.16)
project(cppdisplacementx CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_library(cppdisplacementx STATIC
    src/rng.cpp
    src/params.cpp
    src/command_list.cpp
    src/tile_bins.cpp
    src/cpu_compositor.cpp
    src/post.cpp
    src/sprite_atlas.cpp
    src/composite_glsl.cpp
    src/engine.cpp
)

target_include_directories(cppdisplacementx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(cppdisplacementx PUBLIC Threads::Threads)

enable_testing()

add_executable(cppdx_tests tests/parity_tests.cpp)
target_link_libraries(cppdx_tests PRIVATE cppdisplacementx)
add_test(NAME parity COMMAND cppdx_tests)
