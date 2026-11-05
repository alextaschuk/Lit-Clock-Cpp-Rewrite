#!/bin/bash
##################################################
# Get system-dependent and necessary info from the
# user.
##################################################


##################################################
# Get the Pi's model / prompt the user for it.
##################################################
get_pi_model() {
    PI_MODEL=""
    MODEL_SELECTION=""

    if [[ -r /proc/device-tree/model ]]; then
        PI_MODEL=$(tr -d '\0' < /proc/device-tree/model)
    fi

    echo ""

    if [[ -z "$PI_MODEL" ]]; then
        echo "Error: Unable to detect your Pi's model."
        MODEL_CONFIRM="N"
    else
        echo "It looks like you have a $PI_MODEL"
        read -r -p "Is this correct? [Y/N]: " MODEL_CONFIRM
    fi

    case "$MODEL_CONFIRM" in
        [yY]|[yY][eE][sS])
            if [[ "$PI_MODEL" =~ Raspberry\ Pi\ 5 ]]; then
                MODEL_SELECTION=2
            elif [[ "$PI_MODEL" =~ Raspberry\ Pi\ [1-4] ]]; then
                MODEL_SELECTION=1
            else
                echo "Error: Unable to identify a supported Raspberry Pi model."
                MODEL_CONFIRM="N"
            fi
            ;;&

        [nN]|[nN][oO])
            echo ""
            echo "Please select which Raspberry Pi model is closest to yours:"
            echo "1) Raspberry Pi 4 or older"
            echo "2) Raspberry Pi 5 or newer"

            while true; do
                read -r -p "Enter 1 or 2: " MODEL_SELECTION

                if [[ "$MODEL_SELECTION" == "1" || "$MODEL_SELECTION" == "2" ]]; then
                    break
                fi

                echo "Invalid selection. Please enter 1 or 2."
            done
            ;;
    esac
}


##################################################
# Get VCOM value from user input.
##################################################
get_VCOM() {
    while true; do
        read -r -p "Enter the VCOM value printed on the screen's FPC (e.g. -2.79): " VCOM

        if [[ "$VCOM" =~ ^-[0-9]+\.[0-9]{2}$ ]]; then
            break
        fi

        echo "Invalid VCOM value. Try again."
    done
}


##################################################
# Get INCLUDE_CREDITS value from user input.
##################################################
get_INCLUDE_CREDITS () {
    while true; do
        read -r -p "Include the credits (book title & author) under each quote? [Y/N]: " CREDITS_INPUT

        case "$CREDITS_INPUT" in
            [yY]|[yY][eE][sS]|"")
                INCLUDE_CREDITS=true
                break
                ;;
            [nN]|[nN][oO])
                INCLUDE_CREDITS=false
                break
                ;;
            *)
                echo "Invalid input. Please enter Y or N."
                ;;
        esac
    done
}

##################################################
# Call all functions related to user input.
#
# Should be called in install_clock.sh.
##################################################
get_user_config() {
    get_pi_model
    get_VCOM
    get_INCLUDE_CREDITS
}