#!/bin/bash
##################################################
# Configure and install systemd services.
#
# The clock uses two .service scripts to auto-boot
# at startup: CPP_clock_build.service and 
# CPP_clock.serivce. The former builds the clock's
# binary and the latter runs the binary.
##################################################


##################################################
# Escape characters that have special meaning in a
# sed replacement.
##################################################
escape_sed_replacement() { 
    printf '%s' "$1" | sed 's/[\\&|]/\\&/g'
}


##################################################
# Configure and install the clock's build service
# using CPP_clock_build.service as a template.
#
# This should not modify the template file.
##################################################
install_build_service() {
    if [[ ! -f "$BUILD_SERVICE" ]]; then
        echo "Error: $BUILD_SERVICE not found in $SCRIPTS_DIR."
        exit 1
    fi

    local rendered_service
    rendered_service="$(mktemp)"

    sed "s|__WORKING_DIRECTORY__|$(escape_sed_replacement "$BUILD_DIR")|g" \
        "$BUILD_SERVICE" > "$rendered_service"
    
    sudo install -m 644 "$rendered_service" /etc/systemd/system/CPP_clock_build.service
    rm -f "$rendered_service"
    sudo systemctl daemon-reload
    sudo systemctl enable --now CPP_clock_build.service
}


##################################################
# Configure and install the clock's startup
# service using CPP_clock.service as a template.
#
# This should not modify the template file.
##################################################
install_clock_service() {
    if [[ ! -f "$CLOCK_SERVICE" ]]; then
        echo "Error: $CLOCK_SERVICE not found in $SCRIPTS_DIR."
        exit 1
    fi

    local rendered_service
    rendered_service="$(mktemp)"

    sed "s|__EXEC_START__|$(escape_sed_replacement "$BINARY")|g" \
        "$CLOCK_SERVICE" > "$rendered_service"

    sudo install -m 644 "$rendered_service" /etc/systemd/system/CPP_clock.service
    rm -f "$rendered_service"
    sudo systemctl daemon-reload
    sudo systemctl enable --now CPP_clock.service
}

##################################################
# Install the clock's .service files. This also
# starts the clock. 
#
# Should be called in install_clock.sh.
##################################################
install_systemd_services() {
    echo "Enabling boot-time startup for the clock and starting the clock (this may take a minute)..."
    install_build_service
    install_clock_service
}