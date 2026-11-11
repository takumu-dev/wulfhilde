find_package(lodepng CONFIG REQUIRED)
find_package(pedantjson CONFIG REQUIRED)

wulfhilde_add_module(assets
    HEADERS
        include/wulfhilde/assets/asset_error.hpp
        include/wulfhilde/assets/asset_problem.hpp
        include/wulfhilde/assets/content_root.hpp
        include/wulfhilde/assets/load_map.hpp
        include/wulfhilde/assets/load_map_assets.hpp
        include/wulfhilde/assets/map_assets.hpp
        include/wulfhilde/assets/material.hpp
        include/wulfhilde/assets/texture_image.hpp
    SOURCES
        content_root.cpp
        file_format.cpp
        file_format.hpp
        file_reading.cpp
        file_reading.hpp
        image_decoding.cpp
        image_decoding.hpp
        json_file.cpp
        json_file.hpp
        load_map.cpp
        load_map_assets.cpp
        map_decoding.cpp
        map_decoding.hpp
        material_decoding.cpp
        material_decoding.hpp
        mip_chain.cpp
        mip_chain.hpp
        object_reader.cpp
        object_reader.hpp
    DEPENDS
        world
    PACKAGES
        lodepng
        pedantjson
)
target_link_libraries(wulfhilde_assets PRIVATE lodepng pedantjson::pedantjson)
