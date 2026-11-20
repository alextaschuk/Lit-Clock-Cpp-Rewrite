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
        fatal 0 "$BUILD_SERVICE not found in $SCRIPTS_DIR."
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
        fatal 0 "$CLOCK_SERVICE not found in $SCRIPTS_DIR."
        exit 1
    fi

    local rendered_service
    rendered_service="$(mktemp)"

    sed -e "s|__EXEC_START__|$(escape_sed_replacement "$CLOCK_BINARY")|g" \
        -e "s|__SYNC_TIME_SCRIPT__|$(escape_sed_replacement "$SYNC_TIME_SCRIPT")|g" \
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


##################################################
# Disable and delete the clock's two startup
# services.
#
# Should be called in uninstall_clock.sh.
##################################################
uninstall_systemd_services() {
    echo ""
    echo "Stopping the clock and removing its startup services..."

    local service
    local service_file

    # the clock's service is first so that the clock is stopped before anything is deleted
    for service in CPP_clock.service CPP_clock_build.service; do
        service_file="/etc/systemd/system/$service"

        if [[ ! -f "$service_file" ]]; then
            warn 0 "$service is not installed. Moving on..."
            continue
        fi

        sudo systemctl disable --now "$service"
        sudo rm -f "/etc/systemd/system/$service"
        echo "Removed $service."
    done

    sudo systemctl daemon-reload
    sudo systemctl reset-failed CPP_clock.service CPP_clock_build.service 2>/dev/null || true
}