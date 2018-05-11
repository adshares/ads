cd ..
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PROJECT_CONFIG=esc ../src
make clean
make -j4
make install
cd ../scripts
./installescQA.sh 7290 8290 8290 "FALSE"
nohup ./fulltest.sh > test.txt &
