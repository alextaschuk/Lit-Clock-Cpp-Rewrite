#!/bin/bash
#
# This script pulls changes from the remote repo's main branch.
#
# Since the Python Clock is where the most up-to-date version of the quotes CSV file lives, the
# hash of the most recent CSV on the Python Clock's repo is compared against the its last
# recorded hash on the Pi. If the hashes don't match (changes have been pushed to the CSV since
# the last check), download the CSV and overwrite the current local version, then update the
# file to use the CPP formatting delimiters.
#
# Finally, the Pi is restarted.

set -euo pipefail # exit the script if a command fails at any point.

echo "$(date): Begin C++ Clock's update script."

PY_REPO="alextaschuk/Literary-Quote-Clock"
BRANCH="main"
FILE="quotes.csv"
CSV_URL="https://raw.githubusercontent.com/$PY_REPO/$BRANCH/$FILE"

REPO_DIR="__REPO_DIR__"
SHARE_DIR="$REPO_DIR/share"
CSV_FILE="$SHARE_DIR/$FILE"
SHA_FILE="$SHARE_DIR/$FILE.sha"

echo "$(date): Attempting to pull from remote repo."
git -C "$REPO_DIR" pull

echo "$(date): Checking the remote CSV version of Python Clock..."
REMOTE_SHA=$(curl -fsS \
    "https://api.github.com/repos/$PY_REPO/contents/$FILE?ref=$BRANCH" \
    | grep -m1 '"sha":' \
    | cut -d'"' -f4)

if [[ -z "$REMOTE_SHA" ]]; then
    echo "$(date): Error: Could not determine the remote CSV's SHA."
    exit 1
fi

LOCAL_SHA=$(cat "$SHA_FILE" 2>/dev/null || true)

if [ "$REMOTE_SHA" != "$LOCAL_SHA" ]; then
    echo "$(date): CSV changed since last check. Downloading new version..."

    TEMP_CSV="$(mktemp "$CSV_FILE.tmp.XXXXXX")"
    trap 'rm -f "$TEMP_CSV"' EXIT

    curl -fLsS "$CSV_URL" -o "$TEMP_CSV"

    echo "$(date): Updating delimiter characters in the CSV..."
    sed -i \
        -e 's/\*/\\*/g' \
        -e 's/_/\\_/g' \
        -e 's/◻/_/g' \
        -e 's/◯/*/g' \
        -e 's/␤/\\n/g' \
        -e 's/⇇/\\n\\n/g' \
        "$TEMP_CSV"

    mv "$TEMP_CSV" "$CSV_FILE"

    echo "$REMOTE_SHA" > "$SHA_FILE" # update with the most recent hash
    trap - EXIT

    echo "$(date): Successfully updated the CSV."
else
    echo "$(date): No changes to the CSV."
fi

echo "$(date): Restarting the Pi..."
sudo shutdown -r now
