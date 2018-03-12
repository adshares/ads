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
            python \
            python-pip \
            python-dev \
    && pip install --upgrade pip \
    && true
