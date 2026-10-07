# CEF packaging patterns adapted from cefsimple: Copyright (c) 2014 The
# Chromium Embedded Framework Authors. BSD license in THIRD_PARTY_NOTICES.md.
# Native CEF is deliberately opt-in; the root policy build stays portable.
if(NOT APPLE)
  message(FATAL_ERROR "The CEF engineering preview supports macOS only. Build with KUNLUN_BUILD_CEF=OFF for portable policy tests.")
endif()
option(USE_SANDBOX "Enable the CEF sandbox" ON)
if(NOT USE_SANDBOX)
  message(FATAL_ERROR "The CEF engineering preview requires USE_SANDBOX=ON.")
endif()
enable_language(C)
enable_language(OBJCXX)

# One pinned minimal distribution per architecture, not a universal SDK.
if(CMAKE_OSX_ARCHITECTURES)
  set(_kunlun_arch "${CMAKE_OSX_ARCHITECTURES}")
else()
  set(_kunlun_arch "${CMAKE_SYSTEM_PROCESSOR}")
endif()
if(_kunlun_arch STREQUAL "arm64")
  set(_kunlun_artifact macosarm64)
elseif(_kunlun_arch STREQUAL "x86_64")
  set(_kunlun_artifact macosx64)
else()
  message(FATAL_ERROR "CEF requires a single macOS architecture: arm64 or x86_64 (got '${_kunlun_arch}').")
endif()
set(PROJECT_ARCH "${_kunlun_arch}")
file(READ "${PROJECT_SOURCE_DIR}/cef/pin.json" _kunlun_pin)
string(JSON _kunlun_version GET "${_kunlun_pin}" cefVersion)
string(JSON _kunlun_archive GET "${_kunlun_pin}" artifacts "${_kunlun_artifact}" filename)
string(REGEX REPLACE "\\.tar\\.bz2$" "" _kunlun_sdk_dir "${_kunlun_archive}")
if(NOT CEF_ROOT)
  set(CEF_ROOT "${PROJECT_SOURCE_DIR}/.deps/cef/${_kunlun_sdk_dir}" CACHE PATH "Pinned CEF binary distribution")
endif()
get_filename_component(CEF_ROOT "${CEF_ROOT}" ABSOLUTE)
if(NOT EXISTS "${CEF_ROOT}/include/cef_version.h")
  message(FATAL_ERROR "Pinned CEF SDK not found at '${CEF_ROOT}'. Fetch the ${_kunlun_artifact} artifact from cef/pin.json or set CEF_ROOT.")
endif()
file(STRINGS "${CEF_ROOT}/include/cef_version.h" _kunlun_sdk_version
  REGEX "^#define CEF_VERSION \"")
if(NOT _kunlun_sdk_version STREQUAL "#define CEF_VERSION \"${_kunlun_version}\"")
  message(FATAL_ERROR "CEF_ROOT version does not match cef/pin.json: expected '${_kunlun_version}', got '${_kunlun_sdk_version}'.")
endif()
set(_kunlun_framework "${CEF_ROOT}/Release/Chromium Embedded Framework.framework")
execute_process(
  COMMAND /usr/bin/lipo -archs "${_kunlun_framework}/Chromium Embedded Framework"
  RESULT_VARIABLE _kunlun_lipo_result OUTPUT_VARIABLE _kunlun_sdk_arch
  ERROR_VARIABLE _kunlun_lipo_error OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _kunlun_lipo_result EQUAL 0 OR NOT _kunlun_sdk_arch STREQUAL _kunlun_arch)
  message(FATAL_ERROR "CEF Release framework architecture must be '${_kunlun_arch}'; got '${_kunlun_sdk_arch}'. ${_kunlun_lipo_error}")
endif()

