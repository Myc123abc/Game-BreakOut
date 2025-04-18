#!/bin/bash

if [ ! -d build ]; then
  cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS="-stdlib=libc++" -DCMAKE_BUILD_TYPE=Debug -GNinja -Bbuild
fi

cmake --build build

glslc -fshader-stage=vertex shader/vertex.glsl -o build/vertex.spv
glslc -fshader-stage=fragment shader/fragment.glsl -o build/fragment.spv
glslc -fshader-stage=compute shader/compute.glsl -o build/compute.spv
glslc -fshader-stage=compute shader/gradient_color.comp -o build/gradient_color.spv
glslc -fshader-stage=vertex shader/triangle.vert -o build/triangle_vert.spv
glslc -fshader-stage=fragment shader/triangle.frag -o build/triangle_frag.spv
glslc -fshader-stage=vertex shader/triangle_mesh.vert -o build/triangle_mesh_vert.spv
