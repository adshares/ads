#!/bin/bash

# Formatting date and time
date_msg()
{
    echo "$(date --date=@$1 +'%F %T')  $2"
}

# Calculating transactions per second
tps() {
    actual_txs=
    tac $1 | grep CLOCK | head -2 | while read line
    do
        if [[ ${line} =~ \[([0-9]+)\]\ CLOCK:\ ([0-9A-F]+).*txs:([0-9]+) ]]; then

            if [ -z "$actual_txs" ]; then
                actual_time=${BASH_REMATCH[1]}
                actual_clock="0x${BASH_REMATCH[2]}"
                actual_txs=${BASH_REMATCH[3]}
            else
                last_time=${BASH_REMATCH[1]}
                last_clock="0x${BASH_REMATCH[2]}"
                last_txs=${BASH_REMATCH[3]}

                if [ $(( $actual_clock )) -ge "0" ] && [ $(( $last_clock )) -lt "0" ]; then
                    txs=${actual_txs}
                else
                    txs="$(($actual_txs - $last_txs))"
                fi

                if [ -n "$2" ]; then
                    date_msg ${actual_time} "0x${actual_clock: -2}  $txs"
                else
                    echo ${txs}
                fi

                break
            fi
        fi
    done
}

# Parsing CLOCK log
clock() {
    line=`tac $2 | grep CLOCK | head -1`
    regex="^\[([0-9]+)\]\ CLOCK:\ ([0-9A-F]+).*\ $1:([0-9]+)"

    if [[ ${line} =~ ${regex} ]]; then

        time=${BASH_REMATCH[1]}
        clock=${BASH_REMATCH[2]}
        val=${BASH_REMATCH[3]}

        if [ -n "$3" ]; then
            date_msg ${time} "0x${clock: -2}  ${val}"
        else
            echo ${val}
        fi

    else
        >&2 echo "Cannot find CLOCK log"
        exit 1
    fi
}

# Parsing log
log() {
    line=`tac $3 | grep "$1" | head -1`
    regex="^\[([0-9]+)\]\ .*\ $2:([0-9]+)"

    if [[ ${line} =~ ${regex} ]]; then

        time=${BASH_REMATCH[1]}
        val=${BASH_REMATCH[2]}

        if [ -n "$4" ]; then
            date_msg ${time} ${val}
        else
            echo ${val}
        fi

    else
        >&2 echo "Cannot find $1 log"
        exit 1
    fi
}

# Number of awaiting transactions
txs() {
    clock "txs" $1 $2
}

# Number of active peers
peers() {
    clock "peers" $1 $2
}

# Number of currently open connections
connections() {
    log "OFFICE new ticket" "open" $1 $2
}

# Display error message
show_error() {
    >&2 echo "Error: $1"
    >&2 echo
    >&2 show_usage
    exit 1
}

# Display help message
show_help() {
    echo "Monitor ADS network."
    echo
    show_usage
}

# Display usage message
show_usage() {
    echo "Usage"
    echo "  $0 [options] <path-to-stderr> <command>"
    echo
    echo "Commands"
    echo "  tps                 Transactions per second"
    echo "  txs                 The number of awaiting transactions"
    echo "  peers               The number of active peers"
    echo "  conns               The number of currently open connections"
    echo
    echo "Options"
    echo "  -v, --verbose       Verbose mode"
    echo "  -c, --continuous    Continuous monitoring"
    echo "  -h, --help          Display this help and exit"
}

# Parsing options
PARAMS=""
while (( "$#" )); do
    case "$1" in
        -v|--verbose)
            verbose=1
            shift 1
        ;;
        -c|--continuous)
            continuous=1
            shift 1
        ;;
        -[vc][vc])
            if [[ $1 = *"v"* ]]; then verbose=1; fi
            if [[ $1 = *"c"* ]]; then continuous=1; fi
            shift 1
        ;;
        -h|-\?|--help)
            show_help
            exit 0
        ;;
        --) # End argument parsing
            shift
            break
        ;;
        -*|--*=) # Unsupported flags
            show_error "Unsupported flag $1"
        ;;
        *) # Preserve positional arguments
            PARAMS="$PARAMS $1"
            shift
        ;;
    esac
done
# Set positional arguments in their proper place
eval set -- "${PARAMS}"

if [ -z "$1" ]; then
    show_error "Path to stderr is required"
fi
if [ -z "$2" ]; then
    show_error "Command is required"
fi
if [ -n "$3" ]; then
    show_error "To many arguments"
fi

# Parsing command
monitor() {
    case "$1" in
        tps)    tps $2 $3 ;;
        txs)    txs $2 $3 ;;
        peers)  peers $2 $3 ;;
        conns)  connections $2 $3 ;;
        *)      show_error "Unsupported command \"$1\"" ;;
    esac
}

# Runs the monitor in a loop
if [ -n "$continuous" ]; then
    while true; do
        monitor $2 $1 ${verbose}
        sleep 1
    done
else
    monitor $2 $1 ${verbose}
fi