list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)
# Minimal archives contain only Release. Debug consumers still load that framework.
set(CEF_BINARY_DIR "${CEF_ROOT}/Release")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_EXTENSIONS OFF)
# The policy target was created in the parent directory before CEF selected its
# deployment baseline. Keep every object linked into the app on that baseline.
target_compile_options(kunlun_host_policy PRIVATE "-mmacosx-version-min=${CEF_TARGET_SDK}")
set_target_properties(kunlun_host_policy PROPERTIES OSX_ARCHITECTURES "${_kunlun_arch}")
add_subdirectory("${CEF_LIBCEF_DLL_WRAPPER_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/libcef_dll_wrapper")
set_target_properties(libcef_dll_wrapper PROPERTIES
  CXX_STANDARD 20 XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++20")

find_package(Python3 3.12 REQUIRED COMPONENTS Interpreter)
set(_kunlun_generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(_kunlun_asset_header "${_kunlun_generated}/embedded_assets.h")
add_custom_command(OUTPUT "${_kunlun_asset_header}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${_kunlun_generated}"
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/embed_assets.py"
    --assets "${PROJECT_SOURCE_DIR}/assets" --output "${_kunlun_asset_header}"
  DEPENDS "${PROJECT_SOURCE_DIR}/tools/embed_assets.py"
    "${PROJECT_SOURCE_DIR}/assets/index.html" "${PROJECT_SOURCE_DIR}/assets/app.css"
    "${PROJECT_SOURCE_DIR}/assets/app.js" "${PROJECT_SOURCE_DIR}/assets/bridge.js"
  VERBATIM)

add_library(kunlun_cef_host STATIC app.cc client.cc bundle.cc "${_kunlun_asset_header}")
SET_LIBRARY_TARGET_PROPERTIES(kunlun_cef_host)
target_include_directories(kunlun_cef_host PUBLIC
  "${PROJECT_SOURCE_DIR}/src" "${_kunlun_generated}" "${CEF_ROOT}")
target_link_libraries(kunlun_cef_host PUBLIC kunlun_host_policy libcef_dll_wrapper)
set_target_properties(kunlun_cef_host PROPERTIES
  CXX_STANDARD 20 XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++20")

# Keep bundle paths configuration-independent for Ninja, Make and Xcode.
set(CEF_TARGET_OUT_DIR "${PROJECT_BINARY_DIR}/bin")
set(CEF_APP "${CEF_TARGET_OUT_DIR}/Kunlun Desktop.app")
function(kunlun_cef_executable target output_name plist)
  SET_EXECUTABLE_TARGET_PROPERTIES(${target})
  target_link_libraries(${target} PRIVATE kunlun_cef_host libcef_dll_wrapper ${CEF_STANDARD_LIBS})
  target_compile_definitions(${target} PRIVATE CEF_USE_SANDBOX)
  set_target_properties(${target} PROPERTIES
    OUTPUT_NAME "${output_name}" MACOSX_BUNDLE_INFO_PLIST "${plist}"
    CXX_STANDARD 20 OBJCXX_STANDARD 20
    XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++20"
    RUNTIME_OUTPUT_DIRECTORY "${CEF_TARGET_OUT_DIR}")
  foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set_target_properties(${target} PROPERTIES
      "RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${CEF_TARGET_OUT_DIR}")
  endforeach()
endfunction()

set(EXECUTABLE_NAME "Kunlun Desktop")
set(PRODUCT_NAME "${EXECUTABLE_NAME}")
set(VERSION_SHORT "${PROJECT_VERSION}")
configure_file(mac/Info.plist.in "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" @ONLY)
add_executable(kunlun_desktop MACOSX_BUNDLE main_mac.mm)
kunlun_cef_executable(kunlun_desktop "${EXECUTABLE_NAME}" "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
# CEF's macros only apply these flags to C/CXX, not Objective-C++.
target_compile_options(kunlun_desktop PRIVATE
  "$<$<COMPILE_LANGUAGE:OBJCXX>:${CEF_COMPILER_FLAGS};${CEF_CXX_COMPILER_FLAGS};-fobjc-arc>"
  "$<$<AND:$<COMPILE_LANGUAGE:OBJCXX>,$<CONFIG:Debug>>:${CEF_COMPILER_FLAGS_DEBUG};${CEF_CXX_COMPILER_FLAGS_DEBUG}>"
  "$<$<AND:$<COMPILE_LANGUAGE:OBJCXX>,$<CONFIG:Release>>:${CEF_COMPILER_FLAGS_RELEASE};${CEF_CXX_COMPILER_FLAGS_RELEASE}>")
set_target_properties(kunlun_desktop PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
# The upstream copy macro uses ln -sf, which follows existing directory
# symlinks on repeated builds. Start with a clean generated framework so it
# cannot accumulate broken nested links in Libraries, Resources or Versions/A.
add_custom_command(TARGET kunlun_desktop PRE_LINK
  COMMAND "${CMAKE_COMMAND}" -E rm -rf
    "${CEF_APP}/Contents/Frameworks/Chromium Embedded Framework.framework"
  VERBATIM)
COPY_MAC_FRAMEWORK(kunlun_desktop "${CEF_BINARY_DIR}" "${CEF_APP}")

# Package after target dependencies, not file-level link dependencies on helper
# names containing parentheses (which Make interprets as archive members).
add_custom_target(kunlun_desktop_bundle ALL DEPENDS kunlun_desktop)

# All upstream helper variants are required, including GPU and renderer helpers.
foreach(suffix_list ${CEF_HELPER_APP_SUFFIXES})
  string(REPLACE ":" ";" suffix_list "${suffix_list}")
  list(GET suffix_list 0 name_suffix)
  list(GET suffix_list 1 target_suffix)
  list(GET suffix_list 2 BUNDLE_ID_SUFFIX)
  set(helper_target "kunlun_desktop_helper${target_suffix}")
  set(EXECUTABLE_NAME "Kunlun Desktop Helper${name_suffix}")
  set(PRODUCT_NAME "${EXECUTABLE_NAME}")
  set(helper_plist "${CMAKE_CURRENT_BINARY_DIR}/helper-Info${target_suffix}.plist")
  configure_file(mac/helper-Info.plist.in "${helper_plist}" @ONLY)
  add_executable(${helper_target} MACOSX_BUNDLE helper_mac.cc)
  kunlun_cef_executable(${helper_target} "${EXECUTABLE_NAME}" "${helper_plist}")
  add_dependencies(kunlun_desktop ${helper_target})
  add_dependencies(kunlun_desktop_bundle ${helper_target})
  # This small packaging step also runs when only a helper or plist changes.
  add_custom_command(TARGET kunlun_desktop_bundle POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "$<TARGET_BUNDLE_DIR:${helper_target}>"
      "${CEF_APP}/Contents/Frameworks/${EXECUTABLE_NAME}.app"
    VERBATIM)
endforeach()

add_custom_command(TARGET kunlun_desktop_bundle POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CEF_APP}/Contents/Resources/licenses"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${CEF_ROOT}/LICENSE.txt" "${CEF_ROOT}/CREDITS.html" "${CEF_APP}/Contents/Resources/licenses"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    "${CEF_APP}/Contents/Resources/licenses"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${PROJECT_SOURCE_DIR}/cef/pin.json" "${CEF_APP}/Contents/Resources/pin.json"
  VERBATIM)

if(BUILD_TESTING)
  add_test(NAME native_host
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/check_native.py"
      "$<TARGET_FILE:kunlun_desktop>")
  set_tests_properties(native_host PROPERTIES TIMEOUT 90 LABELS "native;integration")
endif()

message(STATUS "Unqualified CEF engineering preview bundle: ${CEF_APP}")
