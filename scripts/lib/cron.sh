#!/bin/bash
##################################################
# Configure and install a cron job for 
# auto-updates.
##################################################


##################################################
# Configure and create a cron job that is ran once
# every day at 04:00.
#
# Should be called in install_clock.sh.
##################################################
create_clock_cron_job() {
    echo "Enabling auto-updates for the clock..."

    # Add the update script to the user's cron table
    LOG_FILE="$ROOT/update_clock.log"
    CRON_JOB="0 4 * * * bash \"$UPDATE_SCRIPT\" >> \"$LOG_FILE\" 2>&1"
    CURRENT_CRONTAB="$(crontab -l 2>/dev/null || true)"

    if printf '%s\n' "$CURRENT_CRONTAB" | grep -Fq "$UPDATE_SCRIPT"; then
        echo "Auto-update cron job already exists."
    else
        printf '%s\n%s\n' "$CURRENT_CRONTAB" "$CRON_JOB" | crontab -
        echo "The clock's auto-update script has been configured to run daily at 4:00 A.M. Logs are stored at $LOG_FILE."
    fi
}
