#!/bin/bash
##################################################
# Waits for the RPi to set its internal system
# clock (requries connection to a network).
#
# A majority of RPis don't have an RTC (they all
# require external hardware for it) and rely
# on NTP to synchronize their system's clock.
##################################################

MAX_WAIT_SECONDS=60 # wait for systemd to start its time sync service
CHECK_INTERVAL_SECONDS=2

waited=0
while (( waited < MAX_WAIT_SECONDS )); do
    if [[ "$(timedatectl show --property=NTPSynchronized --value 2>/dev/null)" == "yes" ]]; then
        echo "System time is synchronised (waited $waited seconds)."
        exit 0
    fi

    sleep "$CHECK_INTERVAL_SECONDS"
    waited=$(( waited + CHECK_INTERVAL_SECONDS ))
done

echo "WARN: System time was not synchronised after $MAX_WAIT_SECONDS seconds. Starting the clock anyway."
exit 0