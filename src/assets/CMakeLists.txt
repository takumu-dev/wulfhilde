find_package(pedantjson CONFIG REQUIRED)

wulfhilde_add_module(assets
    HEADERS
        include/wulfhilde/assets/asset_error.hpp
        include/wulfhilde/assets/content_root.hpp
        include/wulfhilde/assets/load_map.hpp
    SOURCES
        content_root.cpp
        json_file.cpp
        json_file.hpp
        load_map.cpp
        map_decoding.cpp
        map_decoding.hpp
        object_reader.cpp
        object_reader.hpp
    DEPENDS
        world
    PACKAGES
        pedantjson
)
target_link_libraries(wulfhilde_assets PRIVATE pedantjson::pedantjson)
