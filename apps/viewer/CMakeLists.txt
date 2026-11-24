# wulfhilde-viewer, the map-preview tool. It is a client of the engine like any game, linking only
# the umbrella target. Its code is a small library, which tests/viewer tests, plus main().

add_library(wulfhilde_viewer_lib STATIC
    benchmark.cpp
    benchmark.hpp
    benchmark_record.cpp
    benchmark_record.hpp
    benchmark_walks.cpp
    benchmark_walks.hpp
    cameras.cpp
    cameras.hpp
    command_line.cpp
    command_line.hpp
    controller.cpp
    controller.hpp
    input.cpp
    input.hpp
    pacing.cpp
    pacing.hpp
    tick_clock.cpp
    tick_clock.hpp
)
target_include_directories(wulfhilde_viewer_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(wulfhilde_viewer_lib PUBLIC wulfhilde::wulfhilde)
wulfhilde_target_conventions(wulfhilde_viewer_lib)

add_executable(wulfhilde-viewer main.cpp)
target_link_libraries(wulfhilde-viewer PRIVATE wulfhilde_viewer_lib)
wulfhilde_target_conventions(wulfhilde-viewer)

if(WULFHILDE_INSTALL)
    install(TARGETS wulfhilde-viewer RUNTIME)
endif()
