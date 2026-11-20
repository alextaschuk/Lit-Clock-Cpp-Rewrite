#!/bin/bash
##################################################
# Download and process quotes
##################################################


##################################################
# Download the Quotes CSV from the Python clock's
# remote repo.
#
# Notice that this is a subshell function.
# Should be called in install_clock.sh.
##################################################
download_quotes_csv() (
    local temp_csv

    temp_csv="$(mktemp "$SHARE_DIR/quotes.csv.tmp.XXXXXX")"
    trap 'rm -f "$temp_csv"' EXIT

    curl -fLsS "$QUOTE_URL" -o "$temp_csv"

    process_quotes "$temp_csv"

    mv "$temp_csv" "$SHARE_DIR/quotes.csv"

    echo "Downloaded quotes.csv from the Python Clock's remote repo..."
    echo ""
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
    log 1 "Checking the remote CSV version of Python Clock..."

    local response=""
    REMOTE_SHA=""


    # Store GitHub's whole reply before searching it. If curl is piped straight into
    # `grep -m1`, grep stops reading after its first match and curl fails whenever the
    # reply is large (GitHub includes the file's contents in it for files under 1 MB).
    if response="$(curl -fsS "https://api.github.com/repos/$PY_REPO/contents/$FILE?ref=$BRANCH")"; then
        REMOTE_SHA="$(grep -m1 '"sha":' <<< "$response" | cut -d'"' -f4)" || REMOTE_SHA=""
    fi

    if [[ -z "$REMOTE_SHA" ]]; then
        error_ts "Could not determine the remote CSV's SHA."
        return 1
    fi
}


##################################################
# Download the quotes CSV and record its hash.
# Prevents the update script's first run from
# redownloading the same CSV.
#
# Should be called in install_clock.sh.
##################################################
install_quotes_csv() {
    local sha_to_record=""

    # Read the hash before downloading: if the CSV changes in between, the update script
    # sees a different hash and downloads the newer version.
    if get_remote_csv_hash; then
        sha_to_record="$REMOTE_SHA"
    fi

    download_quotes_csv

    if [[ -n "$sha_to_record" ]]; then
        echo "$sha_to_record" > "$SHA_FILE"
    else
        rm -f "$SHA_FILE" # if hash fails to save, update script will download CSV on first run anyway
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
        log 1 "CSV changed since last check. Downloading new version..."
        CSV_UPDATED=true

        download_quotes_csv

        echo "$REMOTE_SHA" > "$SHA_FILE"
        log 1 "Successfully updated the CSV."
    else
        log 1 "No changes to the CSV."
    fi
}