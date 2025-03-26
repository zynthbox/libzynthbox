#!/bin/bash

set -ex

SCRIPTPATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

# Stop all running heavy processes
systemctl stop jack2 zynthbox-qml zynthian-webconf zynthian-webconf-fmserver

(
# Create build dir and cd into it
mkdir -p $SCRIPTPATH/build && cd $SCRIPTPATH/build

# Configure
cmake -DCMAKE_INSTALL_SYSCONFDIR=/etc -DCMAKE_INSTALL_LOCALSTATEDIR=/var -DCMAKE_EXPORT_NO_PACKAGE_REGISTRY=ON -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF -DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DCMAKE_INSTALL_RUNSTATEDIR=/run "-GUnix Makefiles" -DCMAKE_VERBOSE_MAKEFILE=ON -DCMAKE_INSTALL_LIBDIR=lib/arm-linux-gnueabihf -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo -DPRINT_DEBUG_LOGS=1 ..

# Build
make -j$(nproc) "INSTALL=install --strip-program=true" VERBOSE=1

# Install
make -j$(nproc) install AM_UPDATE_INFO_DIR=no "INSTALL=install --strip-program=true"
)

# Start the stopped processes
systemctl start jack2 zynthbox-qml zynthian-webconf zynthian-webconf-fmserver
