#!/bin/bash
##################################################
# Script to uninstall the clock from a user's
# system.
##################################################


set -euo pipefail

U_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)" # path to this file
ROOT="$(dirname "$U_SCRIPT_DIR")"

source "$U_SCRIPT_DIR/lib/common.sh"

source "$U_SCRIPT_DIR/lib/cron.sh"
source "$U_SCRIPT_DIR/lib/dependencies.sh"
source "$U_SCRIPT_DIR/lib/services.sh"


##################################################
# Make sure that the script was ran correctly and
# that ROOT is the clock's folder, since it's
# deleted at the end.
##################################################
check_uninstall_environment() {
    if [[ "$EUID" -eq 0 ]]; then
        fatal 0 "Run this script as your regular user, without sudo."
        exit 1
    fi

    if [[ -z "$ROOT" || "$ROOT" == "/" || "$ROOT" == "$HOME" ]]; then
        fatal 0 "Refusing to uninstall: $ROOT is not the clock's folder."
        exit 1
    fi

    if [[ ! -f "$CMAKE_FILE" || ! -f "$CONSTANTS_FILE" || ! -f "$SCRIPTS_DIR/uninstall_clock.sh" ]]; then
        fatal 0 "Refusing to uninstall: $ROOT does not look like the clock's folder."
        exit 1
    fi
}


##################################################
# Ask the user to confirm, since the uninstall
# can't be undone.
##################################################
confirm_uninstall() {
    local answer

    echo "This script will:"
    echo "  - Stop the clock and remove its startup services."
    echo "  - Remove the auto-update cron job."
    echo "  - Remove the GPIO library, IFF the clock's install script installed it."
    echo "  - Permanently delete $ROOT and everything in it."
    echo "*Note*: This action cannot be undone."
    echo ""

    while true; do
        read -r -p "Uninstall the clock? [Y/N]: " answer
        case "$answer" in
            [yY]|[yY][eE][sS])
                break ;;
            [nN]|[nN][oO])
                echo "Uninstall cancelled. Nothing was changed."
                exit 0
                ;;
            *)
                echo "Invalid input. Please enter Y or N." ;;
        esac
    done
}


##################################################
# Delete the clock's folder.
##################################################
delete_clock_dir() {
    echo ""
    echo "Deleting $ROOT..."

    cd / # don't delete the directory that the shell is in

    # The GPIO library is installed with sudo, which can leave files in extern/ that only
    # root is able to delete. So if deleting as the user fails, it is retried with sudo.
    if ! rm -rf -- "${ROOT:?}" 2>/dev/null; then
        sudo rm -rf -- "${ROOT:?}"
    fi

    echo "Deleted $ROOT."
}


# The uninstall process is run from a function so that bash has read this whole file before
# the file is deleted along with the rest of the clock's folder.
main() {
    check_uninstall_environment

    echo "┌────────────────────────────┐"
    echo "│Literary Quote Clock Install│"
    echo "└────────────────────────────┘"

    confirm_uninstall

    # If one of these steps fails the script stops here, before the folder (and this script)
    # is deleted, so that it can be run again.
    uninstall_systemd_services  # services.sh
    remove_clock_cron_job       # cron.sh
    uninstall_dependencies      # dependencies.sh
    delete_clock_dir

    echo ""
    echo "Clock uninstall complete. To reinstall, visit https://github.com/alextaschuk/Lit-Clock-Cpp-Rewrite"
}

main "$@"
exit 0