add_library(imgui OBJECT
  imgui.cpp
  imgui_demo.cpp
  imgui_draw.cpp
  imgui_tables.cpp
  imgui_widgets.cpp
  imgui_impl_sdl3.cpp
  imgui_impl_sdlgpu3.cpp)
target_link_libraries(imgui PUBLIC SDL3::SDL3)
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})