#!/bin/bash
set -euo pipefail

##################################################
# Run this script from the clock's root directory.
##################################################

if [[ "$EUID" -eq 0 ]]; then
    echo "Error: Run this setup script as your regular user, without sudo."
    exit 1
fi

# Directories
ROOT="$(pwd)" # e.g. home/user/Lit-Clock-Cpp-Rewrite
BUILD_DIR="$ROOT/build"
SHARE_DIR="$ROOT/share"
SCRIPTS_DIR="$ROOT/scripts"

# Files
CONSTANTS_FILE="$ROOT/include/constants.hpp"
BINARY="$BUILD_DIR/clock"
BUILD_SERVICE="$SCRIPTS_DIR/CPP_clock_build.service"
CLOCK_SERVICE="$SCRIPTS_DIR/CPP_clock.service"

QUOTE_URL="https://raw.githubusercontent.com/alextaschuk/Literary-Quote-Clock/main/quotes.csv"

if [[ ! -f "$ROOT/CMakeLists.txt" ]]; then
    echo "Error: This script must be ran from the clock's root directory (Lit-Clock-Cpp-Rewrite/)."
    exit 1
fi
if [[ ! -f "$CONSTANTS_FILE" ]]; then
    echo "Error: constants.hpp not found at $CONSTANTS_FILE"
    exit 1
fi

##################################################
# Get VCOM and INCLUDE_CREDITS values from user
# input.
##################################################
while true; do
    read -r -p "Enter the VCOM value printed on the screen's FPC (e.g. -2.79): " VCOM
    if [[ "$VCOM" =~ ^-[0-9]+\.[0-9]{2}$ ]]; then
        break
    fi
    echo "Invalid VCOM value. Try again."
done
echo "Updated VCOM value in constants.hpp to $VCOM."

read -r -p "Include the credits (book title & author) under each quote? [Y/n]: " CREDITS_INPUT
case "$CREDITS_INPUT" in
    [nN]|[nN][oO])
        INCLUDE_CREDITS=false
        ;;
    *)
        INCLUDE_CREDITS=true
        ;;
esac

# Update the VCOM and INCLUDE_CREDITS variables in constants.hpp with user's input
sed -i -E \
    "s|^([[:space:]]*inline constexpr double VCOM[[:space:]]*=[[:space:]]*).*;|\1${VCOM};|" \
    "$CONSTANTS_FILE"

sed -i -E \
    "s|^([[:space:]]*inline constexpr bool INCLUDE_CREDITS[[:space:]]*=[[:space:]]*).*;|\1${INCLUDE_CREDITS};|" \
    "$CONSTANTS_FILE"

echo "Updated constants.hpp:"
grep -E '\tinline constexpr (double VCOM|bool INCLUDE_CREDITS)' "$CONSTANTS_FILE"

##################################################
# Generate the Clock's Build Files
##################################################
echo "Generating build files..."
cmake -S "$ROOT" -B "$BUILD_DIR"

##################################################
# Download the Quotes CSV and convert the text
# formatting delimiters
##################################################
echo "Downloading quotes.csv from the Python Clock's remote repo..."
TEMP_CSV="$(mktemp "$SHARE_DIR/quotes.csv.tmp.XXXXXX")"
trap 'rm -f "$TEMP_CSV"' EXIT

curl -fLsS "$QUOTE_URL" -o "$TEMP_CSV"

# Escape existing delimiters, then convert the Python Clock delimiters.
sed -i \
    -e 's/\*/\\*/g' \
    -e 's/_/\\_/g' \
    -e 's/◻/_/g' \
    -e 's/◯/*/g' \
    -e 's/␤/\\n/g' \
    -e 's/⇇/\\n\\n/g' \
    "$TEMP_CSV"

mv "$TEMP_CSV" "$SHARE_DIR/quotes.csv"
trap - EXIT

##################################################
# Update the service files to use the necessary
# paths and install them in the systemd manager
##################################################
echo "Configuring the startup services..."
if [[ ! -f "$BUILD_SERVICE" || ! -f "$CLOCK_SERVICE" ]]; then
    echo "Error: Service files not found in $SCRIPTS_DIR."
    exit 1
fi

sed -i \
    "s|__WORKING_DIRECTORY__|$BUILD_DIR|g" \
    "$BUILD_SERVICE"

sed -i \
    "s|__EXEC_START__|$BINARY|g" \
    "$CLOCK_SERVICE"

echo "Installing systemd services..."
sudo install -m 644 "$BUILD_SERVICE" /etc/systemd/system/CPP_clock_build.service
sudo install -m 644 "$CLOCK_SERVICE" /etc/systemd/system/CPP_clock.service

echo "Reloading the systemd manager..."
sudo systemctl daemon-reload

##################################################
# Start the clock
##################################################
echo "Enabling clock services and starting the clock (this may take a minute)..."
sudo systemctl enable --now CPP_clock_build.service
sudo systemctl enable --now CPP_clock.service

##################################################
# Enable automatic updates
##################################################
echo "Configuring the clock's auto-update script..."

# Add the update script to the user's cron table
UPDATE_SCRIPT="$SCRIPTS_DIR/update_clock.sh"

if [[ ! -f "$UPDATE_SCRIPT" ]]; then
    echo "Error: Update script not found at $UPDATE_SCRIPT"
    exit 1
fi

LOG_FILE="$ROOT/update_clock.log"
CRON_JOB="0 4 * * * bash $UPDATE_SCRIPT >> $LOG_FILE 2>&1"
CURRENT_CRONTAB="$(crontab -l 2>/dev/null || true)"

sed -i "s|^REPO_DIR=\"__REPO_DIR__\"$|REPO_DIR=\"$ROOT\"|" "$UPDATE_SCRIPT"

if printf '%s\n' "$CURRENT_CRONTAB" | grep -Fq "$UPDATE_SCRIPT"; then
    echo "Auto-update cron job already exists."
else
    printf '%s\n%s\n' "$CURRENT_CRONTAB" "$CRON_JOB" | crontab -
    echo "The clock's auto-update script has been configured to run daily at 4:00 A.M. Logs are stored at $LOG_FILE."
fi

echo "Clock setup complete."