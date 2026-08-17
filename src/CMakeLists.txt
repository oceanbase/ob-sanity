add_library(src_base INTERFACE)
target_include_directories(src_base INTERFACE ${DEVEL_DIR}/include ${CMAKE_SOURCE_DIR}/include)

add_subdirectory(sanity)
