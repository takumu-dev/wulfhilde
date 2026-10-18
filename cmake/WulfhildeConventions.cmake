# The compiler settings every target of the engine, the viewer and the tests builds with.
#
# Everything here is PRIVATE to the target, so none of it reaches the installed package's
# interface. Warnings become errors only through CMAKE_COMPILE_WARNING_AS_ERROR, which the engine's
# own presets set; a game building the engine as a subproject keeps its own choice.

include_guard(GLOBAL)

# Turns on the strict warnings. Third-party headers come in as SYSTEM includes, so these apply to
# the engine's own code alone.
function(wulfhilde_target_warnings target)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /w14242 # conversion from one integer type to a smaller one
            /w14254 # a larger bit field assigned to a smaller one
            /w14263 # a member function hides a virtual one
            /w14265 # a class with virtual functions has a non-virtual destructor
            /w14287 # an unsigned and a negative constant compared
            /we4289 # a for-loop variable used outside the loop
            /w14296 # an expression that is always true or false
            /w14311 # a pointer truncated
            /w14545 /w14546 /w14547 /w14549 # a comma operator with no effect
            /w14555 # an expression with no effect
            /w14619 # a pragma naming a warning that doesn't exist
            /w14640 # a local static that isn't thread-safe to construct
            /w14826 # a sign-extending conversion
            /w14905 /w14906 # a string literal cast to the wrong kind of string
            /w14928 # more than one user-defined conversion
        )
        # MSVC deprecates standard C functions such as freopen in favour of its own _s variants,
        # which the other standard libraries lack.
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wold-style-cast
            -Wnon-virtual-dtor
            -Woverloaded-virtual
            -Wcast-align
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
            -Wimplicit-fallthrough
        )
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            # Checks each Doxygen comment against the declaration it documents.
            target_compile_options(${target} PRIVATE -Wdocumentation)
        endif()
    endif()
endfunction()

# Applies the settings of the engine's own code: the strict warnings, and exceptions off when
# WULFHILDE_EXCEPTIONS is off. The tests use only the warnings, since Catch2 needs exceptions.
function(wulfhilde_target_conventions target)
    wulfhilde_target_warnings(${target})
    if(NOT WULFHILDE_EXCEPTIONS)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            target_compile_options(${target} PRIVATE /EHs-c-)
            target_compile_definitions(${target} PRIVATE _HAS_EXCEPTIONS=0)
        else()
            target_compile_options(${target} PRIVATE -fno-exceptions)
        endif()
    endif()
endfunction()
