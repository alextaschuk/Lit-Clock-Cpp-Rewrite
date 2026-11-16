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
    echo ""
    echo "Enabling auto-updates for the clock..."

    # Add the update script to the user's cron table
    LOG_FILE="$SCRIPTS_DIR/update_clock.log"
    CRON_JOB="0 4 * * * bash \"$UPDATE_SCRIPT\" >> \"$LOG_FILE\" 2>&1"
    CURRENT_CRONTAB="$(crontab -l 2>/dev/null || true)"

    if printf '%s\n' "$CURRENT_CRONTAB" | grep -Fq "$UPDATE_SCRIPT"; then
        echo "Auto-update cron job already exists."
    else
        printf '%s\n%s\n' "$CURRENT_CRONTAB" "$CRON_JOB" | crontab -
        echo "The clock's auto-update script has been configured to run daily at 4:00 A.M. Logs are stored at $LOG_FILE."
    fi
}


##################################################
# Remove the auto-update cron job from the user's
# cron table.
#
# Should be called in uninstall_clock.sh.
##################################################
remove_clock_cron_job() {
    echo ""
    echo "Removing the clock's auto-update cron job..."

    local current_crontab
    local new_crontab
    current_crontab="$(crontab -l 2>/dev/null || true)"

    if ! printf '%s\n' "$current_crontab" | grep -Fq "$UPDATE_SCRIPT"; then
        warn "Auto-update cron job not found. Moving on..."
        return 0
    fi

    new_crontab="$(printf '%s\n' "$current_crontab" | grep -Fv "$UPDATE_SCRIPT" || true)"

    if printf '%s' "$new_crontab" | grep -q '[^[:space:]]'; then
        printf '%s\n' "$new_crontab" | crontab -
    else
        crontab -r # auto-update was the only job, so remove the (now empty) cron table
    fi

    echo "Auto-update cron job removed."
}
