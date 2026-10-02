cmake_minimum_required(VERSION 3.13)
project(flutter_inappwebview_linux_webkitgtk LANGUAGES CXX)

set(PLUGIN_NAME "flutter_inappwebview_linux_webkitgtk_plugin")

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTK IMPORTED_TARGET gtk+-3.0)
pkg_check_modules(WEBKIT2GTK IMPORTED_TARGET webkit2gtk-4.1)
pkg_check_modules(JAVASCRIPTCORE IMPORTED_TARGET javascriptcoregtk-4.1)
pkg_check_modules(LIBSOUP IMPORTED_TARGET libsoup-3.0)

set(_scomm_linux_webkitgtk_missing "")
if(NOT GTK_FOUND)
  list(APPEND _scomm_linux_webkitgtk_missing "gtk+-3.0")
endif()
if(NOT WEBKIT2GTK_FOUND)
  list(APPEND _scomm_linux_webkitgtk_missing "webkit2gtk-4.1")
endif()
if(NOT JAVASCRIPTCORE_FOUND)
  list(APPEND _scomm_linux_webkitgtk_missing "javascriptcoregtk-4.1")
endif()
if(NOT LIBSOUP_FOUND)
  list(APPEND _scomm_linux_webkitgtk_missing "libsoup-3.0")
endif()
if(_scomm_linux_webkitgtk_missing)
  string(REPLACE ";" ", " _scomm_linux_webkitgtk_missing_str
         "${_scomm_linux_webkitgtk_missing}")
  message(FATAL_ERROR
    "Scomm Linux WebKitGTK backend requires:\n"
    "  gtk+-3.0\n"
    "  webkit2gtk-4.1\n"
    "  javascriptcoregtk-4.1\n"
    "  libsoup-3.0\n"
    "\n"
    "WebKitGTK 4.0 / libsoup 2 are no longer supported.\n"
    "Missing: ${_scomm_linux_webkitgtk_missing_str}")
endif()
unset(_scomm_linux_webkitgtk_missing)
unset(_scomm_linux_webkitgtk_missing_str)

message(STATUS
  "flutter_inappwebview_linux_webkitgtk: GTK ${GTK_VERSION}, "
  "WebKitGTK ${WEBKIT2GTK_VERSION}, "
  "JavaScriptCoreGTK ${JAVASCRIPTCORE_VERSION}, "
  "libsoup ${LIBSOUP_VERSION}")

set(PLUGIN_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/webview_plugin.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/webview_plugin_lifecycle_handlers.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/webview_plugin_content_handlers.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/webview_webkitgtk_flutter_settings.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/webview_webkitgtk.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/webview_overlay_window.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/webview_overlay_coords.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/webview_overlay_embedded.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/webview_overlay_popup.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/webview_native_health.cc"
)

add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SOURCES})

apply_standard_settings(${PLUGIN_NAME})

set_target_properties(${PLUGIN_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)
target_compile_definitions(${PLUGIN_NAME} PRIVATE "$<$<NOT:$<CONFIG:Debug>>:NDEBUG>")

target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(${PLUGIN_NAME} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  ${WEBKIT2GTK_INCLUDE_DIRS}
  ${JAVASCRIPTCORE_INCLUDE_DIRS}
)

target_link_libraries(${PLUGIN_NAME} PRIVATE flutter)
target_link_libraries(${PLUGIN_NAME} PRIVATE PkgConfig::GTK)
target_link_libraries(${PLUGIN_NAME} PRIVATE PkgConfig::WEBKIT2GTK)
target_link_libraries(${PLUGIN_NAME} PRIVATE PkgConfig::JAVASCRIPTCORE)
target_link_libraries(${PLUGIN_NAME} PRIVATE PkgConfig::LIBSOUP)

set(flutter_inappwebview_linux_webkitgtk_bundled_libraries
  ""
  PARENT_SCOPE
)
