#!/bin/bash
##################################################
# Shared functions, variables, etc across
# install-related scripts.
##################################################


# Project Directories
BUILD_DIR="$ROOT/build"
SHARE_DIR="$ROOT/share"
SCRIPTS_DIR="$ROOT/scripts"

# Project Files
CONSTANTS_FILE="$ROOT/include/constants.hpp"
CMAKE_FILE="$ROOT/CMakeLists.txt"

# Script and service files
BUILD_SERVICE="$SCRIPTS_DIR/systemd/CPP_clock_build.service"
CLOCK_SERVICE="$SCRIPTS_DIR/systemd/CPP_clock.service"
UPDATE_SCRIPT="$SCRIPTS_DIR/update_clock.sh"

# Other files
BINARY="$BUILD_DIR/clock"

# URLs
QUOTE_URL="https://raw.githubusercontent.com/alextaschuk/Literary-Quote-Clock/main/quotes.csv"

# Project Functions
##################################################
# Ensure script was ran correctly.
##################################################
check_environment() {
    if [[ "$EUID" -eq 0 ]]; then
        error "Run this script as your regular user, without sudo."
        exit 1
    fi

    # verify the files that are read/written to during install exist
    local required_file
    for required_file in "$BUILD_SERVICE" "$CLOCK_SERVICE" "$UPDATE_SERVICE" "$UPDATE_SCRIPT" "$CMAKE_FILE"; do
        if [[ ! -f "$required_file" ]]; then
            error "$required_file is missing."
            exit 1
        fi
    done
}


##################################################
# Wrapper function to log something with the time
# that it was logged.
##################################################
log() {
    printf '%s: %s\n' "$(date)" "$*"
}


##################################################
# Print an error message.
##################################################
error() {
    echo "Error: $*" >&2
}
