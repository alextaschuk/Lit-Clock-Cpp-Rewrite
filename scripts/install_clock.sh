#!/bin/bash

##################################################
# Script to install the clock on a user's system.
#
# There are two ways to run the script:
#
#   1. Download the script (e.g. via curl). This
#   clones the clock's repo and runs the copy
#   of this script inside the clone.
#
#    bash install_clock.sh [parent_dir]
#
#   The repo is cloned into
#   parent_dir/Lit-Clock-Cpp-Rewrite. If no folder
#   is given, the current folder is used.
#
#   2. Run the script inside an existing clone of
#   of the repo.
#
#    bash scripts/install_clock.sh
##################################################


set -euo pipefail

REPO_URL="https://github.com/alextaschuk/Lit-Clock-Cpp-Rewrite.git"
REPO_NAME="Lit-Clock-Cpp-Rewrite"


# BASH_SOURCE is empty when the script is piped into bash after downloading it, so it should
# fall back to the current folder by default.
I_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-.}")" && pwd)" # path to this file


##################################################
# Checks whether the script is running from
# inside a clone of the clock's repo.
##################################################
is_inside_repo() {
    [[ -f "$I_SCRIPT_DIR/../src/clock/horologium.cpp" && -f "$I_SCRIPT_DIR/../CMakeLists.txt" ]]
}


##################################################
# Log an error and exit. Used before log.sh is
# available.
##################################################
bootstrap_fail() {
    echo "Error: $*" >&2
    exit 1
}


##################################################
# Recursively clone the C++ Clock's repo into
# parent-dir/Lit-Clock-Cpp-Rewrite, then
# hand over to the install script in the clone.
#
# If the clock's repo is already at that location,
# it is reused instead of cloned again, so this
# script can be run again after a failed install.
##################################################
clone_and_install() {
    local parent_dir="${1:-$PWD}"
    local repo_dir

    if [[ "$EUID" -eq 0 ]]; then
        bootstrap_fail "Run this script as your regular user, without sudo."
    fi

    if ! command -v git >/dev/null 2>&1; then
        bootstrap_fail "git is needed to download the clock. Install it with: sudo apt install git"
    fi

    mkdir -p -- "$parent_dir" || bootstrap_fail "Could not create the folder $parent_dir."
    parent_dir="$(cd -- "$parent_dir" && pwd)" # make the path absolute
    repo_dir="$parent_dir/$REPO_NAME"

    if [[ -e "$repo_dir" ]]; then
        local existing_url
        existing_url="$(git -C "$repo_dir" remote get-url origin 2>/dev/null || true)"

        if [[ "$existing_url" != *"alextaschuk/$REPO_NAME"* ]]; then
            bootstrap_fail "$repo_dir already exists and is not a copy of the clock's repository. Move it, or pass a different folder to this script."
        fi

        echo "The clock's repository is already at $repo_dir. Using it..."
        git -C "$repo_dir" submodule update --init --recursive \
            || bootstrap_fail "Could not download the clock's submodules into $repo_dir."
    else
        echo "Downloading the clock..."
        git clone --recurse-submodules "$REPO_URL" "$repo_dir" \
            || bootstrap_fail "Could not download the clock from $REPO_URL."
    fi

    # delete the downloaded script and continue with the install using the script in the local repo
    local downloaded_script
    downloaded_script="$I_SCRIPT_DIR/$(basename "${BASH_SOURCE[0]:-}")"
    if [[ -f "$downloaded_script" ]]; then
        rm -f -- "$downloaded_script"
        echo "Deleted the downloaded install script. To run the install again, use: bash $repo_dir/scripts/install_clock.sh"
    fi

    # if the script is piped into bash, its input is the pipe instead, so the terminal is used
    if [[ ! -t 0 ]] && { : < /dev/tty; } 2>/dev/null; then
        exec bash "$repo_dir/scripts/install_clock.sh" < /dev/tty
    fi
    exec bash "$repo_dir/scripts/install_clock.sh"
}

echo "┌──────────────────────────┐"
echo "│Literary Quote Clock Setup│"
echo "└──────────────────────────┘"

# if the script was downloaded on its own, clone the repo and run the clone's copy of this script (which continues below)
if ! is_inside_repo; then
    clone_and_install "$@"
fi

if (( $# > 0 )); then
    echo "Error: This script is already inside the clock's repo ($(dirname "$I_SCRIPT_DIR")), so it installs from there." >&2
    echo "A folder can only be passed to a copy of the script that was downloaded on its own." >&2
    exit 1
fi


ROOT="$(dirname "$I_SCRIPT_DIR")"

source "$I_SCRIPT_DIR/lib/common.sh"
source "$I_SCRIPT_DIR/lib/configure.sh"
source "$I_SCRIPT_DIR/lib/cron.sh"
source "$I_SCRIPT_DIR/lib/dependencies.sh"
source "$I_SCRIPT_DIR/lib/quotes.sh"
source "$I_SCRIPT_DIR/lib/services.sh"
source "$I_SCRIPT_DIR/lib/user_config.sh"


main() {
    check_environment # common.sh

    # TODO: modify echo to automatically wrap text according to terminal width.
    echo -e "This script walks you through all of the necessary services install the clock, \nenable boot-time startup for the clock, \nand have it automatically update (e.g. receive new quotes)."

    get_user_config             # user_config.sh
    install_dependencies        # dependencies.sh
    configure_clock             # configure.sh
    install_quotes_csv          # quotes.sh
    create_clock_cron_job       # cron.sh
    install_systemd_services    # services.sh

    echo ""
    echo "Clock setup complete."
}

main "$@"
exit 0
