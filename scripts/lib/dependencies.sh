#!/bin/bash
##################################################
# Install necessary libraries into extern/,
# depending on which model Pi the user has.
##################################################


EXTERN_DIR="$ROOT/extern"

##################################################
# Installs the bcm2835 library for RPi <= 4.
# see https://www.waveshare.com/wiki/6inch_HD_e-Paper_HAT#Working_with_Raspberry_Pi_.28SPI.29
##################################################
install_bcm2835() {
    echo ""
    local version="1.60"
    local archive="bcm2835-${version}.tar.gz"
    local download_url="https://www.airspayce.com/mikem/bcm2835/${archive}"
    local download_dir
    local lib_dir="$EXTERN_DIR/bcm2835"

    if [[ -f /usr/local/include/bcm2835.h ]]; then
        echo "bcm2835 is already installed at /usr/local/."
        return 0
    fi

    mkdir -p "$EXTERN_DIR" || return 1

    # skip download if bcm2835 is already present
    if [[ -x "$lib_dir/configure" ]]; then
        echo "Using existing bcm2835 source in $lib_dir"
    else
        echo "Downloading bcm2835 ${version}... to ${lib_dir}"

        download_dir="$(mktemp -d "${lib_dir}-download.XXXXXX")" || return 1

        (
            set -e
            trap 'rm -rf "$download_dir"' EXIT
            wget -q --show-progress -O "$download_dir/$archive" "$download_url"
            rm -rf "${lib_dir:?}/"*
            tar -xzf "$download_dir/$archive" -C "$download_dir"
            mv "$download_dir/bcm2835-${version}" "$lib_dir"
        ) || { echo "Failed to download bcm2835." >&2; return 1; }
    fi

    echo "Installing bcm2835 library..."
    (
        set -e
        cd "$lib_dir" || exit
        ./configure
        make -j"$(nproc)"
        sudo make check
        sudo make install
    ) || { echo "Failed to build/install bcm2835." >&2; return 1; }
}


##################################################
# Install the lg library for RPi >= 5.
# https://www.waveshare.com/wiki/6inch_HD_e-Paper_HAT#Working_with_Raspberry_Pi_.28SPI.29
##################################################
install_lg() {
    echo ""
    local lib_dir="$EXTERN_DIR/lg-master"
    local archive="$EXTERN_DIR/lg-master.zip"

    if [[ -f /usr/local/include/lgpio.h ]]; then
        echo "lg library is already installed"
        return 0
    fi

    mkdir -p "$EXTERN_DIR" || return 1

    if [[ -f "$lib_dir/Makefile" ]]; then
        echo "Using existing lg source in $lib_dir"
    else
        echo "Downloading lg library..."
        (
            set -e
            trap 'rm -f "$archive"' EXIT
            wget -q --show-progress -O "$archive" https://github.com/joan2937/lg/archive/master.zip
            unzip -q -o "$archive" -d "$EXTERN_DIR"
        ) || { echo "Failed to download lg." >&2; return 1; }
    fi

    echo "Installing lg library..."
    (
        set -e
        cd "$lib_dir"
        make -j"$(nproc)"
        sudo make install
    ) || { echo "Failed to build/install lg." >&2; return 1; }
}

##################################################
# Installs all external dependencies that the
# clock relies on.
#
# Should be called in install_clock.sh.
##################################################
install_dependencies() {
    case "$MODEL_SELECTION" in
        1) install_bcm2835 ;;
        2) install_lg ;;
        *) error "Invalid Raspberry Pi model selection: $MODEL_SELECTION" ;;
    esac

    echo ""
    echo "GPIO library installation complete."
}