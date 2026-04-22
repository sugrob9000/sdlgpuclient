cmake_minimum_required(VERSION 3.28)
project(rrcc CXX)
set(CMAKE_CXX_STANDARD 23)

find_package(SDL3 REQUIRED)
find_package(fmt REQUIRED)
find_program(glslc glslc REQUIRED)

add_executable(app src/main.cpp)
target_include_directories(app PRIVATE src)
target_link_libraries(app PRIVATE fmt::fmt SDL3::SDL3)

function(compile_shaders target)
  cmake_parse_arguments(PARSE_ARGV 0 arg "" "" "SHADERS")

  set(shader_src_base ${CMAKE_CURRENT_SOURCE_DIR}/shader-src)
  set(shader_out_base ${CMAKE_CURRENT_BINARY_DIR}/compiled-shaders)

  foreach(shader ${arg_SHADERS})
    set(source ${shader_src_base}/${shader})
    set(spirv ${shader_out_base}/${shader}.spv)
    set(depfile ${shader_out_base}/${shader}.d)

    add_custom_command(
      OUTPUT ${spirv}
      DEPENDS ${source}
      DEPFILE ${depfile}
      COMMAND ${glslc} -MD -MF ${depfile}
        -O
        --target-env=vulkan1.0 -mfmt=bin
        -o ${spirv} ${source})
    target_sources(${target} PRIVATE ${spirv})
  endforeach()

  target_compile_options(${target} PRIVATE --embed-dir=${shader_out_base})
endfunction()

compile_shaders(app SHADERS
  fillcolor.vert
  fillcolor.frag
  textured.frag)