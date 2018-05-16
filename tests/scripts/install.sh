mkdir -p /builds/proxy/hpx/build
cd /builds/proxy/hpx/build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_PROJECT_CONFIG=esc -DCMAKE_BUILD_TYPE=Debug /builds/proxy/hpx/src
make clean
make -j4
