cmake_minimum_required(VERSION 3.5)
project( ASSETS_CUSTOM VERSION 1.0.0 )

file(GLOB_RECURSE SHADERS_LIST
     "${CMAKE_CURRENT_LIST_DIR}/shaders/*.hlsl"
     "${CMAKE_CURRENT_LIST_DIR}/shaders/*.glsl")

add_custom_target( SHADERS SOURCES ${SHADERS_LIST} )
set_property(TARGET SHADERS PROPERTY FOLDER "ASSETS")



