#!/bin/bash
##################################################
# Config for the clock's program
##################################################


##################################################
# Update VCOM value in constants.hpp from user
# input.
##################################################
update_VCOM() {
    if ! grep -qE '^[[:space:]]*inline constexpr double VCOM' "$CONSTANTS_FILE"; then
        echo "Error: VCOM declaration not found in constants.hpp." >&2
        return 1
    fi

    sed -i -E \
    "s|^([[:space:]]*inline constexpr double VCOM[[:space:]]*=[[:space:]]*).*;|\1${VCOM};|" \
    "$CONSTANTS_FILE"

    grep -Eq "VCOM[[:space:]]*=[[:space:]]*${VCOM};" "$CONSTANTS_FILE" \
    || { echo "Error: failed to set VCOM in constants.hpp"; exit 1; }

    echo "Updated VCOM value in constants.hpp to $VCOM."
}


##################################################
# Update INCLUDE_CREDITS value in constants.hpp
# from user input.
##################################################
update_INCLUDE_CREDITS() {
    if ! grep -qE '^[[:space:]]*inline constexpr bool INCLUDE_CREDITS' "$CONSTANTS_FILE"; then
        echo "Error: INCLUDE_CREDITS declaration not found in constants.hpp." >&2
        return 1
    fi

    sed -i -E \
    "s|^([[:space:]]*inline constexpr bool INCLUDE_CREDITS[[:space:]]*=[[:space:]]*).*;|\1${INCLUDE_CREDITS};|" \
    "$CONSTANTS_FILE"

    grep -Eq "INCLUDE_CREDITS[[:space:]]*=[[:space:]]*${INCLUDE_CREDITS};" "$CONSTANTS_FILE" \
    || { echo "Error: failed to set INCLUDE_CREDITS in constants.hpp"; exit 1; }

    echo "Updated INCLUDE_CREDITS value in constants.hpp to $INCLUDE_CREDITS."
}


##################################################
# Update constants.hpp variables with user input.
##################################################
update_constants() {
    echo "updating constants.hpp..."
    update_VCOM
    update_INCLUDE_CREDITS
}

##################################################
# Generate the Clock's build files.
##################################################
generate_build_files() {
    echo "Generating build files..."
    local gpio_backend

    case "$MODEL_SELECTION" in
        1) gpio_backend=BCM2835 ;;  # configure CMAKE for RPi <= 4
        2) gpio_backend=LGPIO ;;    # configure CMAKE for RPi >= 5
        *) error "Invalid Raspberry Pi model selection: $MODEL_SELECTION" ;;
    esac

    cmake -S "$ROOT" -B "$BUILD_DIR" -DGPIO_BACKEND="$gpio_backend"
}


##################################################
# Call all config-related functions.
# 
# Should be called in install_clock.sh.
################################################## 
configure_clock() {
    echo "Configuring the clock..."

    update_constants
    generate_build_files
}