#!/bin/bash
##################################################
# Config for the clock's program
##################################################


##################################################
# Set VCOM value in constants.hpp from user input.
##################################################
set_VCOM() {
    if ! grep -qE '^[[:space:]]*inline constexpr double VCOM' "$CONSTANTS_FILE"; then
        warn 0 "VCOM declaration not found in constants.hpp."
        return 1
    fi

    sed -i -E \
    "s|^([[:space:]]*inline constexpr double VCOM[[:space:]]*=[[:space:]]*).*;|\1${VCOM};|" \
    "$CONSTANTS_FILE"

    grep -Eq "VCOM[[:space:]]*=[[:space:]]*${VCOM};" "$CONSTANTS_FILE" \
    || { fatal 0 "Failed to set VCOM in constants.hpp"; exit 1; }

    echo "Set VCOM value in constants.hpp to $VCOM."
}


##################################################
# Set INCLUDE_CREDITS value in constants.hpp from
# user input.
##################################################
set_INCLUDE_CREDITS() {
    if ! grep -qE '^[[:space:]]*inline constexpr bool INCLUDE_CREDITS' "$CONSTANTS_FILE"; then
        warn 0 "INCLUDE_CREDITS declaration not found in constants.hpp."
        return 1
    fi

    sed -i -E \
    "s|^([[:space:]]*inline constexpr bool INCLUDE_CREDITS[[:space:]]*=[[:space:]]*).*;|\1${INCLUDE_CREDITS};|" \
    "$CONSTANTS_FILE"

    grep -Eq "INCLUDE_CREDITS[[:space:]]*=[[:space:]]*${INCLUDE_CREDITS};" "$CONSTANTS_FILE" \
    || { fatal 0 "Failed to set INCLUDE_CREDITS in constants.hpp"; exit 1; }

    echo "Set INCLUDE_CREDITS value in constants.hpp to $INCLUDE_CREDITS."
}


##################################################
# Update constants.hpp variables with user input.
##################################################
update_constants() {
    set_VCOM
    set_INCLUDE_CREDITS
}

##################################################
# Generate the Clock's build files.
##################################################
generate_build_files() {
    echo -e "Generating build files...\n"
    local gpio_backend

    case "$MODEL_SELECTION" in
        1) gpio_backend=BCM2835 ;; # configure CMAKE for RPi <= 4
        2) gpio_backend=LGPIO ;; # configure CMAKE for RPi >= 5
        *) fatal 0 "Invalid Raspberry Pi model selection: $MODEL_SELECTION"; exit 1 ;;
    esac

    cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DBUILD_CLOCK=ON -DGPIO_BACKEND="$gpio_backend"
}


##################################################
# Build the clock and image generator executables.
#
# CPP_build_clock.service repeats this every time
# the RPi boots up, but this show's the compiler's
# progress and any errors instead of hiding them
# in systemd's journal.
##################################################
build_clock() {
    echo ""
    echo "Building the clock (this make take a moment)..."
    cmake --build "$BUILD_DIR"
}


##################################################
# Call all config-related functions.
# 
# Should be called in install_clock.sh.
################################################## 
configure_clock() {
    echo ""
    echo "Configuring the clock..."

    update_constants
    generate_build_files
    build_clock
}
