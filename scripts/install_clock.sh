#!/bin/bash
##################################################
# Script to install the clock on a user's system.
##################################################


set -euo pipefail

I_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)" # path to this file
ROOT="$(dirname "$I_SCRIPT_DIR")"

source "$I_SCRIPT_DIR/lib/common.sh"

source "$I_SCRIPT_DIR/lib/configure.sh"
source "$I_SCRIPT_DIR/lib/cron.sh"
source "$I_SCRIPT_DIR/lib/dependencies.sh"
source "$I_SCRIPT_DIR/lib/quotes.sh"
source "$I_SCRIPT_DIR/lib/services.sh"
source "$I_SCRIPT_DIR/lib/user_config.sh"

check_environment # common.sh

echo "┌──────────────────────────┐"
echo "│Literary Quote Clock Setup│"
echo "└──────────────────────────┘"
echo "This script walks you through all of the necessary services install the clock, enable boot-time startup for the clock, and have it automatically update (e.g. receive new quotes)."

get_user_config             # user_config.sh
install_dependencies        # dependencies.sh
configure_clock             # configure.sh
download_quotes_csv         # quotes.sh
create_clock_cron_job       # cron.sh
install_systemd_services    # services.sh

echo "Clock setup complete."