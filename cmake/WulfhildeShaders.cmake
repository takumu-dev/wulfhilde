# How shaders reach the code that runs them: compiled from Slang when the engine builds, never at run
# time, and embedded in the target that creates their pipelines.

include_guard(GLOBAL)

# Compiles Slang files into SPIR-V that `target` embeds. slang::slangc must be found first.
#
#   SOURCES             the files to compile, each holding one pipeline's entry points
#   MODULE_DIRECTORIES  where the modules they import are found
#
# Each <name>.slang becomes one SPIR-V 1.6 module, in scalar layout, with every entry point under
# its own name and descriptor handles in set 0. Debug builds keep the source for stepping through,
# and others are optimised. Two headers come of it, which `target` includes by name:
#
#   <name>_spirv.h      `const uint32_t <name>_spirv[]`, the module, for a namespace to hold
#   <name>_layout.hpp   static_asserts that each struct the shaders share with C++ has a twin of
#                       the same name and fields, laid out alike, for the scope of the twins to
#                       include, as cmake/shader_layout.cmake describes
function(wulfhilde_add_shaders target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;MODULE_DIRECTORIES")
    set(layout_script "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/shader_layout.cmake")
    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    set(module_options "")
    foreach(directory IN LISTS arg_MODULE_DIRECTORIES)
        cmake_path(ABSOLUTE_PATH directory)
        list(APPEND module_options -I "${directory}")
    endforeach()

    foreach(source IN LISTS arg_SOURCES)
        cmake_path(ABSOLUTE_PATH source OUTPUT_VARIABLE source_path)
        cmake_path(GET source STEM name)
        set(spirv "${output_dir}/${name}_spirv.h")
        set(layout "${output_dir}/${name}_layout.hpp")
        set(reflection "${output_dir}/${name}.reflection.json")
        add_custom_command(
            OUTPUT "${spirv}" "${layout}"
            BYPRODUCTS "${reflection}"
            COMMAND slang::slangc "${source_path}" ${module_options}
                -target spirv -profile spirv_1_6 -std 2026 -warnings-as-errors all
                -matrix-layout-column-major -fvk-use-scalar-layout -fvk-use-entrypoint-name
                -bindless-space-index 0 "$<IF:$<CONFIG:Debug>,-g;-O0,-O2>"
                -depfile "${spirv}.d" -reflection-json "${reflection}"
                -source-embed-style u32 -source-embed-name "${name}_spirv" -o "${spirv}"
            COMMAND "${CMAKE_COMMAND}" "-DREFLECTION=${reflection}" "-DOUTPUT=${layout}"
                "-DSOURCE=${name}.slang" -P "${layout_script}"
            MAIN_DEPENDENCY "${source_path}"
            DEPENDS "${layout_script}"
            DEPFILE "${spirv}.d"
            COMMENT "Compiling the shaders of ${name}.slang"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
        target_sources(${target} PRIVATE "${spirv}" "${layout}")
    endforeach()
    # As system headers, so that the warnings and clang-tidy leave generated code alone.
    target_include_directories(${target} SYSTEM PRIVATE "${output_dir}")
endfunction()
