#!/bin/bash
set -euo pipefail

##################################################
# Run this script from the clock's root directory.
##################################################

if [[ "$EUID" -eq 0 ]]; then
    echo "Error: Run this setup script as your regular user, without sudo."
    exit 1
fi

ROOT="$(pwd)" # e.g. home/user/Lit-Clock-Cpp-Rewrite

BUILD_SERVICE="CPP_clock_build.service"
CLOCK_SERVICE="CPP_clock.service"

if [[ ! -f "$ROOT/CMakeLists.txt" ]]; then
    echo "Error: This script must be ran from the clock's root directory (Lit-Clock-Cpp-Rewrite/)."
    exit 1
fi

echo "┌──────────────────────────────┐"
echo "│Literary Quote Clock Uninstall│"
echo "└——————————————────—————————──—┘"
printf "This script uninstalls the Literary Quote Clock and all associated files, processes, etc.\r\n"


##################################################
# Stop the clock and uninstall the startup service
# scripts and the auto-update script.
##################################################
printf "\r\nUninstalling the clock's startup scripts and the auto-update script..."

# remove service scripts
sudo systemctl stop "$CLOCK_SERVICE"

sudo systemctl disable "$CLOCK_SERVICE"
sudo systemctl disable "$BUILD_SERVICE"

sudo rm /etc/systemd/system/"$CLOCK_SERVICE"
sudo rm /etc/systemd/system/"$BUILD_SERVICE"

sudo systemctl daemon-reload
sudo systemctl reset-failed

# remove auto-update from cron table
UPDATE_SCRIPT="$ROOT/scripts/update_clock.sh"
CURRENT_CRONTAB="$(crontab -l 2>/dev/null || true)"

if ! printf '%s\n' "$CURRENT_CRONTAB" | grep -Fq "$UPDATE_SCRIPT"; then
    echo "Auto-update cron job not found. Moving on..."
fi

# keep all other cron jobs
NEW_CRONTAB="$(printf '%s\n' "$CURRENT_CRONTAB" | grep -Fv "$UPDATE_SCRIPT" || true)"
 
if printf '%s' "$NEW_CRONTAB" | grep -q '[^[:space:]]'; then
    printf '%s\n' "$NEW_CRONTAB" | crontab -
else
    crontab -r # auto-update was the only thing in crontab, so wipe the crontable
fi

printf "\r\nStartup and auto-update scripts successfully uninstalled."

##################################################
# Remove the bcm2835 library (ignores system-wide)
# bcm2835 installations).
##################################################
sudo rm /usr/local/include/bcm2835.h
sudo rm /usr/local/lib/libbcm2835.a

##################################################
# Delete Lit-Clock-Cpp-Rewrite/ directory
##################################################
cd /
if [[ -n "$ROOT" && "$ROOT" != "/" && "$ROOT" != "$HOME" ]]; then
    rm -rf -- "$ROOT"
    echo "Deleted Lit-Clock-Cpp-Rewrite/ directory"
fi

echo "Clock uninstall complete. To reinstall, visit https://github.com/alextaschuk/Lit-Clock-Cpp-Rewrite"