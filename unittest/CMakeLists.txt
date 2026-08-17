add_library(ut_base INTERFACE)
target_include_directories(ut_base INTERFACE ${DEVEL_DIR}/include ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(ut_base INTERFACE pthread gtest dl -static-libgcc -static-libstdc++)

add_subdirectory(sanity)
