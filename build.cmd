rm -r _build
mkdir _build
cd _build 
cmake --DCMAKE_BUILD_TYPE=Debug
make
cd ..
