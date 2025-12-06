#!/usr/bin/env bash

# Copyright (c) 2025, Arm Limited or its affiliates. All rights reserved.
#  SPDX-License-Identifier : Apache-2.0
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
##

# FWTS Build Script for AArch64 using Buildroot on both Ubuntu x86_64 and AArch64 Native Systems
# Usage: ./build-standalone-fwts.sh
set -e

TOP_DIR="$(pwd)"
FWTS_WORKSPACE="$TOP_DIR/fwts_workspace"
BUILDROOT_DIR="$FWTS_WORKSPACE/buildroot"
BUILDROOT_CONFIG_FILE="buildroot_defconfig"
BUILDROOT_CONFIG_URL="https://raw.githubusercontent.com/ARM-software/arm-systemready/main/common/config/buildroot_defconfig"
CONFIG_URL="https://raw.githubusercontent.com/ARM-software/arm-systemready/refs/heads/main/common/config/systemready-band-source.cfg"
# User can set this variable to a specific version supported by ARM SystemReady to build a specific FWTS version
USER_DEFINED_FWTS_VERSION=""

function get_source() {
    echo "==> Installing dependencies..."
    sudo apt install -y build-essential bison flex libncurses-dev \
        unzip zip rsync python3 perl git pkg-config bc wget curl \
        cmake ninja-build libssl-dev

    if [ -d "$TOP_DIR/fwts_workspace" ]; then
        echo "FWTS worksapce directory already exists."
    else
        echo "Creating FWTS worksapce."
        mkdir -p "$TOP_DIR/fwts_workspace"
    fi

    echo "Downloading configuration file from ARM SystemReady GitHub to retrieve the default supported FWTS version..."
    if [ -f "$FWTS_WORKSPACE/systemready-band-source.cfg" ]; then
        . $FWTS_WORKSPACE/systemready-band-source.cfg
    else
        wget "$CONFIG_URL" -O "$FWTS_WORKSPACE/systemready-band-source.cfg"
        . $FWTS_WORKSPACE/systemready-band-source.cfg
    fi

    echo "==> Downloading Buildroot source code. TAG: $BUILDROOT_SRC_VERSION"
    if [ -d "$BUILDROOT_DIR" ]; then
        echo "Buildroot directory already exists. Skipping clone."
    else
        git clone -b "$BUILDROOT_SRC_VERSION" https://gitlab.com/buildroot.org/buildroot.git "$BUILDROOT_DIR"
    fi

    echo "==> Downloading and applying FWTS patch for either the default arm-systemready version or the used-defined FWTS version..."
    if [ -f "$FWTS_WORKSPACE/$PATCH_FILE" ]; then
        echo "Patch file already exists locally. Skipping download."
    else
        if [ "$USER_DEFINED_FWTS_VERSION" = "" ]; then
            PATCH_URL="https://raw.githubusercontent.com/ARM-software/arm-systemready/main/common/patches/build_fwts_version_$FWTS_VERSION.patch"
            PATCH_FILE="build_fwts_version_$FWTS_VERSION.patch"
        else
            PATCH_URL="https://raw.githubusercontent.com/ARM-software/arm-systemready/main/common/patches/build_fwts_version_$USER_DEFINED_FWTS_VERSION.patch"
            PATCH_FILE="build_fwts_version_$USER_DEFINED_FWTS_VERSION.patch"
        fi
        wget "$PATCH_URL" -O "$FWTS_WORKSPACE/$PATCH_FILE"
    fi

    # Check if patch is already applied
    pushd "$BUILDROOT_DIR/package/fwts" > /dev/null
    if git apply --check "$FWTS_WORKSPACE/$PATCH_FILE"; then
        echo "Applying FWTS patch..."
        git apply "$FWTS_WORKSPACE/$PATCH_FILE"
    else
        echo "Patch may already be applied or failed pre-check. Skipping."
    fi
    popd > /dev/null

    if [ -f "$FWTS_WORKSPACE/$BUILDROOT_CONFIG_FILE" ]; then
        echo "Config file already exists locally. Skipping download."
    else
        echo "Downloading Buildroot Config from ARM SystemReady GitHub..."
        wget "$BUILDROOT_CONFIG_URL" -O "$FWTS_WORKSPACE/$BUILDROOT_CONFIG_FILE"
    fi
}

function build_fwts() {
    cd "$BUILDROOT_DIR"
    echo "==> Cleaning previous build"
    make clean

    echo "==> Configuring Buildroot for AArch64 target."
    cp "$FWTS_WORKSPACE/$BUILDROOT_CONFIG_FILE" "$BUILDROOT_DIR/configs"
    make $BUILDROOT_CONFIG_FILE

    echo "==> Building FWTS..."
    if ! make -j"$(nproc)" fwts; then
        echo "FWTS build failed!" >&2
        exit 1
    fi

    echo "==> Build complete."
    echo "FWTS binaries and dependencies can be found in:"
    echo "  fwts_workspace/buildroot/output/target/usr/bin/fwts"
    echo "  fwts_workspace/buildroot/output/target/usr/bin/kernelscan"
    echo "  fwts_workspace/buildroot/output/target/usr/lib64/fwts/"
    echo "  fwts_workspace/buildroot/output/target/usr/lib/fwts/"
    echo "  fwts_workspace/buildroot/output/target/usr/share/fwts/"
}

