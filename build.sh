#!/bin/bash

if [ ! -d build ]; then
  cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS="-stdlib=libc++" -DCMAKE_BUILD_TYPE=Debug -GNinja -Bbuild
fi

cmake --build build

glslc -fshader-stage=vertex shader/vertex.glsl -o build/vertex.spv
glslc -fshader-stage=fragment shader/fragment.glsl -o build/fragment.spv
