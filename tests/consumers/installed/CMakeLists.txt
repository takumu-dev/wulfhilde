# A game that finds the installed engine package, either whole or as a subset of its modules.
cmake_minimum_required(VERSION 4.4)

project(wulfhilde_installed_consumer LANGUAGES CXX)

set(WULFHILDE_COMPONENTS "" CACHE STRING "The modules to ask for, or empty for the whole engine")

if(WULFHILDE_COMPONENTS)
    find_package(wulfhilde CONFIG REQUIRED COMPONENTS ${WULFHILDE_COMPONENTS})
    if(TARGET wulfhilde::wulfhilde)
        message(FATAL_ERROR "Asking for some modules also brought in the umbrella target.")
    endif()
    set(engine wulfhilde::core)
else()
    find_package(wulfhilde CONFIG REQUIRED)
    set(engine wulfhilde::wulfhilde)
endif()

# The engine's warnings and warnings as errors are its own, and must not reach a game's build.
foreach(target IN ITEMS wulfhilde::core ${engine})
    get_target_property(options ${target} INTERFACE_COMPILE_OPTIONS)
    if(options)
        message(FATAL_ERROR "${target} passes compile options to the game: ${options}")
    endif()
endforeach()

add_executable(consumer ../consumer.cpp)
target_link_libraries(consumer PRIVATE ${engine})
