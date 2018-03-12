mkdir -p /builds/proxy/build
cd /builds/proxy/build
cmake -DCMAKE_INSTALL_PREFIX=./ -DCMAKE_BUILD_TYPE=Debug /builds/proxy/hpx/src
make clean
make -j4
sudo make install
echo $PATH
ls /usr/local/bin
which esc
which escd
