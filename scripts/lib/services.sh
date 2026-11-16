#!/bin/bash
##################################################
# Configure and install systemd services.
#
# The clock uses two .service scripts to auto-boot
# at startup: CPP_clock_build.service and 
# CPP_clock.service. The former compiles the
# clock's executable and the latter runs it.
##################################################


##################################################
# Escape characters that have special meaning in a
# sed replacement.
#
# E.g., __EXEC_START__ in CPP_clock.service is
# replaced by the path to the clock's executable.
##################################################
escape_sed_replacement() { 
    printf '%s' "$1" | sed 's/[\\&|]/\\&/g'
}


##################################################
# Configure and install the clock's build service
# as the user using CPP_clock_build.service as a
# template.
#
# This should not modify the template file.
##################################################
install_build_service() {
    if [[ ! -f "$BUILD_SERVICE" ]]; then
        fatal "$BUILD_SERVICE not found in $SCRIPTS_DIR."
        exit 1
    fi

    local rendered_service
    rendered_service="$(mktemp)"

    sed -e "s|__WORKING_DIRECTORY__|$(escape_sed_replacement "$BUILD_DIR")|g" \
        -e "s|__USER__|$(escape_sed_replacement "$(id -un)")|g" \
        "$BUILD_SERVICE" > "$rendered_service"
    
    sudo install -m 644 "$rendered_service" /etc/systemd/system/CPP_clock_build.service
    rm -f "$rendered_service"
    sudo systemctl daemon-reload
    sudo systemctl enable --now CPP_clock_build.service
}


##################################################
# Configure and install the clock's startup
# service as the user using CPP_clock.service as a
# template.
#
# This should not modify the template file.
##################################################
install_clock_service() {
    if [[ ! -f "$CLOCK_SERVICE" ]]; then
        fatal "$CLOCK_SERVICE not found in $SCRIPTS_DIR."
        exit 1
    fi

    local rendered_service
    rendered_service="$(mktemp)"

    sed -e "s|__EXEC_START__|$(escape_sed_replacement "$CLOCK_BINARY")|g" \
        -e "s|__TIME_SYNC_SCRIPT__|$(escape_sed_replacement "$TIME_SYNC_SCRIPT")|g" \
        "$CLOCK_SERVICE" > "$rendered_service"

    sudo install -m 644 "$rendered_service" /etc/systemd/system/CPP_clock.service
    rm -f "$rendered_service"
    sudo systemctl daemon-reload
    sudo systemctl enable CPP_clock.service
    sudo systemctl restart CPP_clock.service

}

##################################################
# Install the clock's .service files. This also
# starts the clock. 
#
# Should be called in install_clock.sh.
##################################################
install_systemd_services() {
    echo ""
    echo "Enabling boot-time startup for the clock and starting the clock (this may take a minute)..."
    install_build_service
    install_clock_service
}