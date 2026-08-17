add_executable(test_sanity test_sanity.cpp)
add_dependencies(test_sanity sanitypass sanity)
get_target_property(PASS_PATH sanitypass BIN_PATH)
if (NOT PASS_PATH)
    message(FATAL_ERROR "pass not founded")
endif()
get_target_property(LIB_PATH sanity BIN_PATH)
if (NOT LIB_PATH)
    message(FATAL_ERROR "lib not founded")
endif()
if (CPP_STANDARD EQUAL 11)
  target_compile_options(test_sanity
      PRIVATE
      "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-Xclang -load -Xclang ${PASS_PATH}>")
elseif (CPP_STANDARD EQUAL 20)
  target_compile_options(test_sanity
      PRIVATE
      "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-fpass-plugin=${PASS_PATH}>")
else()
  message(FATAL_ERROR "unexpected")
endif()
target_link_libraries(test_sanity PRIVATE ut_base ${LIB_PATH})
