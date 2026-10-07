cmake_minimum_required(VERSION 3.21)
project(KunlunDesktop VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KUNLUN_BUILD_CEF "Build the macOS CEF engineering preview" OFF)

add_library(kunlun_host_policy STATIC src/host/policy.cc)
target_include_directories(kunlun_host_policy PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
if(MSVC)
  target_compile_options(kunlun_host_policy PRIVATE /W4 /WX)
else()
  target_compile_options(kunlun_host_policy PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(kunlun_policy_tests tests/policy_test.cc)
  target_link_libraries(kunlun_policy_tests PRIVATE kunlun_host_policy)
  add_test(NAME host_policy COMMAND kunlun_policy_tests)

  find_package(Python3 3.12 REQUIRED COMPONENTS Interpreter)
  add_test(NAME build_tools
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s tests -p "*_test.py" -v)
  set_tests_properties(build_tools PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

  find_program(NODE_EXECUTABLE node)
  if(NODE_EXECUTABLE)
    add_test(NAME presentation_bridge
      COMMAND "${NODE_EXECUTABLE}" --test tests/bridge.test.mjs)
    set_tests_properties(presentation_bridge PROPERTIES
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  else()
    message(WARNING "Node.js was not found; presentation bridge tests will not run")
  endif()
endif()

if(KUNLUN_BUILD_CEF)
  add_subdirectory(src/cef)
endif()
