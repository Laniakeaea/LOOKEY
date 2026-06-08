cmake_minimum_required(VERSION 3.24)

project(lookey LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(LOOKEY_TOOLCHAIN_ONLY "Build only a toolchain smoke target without UI dependencies" ON)
option(LOOKEY_FETCH_DEPS "Download dependencies from network when local third-party is missing" OFF)
option(LOOKEY_EMBED_ASSETS "Embed runtime assets into LOOKEY.exe" OFF)
option(LOOKEY_STATIC_MINGW_RUNTIME "Statically link MinGW runtime libraries for portable Windows builds" ON)
set(LOOKEY_EMBEDDED_ASSETS_CPP "" CACHE FILEPATH "Generated C++ source that contains embedded runtime assets")

set(LOOKEY_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party")
set(LOOKEY_GLFW_DIR "${LOOKEY_THIRD_PARTY_DIR}/glfw")
set(LOOKEY_IMGUI_DIR "${LOOKEY_THIRD_PARTY_DIR}/imgui")

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

set(LOOKEY_EXECUTABLE_TYPE "")
if (WIN32)
  set(LOOKEY_EXECUTABLE_TYPE WIN32)
endif()

add_executable(lookey ${LOOKEY_EXECUTABLE_TYPE}
  src/layers/main-window/main.cpp
  src/layers/input-overlay-window/input-overlay-window.cpp
  src/layers/input-overlay-window/input-overlay-layout.cpp
  src/layers/input-overlay-window/input-overlay-render.cpp
  src/layers/input-overlay-window/input-overlay-theme.cpp
  src/layers/input-overlay-window/theme/theme-config.cpp
  src/layers/input-overlay-window/style/style-sanitizer.cpp
  src/layers/input-overlay-window/model/overlay-frame-metrics.cpp
  src/layers/input-overlay-window/model/history-buffer.cpp
  src/functions/system/input-api/input-api.cpp
  src/functions/system/input-api/input-event-codec.cpp
  src/functions/system/input-api/input-key-code.cpp
  src/functions/system/input-api/keyboard-input-api.cpp
  src/functions/system/input-api/mouse-input-api.cpp
  src/functions/system/input-api/windows-hook-session.cpp
  src/functions/system/input-middleware/input-semantics-middleware.cpp
  src/common/time/time-source.cpp
)

if (LOOKEY_EMBED_ASSETS)
  if (NOT LOOKEY_EMBEDDED_ASSETS_CPP OR NOT EXISTS "${LOOKEY_EMBEDDED_ASSETS_CPP}")
    message(FATAL_ERROR "LOOKEY_EMBED_ASSETS=ON requires LOOKEY_EMBEDDED_ASSETS_CPP to point to a generated source file")
  endif()
  target_sources(lookey PRIVATE "${LOOKEY_EMBEDDED_ASSETS_CPP}")
  target_compile_definitions(lookey PRIVATE LOOKEY_EMBED_ASSETS=1)
else()
  target_sources(lookey PRIVATE src/common/resource/embedded-assets.cpp)
endif()

set_target_properties(lookey PROPERTIES OUTPUT_NAME LOOKEY)

target_include_directories(lookey PRIVATE
  src
)

target_compile_definitions(lookey PRIVATE
  LOOKEY_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

if (NOT LOOKEY_EMBED_ASSETS)
  add_custom_command(TARGET lookey POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:lookey>/assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:lookey>/assets/app"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:lookey>/assets/fonts/UbuntuMono"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:lookey>/assets/icons/mouse_key_main_parts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/assets/app/Logo.png"
      "$<TARGET_FILE_DIR:lookey>/assets/app/Logo.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/UbuntuMono/UbuntuMonoNerdFontMono-Bold.ttf"
      "$<TARGET_FILE_DIR:lookey>/assets/fonts/UbuntuMono/UbuntuMonoNerdFontMono-Bold.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/UbuntuMono/UbuntuMonoNerdFontMono-BoldItalic.ttf"
      "$<TARGET_FILE_DIR:lookey>/assets/fonts/UbuntuMono/UbuntuMonoNerdFontMono-BoldItalic.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/UbuntuMono/LICENCE.txt"
      "$<TARGET_FILE_DIR:lookey>/assets/fonts/UbuntuMono/LICENCE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/mouse_key_main_parts"
      "$<TARGET_FILE_DIR:lookey>/assets/icons/mouse_key_main_parts"
  )
endif()

if (WIN32)
  target_sources(lookey PRIVATE
    src/layers/main-window/lookey.rc
  )
  target_compile_definitions(lookey PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_link_libraries(lookey PRIVATE user32 gdi32 gdiplus imm32)
endif()

if (MINGW AND LOOKEY_STATIC_MINGW_RUNTIME)
  target_link_options(lookey PRIVATE -static -static-libgcc -static-libstdc++)
endif()

if (LOOKEY_TOOLCHAIN_ONLY)
  message(STATUS "LOOKEY_TOOLCHAIN_ONLY=ON: building compiler and build-system smoke target only")
  return()
endif()

set(LOOKEY_HAS_LOCAL_GLFW OFF)
set(LOOKEY_HAS_LOCAL_IMGUI OFF)

if (EXISTS "${LOOKEY_GLFW_DIR}/CMakeLists.txt")
  set(LOOKEY_HAS_LOCAL_GLFW ON)
endif()

if (EXISTS "${LOOKEY_IMGUI_DIR}/imgui.cpp")
  set(LOOKEY_HAS_LOCAL_IMGUI ON)
endif()

if (LOOKEY_HAS_LOCAL_GLFW AND LOOKEY_HAS_LOCAL_IMGUI)
  add_subdirectory("${LOOKEY_GLFW_DIR}" EXCLUDE_FROM_ALL)
  set(IMGUI_SOURCE_DIR "${LOOKEY_IMGUI_DIR}")
elseif(LOOKEY_FETCH_DEPS)
  include(FetchContent)

  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
  )

  FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG master
  )

  FetchContent_MakeAvailable(glfw imgui)
  set(IMGUI_SOURCE_DIR "${imgui_SOURCE_DIR}")
else()
  set(LOOKEY_MISSING_DEPS "")
  if (NOT LOOKEY_HAS_LOCAL_GLFW)
    set(LOOKEY_MISSING_DEPS "${LOOKEY_MISSING_DEPS} third-party/glfw")
  endif()
  if (NOT LOOKEY_HAS_LOCAL_IMGUI)
    set(LOOKEY_MISSING_DEPS "${LOOKEY_MISSING_DEPS} third-party/imgui")
  endif()

  message(FATAL_ERROR
    "Dependencies not found:${LOOKEY_MISSING_DEPS}. "
    "Provide local dependencies in those folders, or configure with -DLOOKEY_FETCH_DEPS=ON in a network-enabled environment.")
endif()

find_package(OpenGL REQUIRED)

target_sources(lookey PRIVATE
  ${IMGUI_SOURCE_DIR}/imgui.cpp
  ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
  ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
  ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
  ${IMGUI_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(lookey PRIVATE
  ${IMGUI_SOURCE_DIR}
  ${IMGUI_SOURCE_DIR}/backends
)

target_link_libraries(lookey PRIVATE glfw OpenGL::GL)

if (MINGW)
  target_link_libraries(lookey PRIVATE gdi32)
endif()
