#!/bin/bash

tps() {
    tac $1 | grep CLOCK | while read line
    do
        if [[ $line =~ \[([0-9]+)\]\ CLOCK:\ ([0-9A-F]+).*txs:([0-9]+) ]]; then

            if [ -z "$actual_txs" ]; then
                actual_time=${BASH_REMATCH[1]}
                actual_block_time="0x${BASH_REMATCH[2]}"
                actual_txs=${BASH_REMATCH[3]}
            else
                last_time=${BASH_REMATCH[1]}
                last_block_time="0x${BASH_REMATCH[2]}"
                last_txs=${BASH_REMATCH[3]}

                if [ $(( $actual_block_time )) -ge "0" ] && [ $(( $last_block_time )) -lt "0" ]; then
                    txs=$actual_txs
                else
                    txs="$(($actual_txs - $last_txs))"
                fi

                if [ -n "$2" ]; then
#                    actual_block_time="$(printf '0x%016X' 0x${BASH_REMATCH[2]})"
                    echo "$(date --date=@$actual_time +'%F %T') 0x${actual_block_time: -2}  $txs"
                else
                    echo $txs
                fi

                break
            fi
        fi
    done
}

connections() {
    tac $1 | grep OFFICE | while read line
    do
        if [[ $line =~ \[([0-9]+)\]\ OFFICE\ new\ ticket.*open:([0-9]+) ]]; then

            time=${BASH_REMATCH[1]}
            conn=${BASH_REMATCH[2]}

            if [ -n "$2" ]; then
                echo "$(date --date=@$time +'%F %T')  $conn"
            else
                echo $conn
            fi

            break
        fi
    done
}

monitor() {
    case "$1" in
        tps)  tps $2 $3
            ;;
        conn)  connections $2 $3
            ;;
        *)
            usage
            ;;
    esac
}

usage() {
    echo "Monitor ADS network."
    echo
    echo "Usage"
    echo "  $0 [options] <path-to-stderr> <command>"
    echo
    echo "Commands"
    echo "  tps            Transactions per second"
    echo "  conn           The number of currently open connections"
    echo
    echo "Options"
    echo "  -v             Verbose mode"
    echo "  -c             Continuous monitoring"
    exit 1
}

while getopts ":v?c?" opt; do
    case "$opt" in
        v)  verbose=1
            ;;
        c)  continuous=1
            ;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

if [ -z "$1" ] || [ -z "$2" ] || [ -n "$3" ]
then
    usage
fi

if [ -n "$continuous" ]; then
    while true; do
        monitor $2 $1 $verbose
        sleep 1
    done
else
    monitor $2 $1 $verbose
fi
