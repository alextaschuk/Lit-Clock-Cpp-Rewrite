#!/bin/bash
##################################################
# Install necessary libraries into extern/,
# depending on which model Pi the user has.
##################################################


EXTERN_DIR="$ROOT/extern"

# If the clock installs a fresh GPIO library system-wide,
# a file is created and placed in the library's source
# folder in extern/ after it is installed. The uninstall
# script will only remove the library from the system if
# the file is present.
INSTALLED_BY_CLOCK_MARKER=".installed_by_clock"


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
        echo "bcm2835 is already installed system-wide at /usr/local/. Using existing library..."
        return 0
    fi

    mkdir -p "$EXTERN_DIR" || return 1

    # skip download if bcm2835 is already present
    if [[ -x "$lib_dir/configure" ]]; then
        echo "Using existing bcm2835 source in $lib_dir"
    else
        echo "Downloading bcm2835 ${version} to ${lib_dir}"

        download_dir="$(mktemp -d "${lib_dir}-download.XXXXXX")" || return 1

        (
            trap 'rm -rf "$download_dir"' EXIT

            rm -rf "${lib_dir:?}" &&
            mkdir -p "$lib_dir" &&
            wget -q --show-progress -O "$download_dir/$archive" "$download_url" &&
            tar -xzf "$download_dir/$archive" -C "$lib_dir" --strip-components=1
        ) || { fatal "Failed to download bcm2835." >&2; return 1; }
    fi

    echo "Installing bcm2835 library..."
    (
        cd "$lib_dir" &&
        ./configure &&
        make -j"$(nproc)" &&
        { sudo make check || warn "bcm2835's self-test failed. Installing it anyway." >&2; } &&
        sudo make install
    ) || { fatal "Failed to build/install bcm2835." >&2; return 1; }

    touch "$lib_dir/$INSTALLED_BY_CLOCK_MARKER" # remove system-wide installation in uninstall script
}


##################################################
# Removes the system-wide bcm2835 library.
#
# This function should uninstall bcm2835 only
# when a .installed_by_clock file is present at
# extern/bcm2835/.
##################################################
uninstall_bcm2835() {
    local lib_dir="$EXTERN_DIR/bcm2835"
    local marker="$lib_dir/$INSTALLED_BY_CLOCK_MARKER"

    if [[ ! -f "$marker" ]]; then
        if [[ -f /usr/local/include/bcm2835.h ]]; then
            echo "bcm2835 was not installed by the clock, so it has been left in place."
        fi
        return 0
    fi

    echo "Removing the bcm2835 library that the clock installed..."

    # Let bcm2835 remove its own files first. If that isn't possible (e.g. make is missing),
    # the two files that it installs are still deleted below.
    if [[ -f "$lib_dir/Makefile" ]] && command -v make >/dev/null 2>&1; then
        ( cd "$lib_dir" && sudo make uninstall >/dev/null 2>&1 ) || true
    fi

    sudo rm -f /usr/local/include/bcm2835.h /usr/local/lib/libbcm2835.a
    rm -f "${marker:?}"
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

    require_tools unzip # see common.sh

    mkdir -p "$EXTERN_DIR" || return 1

    if [[ -f "$lib_dir/Makefile" ]]; then
        echo "Using existing lg source in $lib_dir"
    else
        echo "Downloading lg library..."
        (
            trap 'rm -f "$archive"' EXIT

            wget -q --show-progress -O "$archive" https://github.com/joan2937/lg/archive/master.zip &&
            unzip -q -o "$archive" -d "$EXTERN_DIR"
        ) || { echo "Failed to download lg." >&2; return 1; }
    fi

    echo "Installing lg library..."
    (
        cd "$lib_dir" &&
        make -j"$(nproc)" &&
        sudo make install
    ) || { echo "Failed to build/install lg." >&2; return 1; }

    touch "$lib_dir/$INSTALLED_BY_CLOCK_MARKER" # remove system-wide installation in uninstall script
}

##################################################
# Removes the system-wide lg library.
#
# This function should uninstall lg only when if a
# .installed_by_clock file is present at
# extern/lg/.
##################################################
uninstall_lg() {
    local lib_dir="$EXTERN_DIR/lg-master"
    local marker="$lib_dir/$INSTALLED_BY_CLOCK_MARKER"
    local lg_file

    if [[ ! -f "$marker" ]]; then
        if [[ -f /usr/local/include/lgpio.h ]]; then
            echo "The lg library was not installed by the clock, so it has been left in place."
        fi
        return 0
    fi

    echo "Removing the lg library that the clock installed..."

    # Let lg remove its own files first. If that isn't possible (e.g. make is missing), the
    # files that it installs are still deleted below.
    if [[ -f "$lib_dir/Makefile" ]] && command -v make >/dev/null 2>&1; then
        ( cd "$lib_dir" && sudo make uninstall >/dev/null 2>&1 ) || true
    fi

    for lg_file in /usr/local/lib/liblgpio.so* /usr/local/lib/librgpio.so*; do
        sudo rm -f "$lg_file"
    done

    sudo rm -f /usr/local/include/lgpio.h /usr/local/include/rgpio.h \
        /usr/local/bin/rgpiod /usr/local/bin/rgs \
        /usr/local/man/man1/rgpiod.1 /usr/local/man/man1/rgs.1 \
        /usr/local/man/man3/lgpio.3 /usr/local/man/man3/rgpio.3

    sudo ldconfig # refresh the system's list of shared libraries
    rm -f "${marker:?}"
}

##################################################
# Installs all external dependencies that the
# clock relies on.
#
# Should be called in install_clock.sh.
##################################################
install_dependencies() {
    echo ""
    
    case "$MODEL_SELECTION" in
        1) 
            echo "Installing bcm2835 GPIO library. *Note*: This installation is system-wide."
            install_bcm2835
            ;;
        2) 
            echo "Installing lg GPIO library."
            install_lg
            ;;
        *)
            error "Invalid Raspberry Pi model selection: $MODEL_SELECTION"
            ;;
    esac

    echo ""
    echo "GPIO library installation complete."
}


##################################################
# Removes the GPIO library that the install script
# installed system-wide. A library that was
# already on the system before the clock was
# installed is left alone.
#
# Should be called in uninstall_clock.sh.
##################################################
uninstall_dependencies() {
    echo ""
    uninstall_bcm2835
    uninstall_lg
}