#!/usr/bin/env sh

install_dependencies() {

    command -v apt-get
    if [ $? -eq 0 ]; then
        install_apt_dependencies $1
        return 0
    fi

    return 1

    # Following code in unsupported.

    command -v yum
    if [ $? -eq 0 ]; then
        install_yum_dependencies $1
        return 0
    fi

    command -v apk
    if [ $? -eq 0 ]; then
        install_apk_dependencies $1
        return 0
    fi

    return 1
}

install_apt_dependencies() {

    apt-get update -y
    apt-get install -y openssl libboost-all-dev

    if [ $1 ]; then
        apt-get -y install libssl-dev
        apt-get -y install build-essential make cmake
    fi
}

# Unsupported
install_yum_dependencies() {

    yum check-update
    yum install -y openssl boost-devel.x86_64

    if [ $1 ]; then
        yum install -y gcc72.x86_64 gcc72-c++.x86_64
        yum install -y openssl-devel.x86_64
        yum install -y make cmake
    fi
}

# Unsupported
install_apk_dependencies() {

    apk update
    apk add openssl
    apk add boost-dev

    if [ $1 ]; then
        apk add openssl-dev
        apk add gcc g++
        apk add make cmake
    fi
}

compile() {
    if [ $1 ]; then
        shift 1
        mkdir /ads
        cd /ads
        cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PROJECT_CONFIG=esc $* /ads_repo/src
        make -j `nproc` escd esc
    fi
}

# Main function

# Say 'please' to compile, otherwise it will only install dependencies

install_dependencies $1
if [ $? -eq 0 ]; then
    compile $1
fi
