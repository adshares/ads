cd ..
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=./  -DCMAKE_BUILD_TYPE=Debug ../src
make
make install
cd bin
./installesc.sh
