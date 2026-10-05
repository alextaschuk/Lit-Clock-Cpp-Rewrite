#!/bin/bash
#
# To run this with a cron job once a day at 4:00 A.M., run
# sudo crontab -e
# Then, add the following in the file that opens:
# 0 4 * * * bash /path/to/clock/scripts/update_clock.sh
#
# This script pulls changes from the remote repo.
#
# Since the Python Clock is where the most up-to-date version of the quotes CSV file lives, the
# hash of the most recent CSV on the Python Clock's repo is compared against the its last
# recorded hash on the Pi. If the hashes don't match (changes have been pushed to the CSV since
# the last check), download the CSV and overwrite the current local version, then update the
# file to use the CPP formatting delimiters.
#
# Finally, the Pi is restarted.

set -e # exit the script if a command fails at any point.

REPO="alextaschuk/Literary-Quote-Clock"
BRANCH="main"
FILE="quotes.csv"

DEST="/home/user/path/to/clock/share/quotes.csv" # Modify to point to the correct file
SHA_FILE="/home/user/path/to/clock/share/quotes.csv.sha" # Modify to point to the correct file

git pull

REMOTE_SHA=$(curl -s \
    "https://api.github.com/repos/$REPO/commits?path=$FILE&sha=$BRANCH&per_page=1" \
    | grep -m1 '"sha":' \
    | cut -d'"' -f4)

LOCAL_SHA=$(cat "$SHA_FILE" 2>/dev/null)

if [ "$REMOTE_SHA" != "$LOCAL_SHA" ]; then
    # CSV file changed since last check, so download the new version
    if curl -fL -o "$DEST" \
        "https://raw.githubusercontent.com/$REPO/$BRANCH/$FILE"; then

        sed -i \
            -e 's/◻/_/g' \
            -e 's/◯/*/g' \
            -e 's/␤/\\n/g' \
            -e 's/⇇/\\n\\n/g' \
            "$DEST"

        echo "$REMOTE_SHA" > "$SHA_FILE" # update with the most recent hash
    else
        exit 1 # CSV failed to download
    fi
fi

sudo shutdown -r now
