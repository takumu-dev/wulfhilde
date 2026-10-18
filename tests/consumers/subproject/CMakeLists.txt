# A game that adds the engine's source tree as a subproject.
cmake_minimum_required(VERSION 4.4)

project(wulfhilde_subproject_consumer LANGUAGES CXX)

set(WULFHILDE_SOURCE_DIR "" CACHE PATH "The engine's source tree")
add_subdirectory("${WULFHILDE_SOURCE_DIR}" wulfhilde)

# As a subproject, the engine builds its libraries alone.
foreach(target IN ITEMS wulfhilde-viewer wulfhilde_core_tests)
    if(TARGET ${target})
        message(FATAL_ERROR "The engine built ${target} as a subproject.")
    endif()
endforeach()

add_executable(consumer ../consumer.cpp)
target_link_libraries(consumer PRIVATE wulfhilde::wulfhilde)
