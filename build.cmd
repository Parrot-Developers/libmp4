#!/bin/bash
cmake -S . -B build
cd build 
cmake --build . --config RelWithDebInfo
cmake --build . --config Debug
cd ..
