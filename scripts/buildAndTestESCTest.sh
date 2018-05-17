cd ..
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=./ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PROJECT_CONFIG=esc ../src
make clean
make -j4
make install