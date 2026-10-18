# The engine's modules. Each is a static library wulfhilde::<module>, and the umbrella
# wulfhilde::wulfhilde links them all.

add_library(wulfhilde INTERFACE)
add_library(wulfhilde::wulfhilde ALIAS wulfhilde)

add_subdirectory(core)
