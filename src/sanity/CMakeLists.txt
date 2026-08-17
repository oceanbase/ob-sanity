add_library(sanitypass MODULE
    sanitypass.cpp
)
set_target_properties(sanitypass PROPERTIES BIN_PATH ${CMAKE_CURRENT_BINARY_DIR}/libsanitypass.so)

add_library(sanity STATIC
    sanity.cpp
)
target_link_libraries(sanity PRIVATE src_base)
set_target_properties(sanity PROPERTIES BIN_PATH ${CMAKE_CURRENT_BINARY_DIR}/libsanity.a)
