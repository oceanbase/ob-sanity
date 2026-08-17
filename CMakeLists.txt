cmake_minimum_required(VERSION 3.1)

set(DEVTOOLS_DIR "${CMAKE_SOURCE_DIR}/deps/usr/local/oceanbase/devtools")
set(DEVEL_DIR "${CMAKE_SOURCE_DIR}/deps/usr/local/oceanbase/deps/devel")

find_program(CC clang
      PATHS ${DEVTOOLS_DIR}/bin
        NO_DEFAULT_PATH)
find_program(CXX clang++
      PATHS ${DEVTOOLS_DIR}/bin
        NO_DEFAULT_PATH)
if (CC AND CXX)
  set(CMAKE_C_COMPILER ${CC})
  set(CMAKE_CXX_COMPILER ${CXX})
else()
  message(FATAL_ERROR "can't find suitable compiler")
endif()

project(ob-sanity)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_DWARF_4 "")
if (CPP_STANDARD EQUAL 20)
  set(BUILD_DWARF_4 "-gdwarf-4")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -g ${BUILD_DWARF_4} --gcc-toolchain=${DEVTOOLS_DIR} -DCPP_STANDARD=${CPP_STANDARD}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g ${BUILD_DWARF_4} --gcc-toolchain=${DEVTOOLS_DIR} -DCPP_STANDARD=${CPP_STANDARD}")
set(BUILD_FLAGS "RelWithDebInfo" CACHE INTERNAL "build flags")

if(DEFINED _GLIBCXX_USE_CXX11_ABI)
  add_definitions(-D_GLIBCXX_USE_CXX11_ABI=${_GLIBCXX_USE_CXX11_ABI})
endif()

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS} ${DEVEL_DIR}/lib)
file(COPY include DESTINATION .)
add_subdirectory(src)
if(BUILD_TESTING)
  add_subdirectory(unittest)
endif()
