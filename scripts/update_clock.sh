#!/bin/bash
##################################################
# Pull changes from the remote repo's main branch.
#
# Since the Python Clock is where the most
# up-to-date version of the quotes CSV file lives,
# the hash of the most recent CSV on the Python
# Clock's repo is compared against the its last
# recorded hash on the Pi. If the hashes don't
# match (changes have been pushed to the CSV since
# the last check), download the CSV and overwrite the
# current local version, then update the file to
# use the CPP formatting delimiters.
##################################################

set -euo pipefail # exit the script if a command fails at any point.

# Project Directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)" # script must be at the top of the scripts/ dir
ROOT="$REPO_DIR"

source "$SCRIPT_DIR/lib/common.sh"
source "$SCRIPT_DIR/lib/quotes.sh"


echo -e "\n\n$(date): Begin clock's update script."


##################################################
# Attempt to restart the Pi after downloading an
# update.
##################################################
restart_pi() {
    log 1 "Update complete. Restarting the Pi."
    if ! sudo -n shutdown -r now; then
        warn 1 "Update completed, but restart failed (passwordless sudo should be set up for shutdown)." >&2
        return 1
    fi
}


##################################################
# Attempt to pull changes from the C++ Clock's
# remote repo. Stashes changes to constansts.hpp
# before and restores it afterward.
#
# TODO: improve handling for constanst.hpp. Maybe
# use some local device-specific file that 
# constants.hpp includes.
##################################################
check_remote_repo() {
    REPO_UPDATED=false
    log 1 "Attempting to pull from remote repo..."

    local constants_changed=false
    local constants_was_staged=false
    local constants_backup=""
    
    if ! git -C "$REPO_DIR" diff --quiet HEAD -- "$CONSTANTS_FILE"; then
        log 1 "Temporarily stashing local constants.hpp changes."

        constants_backup="$(mktemp)"
        cp "$CONSTANTS_FILE" "$constants_backup"

        if ! git -C "$REPO_DIR" diff --cached --quiet -- "$CONSTANTS_FILE"; then
            constants_was_staged=true
        fi
        git -C "$REPO_DIR" restore --source=HEAD --staged --worktree -- "$CONSTANTS_FILE"
        constants_changed=true
    fi

    BEFORE_COMMIT="$(git -C "$REPO_DIR" rev-parse HEAD)"
    if git -C "$REPO_DIR" pull --ff-only origin "$BRANCH"; then
        :
    else
        local pull_status=$?
        if [[ "$constants_changed" == true ]]; then
            cp "$constants_backup" "$CONSTANTS_FILE"

            if [[ "$constants_was_staged" == true ]]; then
                git -C "$REPO_DIR" add -- "$CONSTANTS_FILE"
            else
                git -C "$REPO_DIR" restore --staged -- "$CONSTANTS_FILE"
            fi

            rm -f "$constants_backup"
        fi
        return "$pull_status"
    fi

    AFTER_COMMIT="$(git -C "$REPO_DIR" rev-parse HEAD)"

    if [[ "$BEFORE_COMMIT" != "$AFTER_COMMIT" ]]; then
        REPO_UPDATED=true
    fi

    if [[ "$constants_changed" == true ]]; then
        cp "$constants_backup" "$CONSTANTS_FILE"

        if [[ "$constants_was_staged" == true ]]; then
            git -C "$REPO_DIR" add -- "$CONSTANTS_FILE"
        else
            git -C "$REPO_DIR" restore --staged -- "$CONSTANTS_FILE"
        fi

        rm -f "$constants_backup"
    fi
}



main() {
    check_remote_repo
    update_csv

    if [[ "$REPO_UPDATED" == true || "$CSV_UPDATED" == true ]]; then
        restart_pi
    else
        log 1 "No updates found, so a restart isn't required."
    fi
}

main "$@"
exit 0
