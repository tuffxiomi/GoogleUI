cmake_minimum_required(VERSION 3.22)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_CXX_COMPILER_TARGET "aarch64-linux-android28")

project(GoogleUI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(PRELOADER_SO "" CACHE FILEPATH "Exact ARM64 libpreloader.so supplied with the target LeviLauncher build")
if(NOT PRELOADER_SO OR NOT EXISTS "${PRELOADER_SO}")
    message(FATAL_ERROR "Set PRELOADER_SO to the exact ARM64 libpreloader.so path")
endif()

add_library(google_ui SHARED
    src/main.cpp
    src/RuntimeState.cpp
    src/RuntimeLog.cpp
    src/RuntimeSupport.cpp
    src/ModContextResolver.cpp
    src/TextUtil.cpp
    src/JniHelpers.cpp
    src/ActivityResolver.cpp
    src/SettingsStorage.cpp
    src/ResponsiveLayout.cpp
    src/WebViewCommon.cpp
    src/IconData.cpp
    src/GoogleChrome.cpp
    src/AdBlockClientDex.cpp
    src/AdBlockData.cpp
    src/AdBlocker.cpp
    src/AndroidWebViewController.cpp
    src/WebsiteChrome.cpp
    src/WebsiteWebViewController.cpp
    src/FloatingButtonController.cpp
    src/DrawCommandAppender.cpp
    src/ModMenuBridgeHooks.cpp
)

set_target_properties(google_ui PROPERTIES
    OUTPUT_NAME "google_ui"
    PREFIX "lib"
    SUFFIX ".so"
    BUILD_RPATH ""
    INSTALL_RPATH ""
    SKIP_BUILD_RPATH TRUE
)

target_include_directories(google_ui PRIVATE
    include
    third_party/minjni
)

target_compile_options(google_ui PRIVATE
    --target=aarch64-linux-android28
    -fvisibility=hidden
    -fvisibility-inlines-hidden
    -fno-exceptions
    -fno-rtti
    -fno-stack-protector
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -ffreestanding
    -nostdinc++
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

target_link_options(google_ui PRIVATE
    --target=aarch64-linux-android28
    -fuse-ld=lld
    -nostdlib
    -nodefaultlibs
    "LINKER:-s"
    "LINKER:--build-id=sha1"
    "LINKER:-z,max-page-size=16384"
    "LINKER:--unresolved-symbols=ignore-all"
)

target_link_libraries(google_ui PRIVATE "${PRELOADER_SO}")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json.in"
    "${CMAKE_CURRENT_BINARY_DIR}/manifest.json"
    COPYONLY
)

set(PACKAGE_ROOT "${CMAKE_CURRENT_BINARY_DIR}/package")
set(PACKAGE_DIR "${PACKAGE_ROOT}/google_ui")
set(PACKAGE_FILE "${CMAKE_CURRENT_BINARY_DIR}/libGoogleUI_V1.0.0.levipack")

add_custom_target(levi_package
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${PACKAGE_ROOT}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PACKAGE_DIR}/config"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PACKAGE_DIR}/resources/adblock"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:google_ui>" "${PACKAGE_DIR}/libgoogle_ui.so"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_BINARY_DIR}/manifest.json" "${PACKAGE_DIR}/manifest.json"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/config/config.json" "${PACKAGE_DIR}/config/config.json"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/config/config.schema.json" "${PACKAGE_DIR}/config/config.schema.json"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/resources/google_button.png" "${PACKAGE_DIR}/resources/google_button.png"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/compatibility/target.json" "${PACKAGE_DIR}/resources/compatibility.json"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/resources/adblock/hosts.txt" "${PACKAGE_DIR}/resources/adblock/hosts.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/resources/adblock/url_tokens.txt" "${PACKAGE_DIR}/resources/adblock/url_tokens.txt"
    COMMAND "${CMAKE_COMMAND}" -E tar "cf" "${PACKAGE_FILE}" --format=zip -C "${PACKAGE_ROOT}" google_ui
    DEPENDS google_ui
    VERBATIM
)
