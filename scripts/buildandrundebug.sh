cd ..
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=./  -DCMAKE_BUILD_TYPE=Debug ../src
make -j4
make install
cd bin
./installesc.sh
