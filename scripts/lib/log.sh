#!/bin/bash
##################################################
# Functions for basic logging.
#
# This is source'd into common.sh for all other
# scripts to use.
##################################################


##################################################
# Logs a message to stdout.
#
# Arguments:
#   1/0 to prefix the message with a timestamp. Timestamp has
#   format of "YYYY-MM-DD HH:MM:SS".
#   The message to log.
##################################################
log() {
    local prefix_with_ts="$1" # 1 if the message should have a timestamp prefixed, 0 otherwise.
    shift || true # the rest of the arguments are the message

    case "$prefix_with_ts" in
        1)
            printf '%s: %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*"
            ;;
        0)
            printf '%s\n' "$*"
            ;;
        *)
            printf '%s\n' "log.sh: The first argument to a log function must be 1 or 0 (1 to prefix timestamp, 0 otherwise), but it was: $prefix_with_ts" >&2
            return 1
            ;;
    esac
}


##################################################
# Log an INFO message to stdout.
#
# Arguments:
#   1/0 to prefix the log with a timestamp. Timestamp has
#   format of "YYYY-MM-DD HH:MM:SS".
#   The message to log.
##################################################
info() {
    local prefix_with_ts="${1-}"
    shift || true
    log "$prefix_with_ts" "INFO: $*"
}


##################################################
# Log an ERROR message to stderr.
#
# Arguments:
#   1/0 to prefix the log with a timestamp. Timestamp has
#   format of "YYYY-MM-DD HH:MM:SS".
#   The message to log.
##################################################
error() {
    local prefix_with_ts="${1-}"
    shift || true
    log "$prefix_with_ts" "ERROR: $*" >&2
}

##################################################
# Log a debug message to stdout.
#
# Arguments:
#   1/0 to prefix the log with a timestamp. Timestamp has
#   format of "YYYY-MM-DD HH:MM:SS".
#   The message to log.
##################################################
debug() {
    local prefix_with_ts="${1-}"
    shift || true
    log "$prefix_with_ts" "DEBUG: $*"
}

##################################################
# Log a warning message to stderr.
#
# Arguments:
#   1/0 to prefix the log with a timestamp. Timestamp has
#   format of "YYYY-MM-DD HH:MM:SS".
#   The message to log.
##################################################
warn() {
    local prefix_with_ts="${1-}"
    shift || true
    log "$prefix_with_ts" "WARN: $*" >&2
}

##################################################
# Log a fatal message to stderr.
#
# Arguments:
#   1/0 to prefix the log with a timestamp. Timestamp has
#   a format of "YYYY-MM-DD HH:MM:SS".
#   The message to log.
##################################################
fatal() {
    local prefix_with_ts="${1-}"
    shift || true
    log "$prefix_with_ts" "FATAL: $*" >&2
}
