FROM ubuntu:16.04

RUN true \
    && apt-get update && apt-get install -y \
            sudo \
            cmake \
            openssl \
            libboost-all-dev \
            libssl-dev \
            dante-server \
            bash \
            python3 \
            python3-pip \
            python3-dev \
    && pip3 install --upgrade pip \
    && pip3 install -U pytest \
    && true
