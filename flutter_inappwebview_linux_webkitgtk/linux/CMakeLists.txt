cmake_minimum_required(VERSION 3.13)
project(flutter_inappwebview_linux_webkitgtk LANGUAGES CXX)

set(PLUGIN_NAME "flutter_inappwebview_linux_webkitgtk_plugin")

find_package(PkgConfig REQUIRED)

pkg_check_modules(WEBKIT2GTK webkit2gtk-4.1)
if(NOT WEBKIT2GTK_FOUND)
  pkg_check_modules(WEBKIT2GTK webkit2gtk-4.0)
  if(NOT WEBKIT2GTK_FOUND)
    message(FATAL_ERROR "WebKitGTK not found. Install libwebkit2gtk-4.1-dev or libwebkit2gtk-4.0-dev")
  endif()
  message(STATUS "flutter_inappwebview_linux_webkitgtk: Using WebKitGTK 4.0")
else()
  message(STATUS "flutter_inappwebview_linux_webkitgtk: Using WebKitGTK 4.1")
endif()

pkg_check_modules(JAVASCRIPTCORE javascriptcoregtk-4.1)
if(NOT JAVASCRIPTCORE_FOUND)
  pkg_check_modules(JAVASCRIPTCORE javascriptcoregtk-4.0)
  if(NOT JAVASCRIPTCORE_FOUND)
    message(FATAL_ERROR "JavaScriptCore not found. Install libjavascriptcoregtk-4.1-dev or libjavascriptcoregtk-4.0-dev")
  endif()
endif()

pkg_check_modules(GTK REQUIRED IMPORTED_TARGET gtk+-3.0)

set(PLUGIN_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/webview_plugin.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/webview_plugin_lifecycle_handlers.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/webview_plugin_content_handlers.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/webview_webkitgtk_flutter_settings.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/webview_webkitgtk.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/webview_overlay_window.cc"
)

add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SOURCES})

apply_standard_settings(${PLUGIN_NAME})

set_target_properties(${PLUGIN_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)
target_compile_definitions(${PLUGIN_NAME} PRIVATE WEBVIEW_ENABLE_DEBUG_PRINTS)
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
target_link_libraries(${PLUGIN_NAME} PRIVATE ${WEBKIT2GTK_LIBRARIES})
target_link_libraries(${PLUGIN_NAME} PRIVATE ${JAVASCRIPTCORE_LIBRARIES})

target_link_directories(${PLUGIN_NAME} PRIVATE ${WEBKIT2GTK_LIBRARY_DIRS})
target_compile_options(${PLUGIN_NAME} PRIVATE ${WEBKIT2GTK_CFLAGS_OTHER})

set(flutter_inappwebview_linux_webkitgtk_bundled_libraries
  ""
  PARENT_SCOPE
)
