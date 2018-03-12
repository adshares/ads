cd ..
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=./  -DEXECUTE_LOCAL_TEST=True -DCMAKE_BUILD_TYPE=Debug ../src
make clean
make -j4
make install
cd ../scripts
./installesc.sh 7290 8290 8290 "TRUE"