function ensure_kernel_version_in_config() {
    cd "$BUILDROOT_DIR"

    # Kernel must be enabled
    if ! grep -q '^BR2_LINUX_KERNEL=y' .config; then
        echo "ERROR: BR2_LINUX_KERNEL is not enabled in Buildroot .config." >&2
        echo "       Fix your buildroot_defconfig (Kernel -> Linux Kernel) and rerun." >&2
        exit 1
    fi

    # Find any version field Buildroot uses
    KVER_LINE=$(grep -E '^BR2_LINUX_KERNEL_(CUSTOM_VERSION_VALUE|VERSION)=' .config || true)
    if [ -z "$KVER_LINE" ]; then
        echo "ERROR: No kernel version configured in Buildroot .config." >&2
        echo "       Expect one of BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE or BR2_LINUX_KERNEL_VERSION." >&2
        echo "       Fix your buildroot_defconfig and rerun the script." >&2
        exit 1
    fi

    KVER=$(printf '%s\n' "$KVER_LINE" | sed 's/.*="//; s/"$//')
    echo "==> Kernel version from Buildroot .config: $KVER"
}

function build_smccc_module() {
    cd "$BUILDROOT_DIR"

    echo "==> Locating FWTS build directory..."
    FWTS_BUILD_DIR_RAW=$(find output/build -maxdepth 1 -type d -name 'fwts-*' | head -n1)
    if [ -z "$FWTS_BUILD_DIR_RAW" ]; then
        echo "ERROR: Could not find FWTS build directory (fwts-* under output/build)" >&2
        exit 1
    fi
    FWTS_BUILD_DIR=$(readlink -f "$FWTS_BUILD_DIR_RAW")
    echo "   FWTS_BUILD_DIR resolved to: $FWTS_BUILD_DIR"

    SMCCC_DIR="$FWTS_BUILD_DIR/smccc_test"
    if [ ! -d "$SMCCC_DIR" ]; then
        echo "ERROR: smccc_test directory not found under $FWTS_BUILD_DIR" >&2
        exit 1
    fi
    if [ ! -f "$SMCCC_DIR/Makefile" ]; then
        echo "ERROR: Makefile not found in $SMCCC_DIR" >&2
        exit 1
    fi

    # Detect kernel build directory
    KDIR_RAW=$(find output/build -maxdepth 1 -type d -name 'linux-*' ! -name '*headers*' | head -n1)

    if [ -z "$KDIR_RAW" ]; then
        echo "==> No Linux kernel build directory found, configuring kernel options in Buildroot (.config)..."

        # Add requested kernel options to .config
        echo "==> Enabling Linux kernel in Buildroot (.config) with arch default config..."
        sed -i '/^BR2_LINUX_KERNEL/d' .config
        sed -i '/^BR2_LINUX_KERNEL_USE_/d' .config
        sed -i '/^BR2_LINUX_KERNEL_DEFCONFIG/d' .config

        cat <<EOF >> .config
# Linux Kernel
BR2_LINUX_KERNEL=y
BR2_LINUX_KERNEL_USE_ARCH_DEFAULT_CONFIG=y
EOF

        echo "==> Running make olddefconfig to refresh Buildroot config..."
        make olddefconfig

        echo "==> Building kernel (make linux)..."
        if ! make linux; then
            echo "ERROR: Linux kernel build failed, cannot build SMCCC module." >&2
            exit 1
        fi

        KDIR_RAW=$(find output/build -maxdepth 1 -type d -name 'linux-*' ! -name '*headers*' | head -n1)
        if [ -z "$KDIR_RAW" ]; then
            echo "ERROR: Linux kernel build directory still not found after building kernel." >&2
            exit 1
        fi
    fi

    KDIR=$(readlink -f "$KDIR_RAW")

    KERNEL_VERSION="${KDIR##*/linux-}"
    echo "==> Using kernel directory: $KDIR"
    echo "==> Detected kernel version (from dir name): $KERNEL_VERSION"

    TOOLCHAIN_PREFIX="$BUILDROOT_DIR/output/host/bin/aarch64-buildroot-linux-gnu-"
    if [ ! -x "${TOOLCHAIN_PREFIX}gcc" ]; then
        echo "ERROR: AArch64 toolchain not found at ${TOOLCHAIN_PREFIX}gcc" >&2
        exit 1
    fi

    echo "==> Preparing kernel for external modules (KDIR=$KDIR)..."
    cd "$KDIR" && make ARCH=arm64 CROSS_COMPILE="$TOOLCHAIN_PREFIX" prepare modules_prepare

    echo "==> Building SMCCC kernel module in $SMCCC_DIR ..."
    make -C "$KDIR" \
         M="$SMCCC_DIR" \
         ARCH=arm64 \
         CROSS_COMPILE="$TOOLCHAIN_PREFIX" \
         modules

    echo "==> SMCCC module build complete: $SMCCC_DIR."
}

get_source
build_fwts
build_smccc_module
