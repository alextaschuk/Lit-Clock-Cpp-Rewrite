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

# Project Files
SHA_FILE="$SHARE_DIR/quotes.csv.sha"

source "$SCRIPT_DIR/lib/common.sh"
source "$SCRIPT_DIR/lib/quotes.sh"

echo "┌───────────────────────────────────────┐"
echo "$(date): Begin C++ Clock's update script."
echo "└───────────────────────────────────────┘"


##################################################
# Attempt to restart the Pi. Print error message
# if unsuccessful.
##################################################
restart_pi() {
    echo "$(date): Update complete. Restarting the Pi..."
    if ! sudo -n shutdown -r now; then
        echo "$(date): Update completed, but restart failed (passwordless sudo should be set up for shutdown)" >&2
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
    echo "$(date): Attempting to pull from remote repo..."

    local constants_changed=false
    local constants_was_staged=false
    local constants_backup=""
    
    if ! git -C "$REPO_DIR" diff --quiet HEAD -- "$CONSTANTS_FILE"; then
        echo "$(date): Temporarily setting aside local constants.hpp changes."

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


check_remote_repo
update_csv

if [[ "$REPO_UPDATED" == true || "$CSV_UPDATED" == true ]]; then
    restart_pi
else
    echo "$(date): No updates, so restart not required."
fi
