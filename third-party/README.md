# Third-party dependencies

Place dependency source trees here for offline build.

Required layout:

- third-party/glfw
- third-party/imgui

Expected files:

- third-party/glfw/CMakeLists.txt
- third-party/imgui/imgui.cpp
- third-party/imgui/backends/imgui_impl_glfw.cpp
- third-party/imgui/backends/imgui_impl_opengl3.cpp

If network is available, you can also enable fetch mode:

cmake -S . -B build -G Ninja -DLOOKEY_FETCH_DEPS=ON
