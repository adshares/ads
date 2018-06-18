#!/bin/bash

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

    apt-get update -qq
    apt-get install -qq openssl libboost-all-dev

    if [ $1 ]; then
        apt-get -qq install libssl-dev
        apt-get -qq install build-essential make cmake
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
    echo $0
    echo $1
    echo $2
#    if [ $1 ]; then
#        shift 1
#        mkdir /ads
#        cd /ads
#        cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PROJECT_CONFIG=esc $* /ads_repo/src
#        make -j `nproc` escd esc
#    fi
}

usage() {
    echo "Compile ads & adsd."
    echo
    echo "Usage"
    echo "  $0 [options] <path-to-source>"
    echo
    echo "Options"
    echo "  -o <dir>       Build output directory (default ./build)"
    echo "  -r             Install all dependencies (may require sudo)"
    echo "  -R             Only install dependencies (may require sudo)"
    echo "  -d             Enable debug build"
    echo "  -m <options>   Make additional options"
    echo "  -c <options>   Cmake additional options"
    echo "  -i             Install binaries (may require sudo)"
    exit 1
}

# Main function

output=./build
build_type=Release
build=1

while getopts ":o:r?R?d?m:c:i?" opt; do
    case "$opt" in
        o)  output=$OPTARG
            ;;
        r)  dependencies=1
            ;;
        R)  dependencies=1
            build=
            ;;
        d)  build_type=Debug
            ;;
        m)  options=$OPTARG
            ;;
        c)  coptions=$OPTARG
            ;;
        i)  install=1
            ;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

if [ -z "$1" ] || [ -n "$2" ] || [ -z "$output" ]
then
    usage
fi

if [ -n "$dependencies" ]
then
    echo "=== Installing dependencies ==="
    install_dependencies $1
fi

if [ -z "$build" ]
then
    echo "=== Build disabled ==="
    exit 0
fi

echo "=== Building ==="
source="$(realpath $1)"
output="$(realpath $output)"

echo "=== Source: $source ==="
echo "=== Output: $output ==="

build=$build_type
if [ -n "$coptions" ]
then
    build="$build | cmake: $coptions"
fi
if [ -n "$options" ]
then
    build="$build | make: $options"
fi
echo "=== Build: $build ==="

mkdir -p $output
#rm -rf $output/*
cd $output

echo "=== Cleaning ==="
make clean

echo "=== Configuring ==="
if ! cmake -DCMAKE_BUILD_TYPE=$build_type -DCMAKE_PROJECT_CONFIG=esc $coptions $source
then
    exit 1
fi

echo "=== Compiling ==="
if ! make -j `nproc` $options escd esc
then
    exit 1
fi

if [ -n "$install" ]
then
    echo "=== Installing binaries ==="
    if ! make -j `nproc` $options install
    then
        exit 1
    fi
fi
echo "=== END ==="
