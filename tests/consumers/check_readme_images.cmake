# Checks that every image the installed README shows by a relative path was installed beside it, so
# that the installed copy reads as the repository's does.
#
#   README  the installed README.md

file(READ "${README}" text)
get_filename_component(directory "${README}" DIRECTORY)

# HTML's src and srcset, and Markdown's ![alt](path). A srcset may list several images, each
# followed by an optional size.
string(REGEX MATCHALL "(src|srcset)=\"[^\"]*\"" attributes "${text}")
string(REGEX MATCHALL "!\\[[^]]*\\]\\([^)]*\\)" markdown "${text}")
set(paths "")
foreach(attribute IN LISTS attributes)
    string(REGEX REPLACE "^[a-z]+=\"(.*)\"$" "\\1" candidates "${attribute}")
    string(REPLACE "," ";" candidates "${candidates}")
    foreach(candidate IN LISTS candidates)
        string(STRIP "${candidate}" candidate)
        string(REGEX REPLACE " .*$" "" candidate "${candidate}")
        list(APPEND paths "${candidate}")
    endforeach()
endforeach()
foreach(image IN LISTS markdown)
    string(REGEX REPLACE "^!\\[[^]]*\\]\\(([^) ]*).*\\)$" "\\1" path "${image}")
    list(APPEND paths "${path}")
endforeach()

set(checked 0)
set(missing "")
foreach(path IN LISTS paths)
    # An image elsewhere on the web isn't the install's to provide.
    if(path MATCHES "^[a-z]+:")
        continue()
    endif()
    math(EXPR checked "${checked} + 1")
    if(NOT EXISTS "${directory}/${path}")
        list(APPEND missing "${path}")
    endif()
endforeach()

if(checked EQUAL 0)
    message(FATAL_ERROR "The installed README shows no image by a relative path, so there is nothing "
        "to check: ${README}"
    )
endif()
if(missing)
    list(JOIN missing ", " missing)
    message(FATAL_ERROR "The installed README shows images that weren't installed beside it: "
        "${missing}"
    )
endif()
