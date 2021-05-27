#!/bin/bash

brew install openssl@1.1 protobuf cmake ninja

rm -rf build
mkdir build
cd build

PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/opt/openssl@1.1/lib/pkgconfig \
    cmake -GNinja ..
ninja
