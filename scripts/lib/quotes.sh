#!/bin/bash
##################################################
# Download and process quotes
##################################################


##################################################
# Download the Quotes CSV from the Python clock's
# remote repo. Notice that this is a subshell 
# function.
# 
# Should be called in install_clock.sh.
##################################################
download_quotes_csv() (
    echo ""
    echo "Downloading quotes.csv from the Python Clock's remote repo..."

    local temp_csv

    temp_csv="$(mktemp "$SHARE_DIR/quotes.csv.tmp.XXXXXX")"
    trap 'rm -f "$temp_csv"' EXIT

    curl -fLsS "$QUOTE_URL" -o "$temp_csv"

    process_quotes "$temp_csv"

    mv "$temp_csv" "$SHARE_DIR/quotes.csv"
)

##################################################
# Convert the Python clock's text formatting
# delimiters to the C++ Clock's delimiters.
##################################################
process_quotes() {
    local csv_file="$1"

    # Escape existing delimiters, then convert the Python Clock delimiters.
    sed -i \
    -e 's/\*/\\*/g' \
    -e 's/_/\\_/g' \
    -e 's/◻/_/g' \
    -e 's/◯/*/g' \
    -e 's/␤/\\n/g' \
    -e 's/⇇/\\n\\n/g' \
    "$csv_file"
}

##################################################
# Get the hash of the quotes csv from the remote
# repo and store it in REMOTE_SHA.
##################################################
get_remote_csv_hash() {
    echo "$(date): Checking the remote CSV version of Python Clock..."
    REMOTE_SHA=$(curl -fsS \
        "https://api.github.com/repos/$PY_REPO/contents/$FILE?ref=$BRANCH" \
        | grep -m1 '"sha":' \
        | cut -d'"' -f4)

    if [[ -z "$REMOTE_SHA" ]]; then
        echo "$(date): Error: Could not determine the remote CSV's SHA."
        exit 1
    fi
}

##################################################
# Attempt to update the quotes CSV file and its
# corresponding hash file with the most recent
# version.
##################################################
update_csv() {
    CSV_UPDATED=false

    get_remote_csv_hash

    LOCAL_SHA=$(cat "$SHA_FILE" 2>/dev/null || true)

    if [[ "$REMOTE_SHA" != "$LOCAL_SHA" ]]; then
        echo "$(date): CSV changed since last check. Downloading new version..."
        CSV_UPDATED=true

        download_quotes_csv

        echo "$REMOTE_SHA" > "$SHA_FILE"
        echo "$(date): Successfully updated the CSV."
    else
        echo "$(date): No changes to the CSV."
    fi
}