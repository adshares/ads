#!/bin/bash

# Formatting date and time
date_msg()
{
    echo "$(date +'%F %T') | $(date --date=@$1 +'%F %T')  $2"
}

# Checks if the node is alive
status() {
    opt_file="$1/options.cfg"

    line=`grep -i addr= ${opt_file} | head -1`
    if [[ ${line} =~ ^addr=(.+)$ ]]; then
        host=${BASH_REMATCH[1]}
    else
        >&2 echo "Cannot find node address"
    fi

    line=`grep -i offi= ${opt_file} | head -1`
    if [[ ${line} =~ ^offi=(.+)$ ]]; then
        port=${BASH_REMATCH[1]}
    else
        >&2 echo "Cannot find node port"
    fi

    line=`grep -i svid= ${opt_file} | head -1`
    if [[ ${line} =~ ^svid=(.+)$ ]]; then
        node_id=${BASH_REMATCH[1]}
    else
        >&2 echo "Cannot find node id"
    fi

    data=`echo '{"run":"get_block"}' | ads -H${host} -P${port} --work-dir=$1 2> /dev/null`
    if [[ ${data} =~ \"current_block_time\":\ \"([0-9]+)\".*\"previous_block_time\":\ \"([0-9]+)\" ]]; then
        if [ -n "$2" ]; then
            date_msg ${BASH_REMATCH[1]} "<-  $(date --date=@${BASH_REMATCH[2]} +'%F %T')  ${node_id}"
        else
            echo ${node_id}
        fi
    elif [[ ${data} =~ \"error\":\ \"(.+)\" ]]; then
        >&2 echo "Response error: ${BASH_REMATCH[1]}"
        exit 1
    else
        >&2 echo "Cannot parse 'get_block' response"
        exit 1
    fi
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
    echo "  $0 [options] <command>"
    echo
    echo "Commands"
    echo "  status                   checks if the node is alive"
    echo "  tps                      transactions per second"
    echo "  txs                      the number of awaiting transactions"
    echo "  peers                    the number of active peers"
    echo "  conns                    the number of currently open connections"
    echo
    echo "Options"
    echo "  -w, --working-dir <DIR>  working directory"
    echo "  -e, --stderr-path <FILE> path to STDERR"
    echo "  -v, --verbose            verbose mode"
    echo "  -c, --continuous         continuous monitoring"
    echo "  -h, --help               display this help and exit"
}

working_dir=~/.ads

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
        -w|--working-dir)
            if [ -z "$2" ]; then
                show_error "Working directory is required"
            fi
            working_dir=$2
            shift 2
        ;;
        -e|--stderr-path)
            if [ -z "$2" ]; then
                show_error "Path to stderr is required"
            fi
            stderr_path=$2
            shift 2
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
    show_error "Command is required"
fi
if [ -n "$2" ]; then
    show_error "To many arguments"
fi

# Checking paths and dirs
if [ ! -d ${working_dir} ]; then
    show_error "Cannot find working directory \"$working_dir\""
fi
working_dir=`realpath ${working_dir}`

if [ -z "$stderr_path" ]; then
    stderr_path="$working_dir/stderr"
fi

if [ ! -f ${stderr_path} ]; then
    show_error "Cannot find STDERR file \"$stderr_path\""
fi
stderr_path=`realpath ${stderr_path}`

# Parsing command
command=
case "$1" in
    status) command="status ${working_dir} ${verbose}" ;;
    tps)    command="tps ${stderr_path} ${verbose}" ;;
    txs)    command="txs ${stderr_path} ${verbose}" ;;
    peers)  command="peers ${stderr_path} ${verbose}" ;;
    conns)  command="connections ${stderr_path} ${verbose}" ;;
    *)      show_error "Unsupported command \"$1\"" ;;
esac

# Runs the monitor in a loop
if [ -n "$continuous" ]; then
    while true; do
        ${command}
        sleep 1
    done
else
    ${command}
fi
