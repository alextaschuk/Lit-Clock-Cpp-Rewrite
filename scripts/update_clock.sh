#!/bin/bash
#
# To run this with a cron job once a day at 4:00 A.M., run
# sudo crontab -e
# Then, add the following in the file that opens:
# 0 4 * * * bash /path/to/Lit-Clock-Cpp-Rewrite/scripts/update_clock.sh >> /home/user/update_clock.log 2>&1
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

REPO_DIR="/home/user/path/to/clock"  # Modify to point to the repo's root
DEST="$REPO_DIR/share/quotes.csv"
SHA_FILE="$REPO_DIR/share/quotes.csv.sha"

PY_REPO="alextaschuk/Literary-Quote-Clock"
BRANCH="main"
FILE="quotes.csv"

echo "$(date): Attempting to pull from remote repo."
git -C "$REPO_DIR" pull

echo "$(date): Attempting to download newer CSV."
REMOTE_SHA=$(curl -fsS \
    "https://api.github.com/repos/$PY_REPO/commits?path=$FILE&sha=$BRANCH&per_page=1" \
    | grep -m1 '"sha":' \
    | cut -d'"' -f4)

LOCAL_SHA=$(cat "$SHA_FILE" 2>/dev/null || true)

if [ "$REMOTE_SHA" != "$LOCAL_SHA" ]; then
    echo "$(date): CSV changed since last check. Downloading new version..."

    if curl -fL -o "$DEST" \
        "https://raw.githubusercontent.com/$PY_REPO/$BRANCH/$FILE"; then

        echo "$(date): Updating delimiter characters in the CSV..."
        sed -i \
            -e 's/\*/\\*/g' \
            -e 's/\_/\\_/g' \
            -e 's/◻/_/g' \
            -e 's/◯/*/g' \
            -e 's/␤/\\n/g' \
            -e 's/⇇/\\n\\n/g' \
            "$DEST"

        echo "$REMOTE_SHA" > "$SHA_FILE" # update with the most recent hash
    else
        echo "$(date): CSV failed to download."
        exit 1
    fi
else
    echo "$(date): No changes to the CSV."
fi

echo "$(date): Restarting the Pi..."
sudo shutdown -r now
