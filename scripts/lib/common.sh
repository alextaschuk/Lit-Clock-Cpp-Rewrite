#!/bin/bash
##################################################
# Shared functions, variables, etc across
# install-related scripts.
##################################################


# Project Directories
BUILD_DIR="$ROOT/build-release"
SHARE_DIR="$ROOT/share"
SCRIPTS_DIR="$ROOT/scripts"

source "$SCRIPTS_DIR/lib/log.sh"

# Project Files
CONSTANTS_FILE="$ROOT/include/constants.hpp"
CMAKE_FILE="$ROOT/CMakeLists.txt"

# Script and service files
BUILD_SERVICE="$SCRIPTS_DIR/systemd/CPP_clock_build.service"
CLOCK_SERVICE="$SCRIPTS_DIR/systemd/CPP_clock.service"
UPDATE_SCRIPT="$SCRIPTS_DIR/update_clock.sh"
SYNC_TIME_SCRIPT="$SCRIPTS_DIR/lib/wait_sync_time.sh"

# Other files
CLOCK_BINARY="$BUILD_DIR/clock"

# The Python Clock's remote repo (quotes CSV lives here)
PY_REPO="alextaschuk/Literary-Quote-Clock"
BRANCH="main"
FILE="quotes.csv"
QUOTE_URL="https://raw.githubusercontent.com/$PY_REPO/$BRANCH/$FILE"
SHA_FILE="$SHARE_DIR/$FILE.sha" # hash of the most recent CSV version that was downloaded


# Project Functions
##################################################
# Verify that all tools required to install and
# run the clock are present on the system.
# 
# A list of missing tools (if any) is printed.
##################################################
require_tools() {
    local tool
    local missing=()

    for tool in "$@"; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            missing+=("$tool")
        fi
    done

    if (( ${#missing[@]} > 0 )); then
        fatal "Missing required tools: ${missing[*]}. Install them with: sudo apt install ${missing[*]}"
        exit 1
    fi
}


##################################################
# Verify the script was ran correctly, and that
# everything the install needs is present before
# anything on the system is changed.
##################################################
check_environment() {
    if [[ "$EUID" -eq 0 ]]; then
        fatal "Run this script as your regular user, without sudo."
        exit 1
    fi

    # verify the files that are read/written to during install exist
    local required_file
    local missing_files=()
    for required_file in "$BUILD_SERVICE" "$CLOCK_SERVICE" "$UPDATE_SCRIPT" "$TIME_SYNC_SCRIPT" "$CMAKE_FILE"; do
        if [[ ! -f "$required_file" ]]; then
            missing_files+=("$required_file")
        fi
    done

    if (( ${#missing_files[@]} > 0 )); then
        fatal "Missing required files: ${missing_files[*]}. Manually download them, or re-clone the repository."
        exit 1
    fi

    # make sure the necessary tools are present
    require_tools cmake make g++ git wget curl

    local min_gxx_version=14 # installed g++ version must be this or above
    local gxx_version
    gxx_version="$(g++ -dumpversion)"
    gxx_version="${gxx_version%%.*}" # keep the major version only
    if [[ ! "$gxx_version" =~ ^[0-9]+$ ]] || (( gxx_version < min_gxx_version )); then
        fatal "g++ $min_gxx_version or newer is needed to compile the clock, but this system has g++ ${gxx_version:-unknown}."
        exit 1
    fi

    if [[ ! -f "$ROOT/extern/spdlog/CMakeLists.txt" ]]; then
        fatal "The spdlog submodule is missing. Run this from $ROOT, then run the install script again: git submodule update --init"
        exit 1
    fi

}
