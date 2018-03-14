#!/bin/bash

#source basefunctions.sh

function createAccount
{
    echo '.............................create Account User' $1 $2

    cd $1

    echo '{"run":"get_me"}' | ./esc


    echo '{"run":"create_account","node":"'$2'"}'


    (echo '{"run":"get_me"}'; echo '{"run":"create_account","node":"'$2'"}') | ./esc

    cd ..
}


createAccount "user1" "0001"

exit 0

getMe ${userpath[1]} "0001-00000000-9B6F" "0001-00000000-9B6F"
getMe ${userpath[2]} "0001-00000001-8B4E" "0001-00000001-8B4E"
getMe ${userpath[3]} "0002-00000000-XXXX" "0002-00000000-"

checkBalance ${userpath[1]} "0001-00000001-8B4E" "280."
checkBalance ${userpath[1]} "0002-00000000-XXXX" "0."
checkBalance ${userpath[2]} "0002-00000000-XXXX" "0."
checkBalance ${userpath[3]} "0002-00000000-XXXX" "0."

sendCash ${userpath[2]} "0002-00000000-XXXX" 99.9
sleep 30
checkBalance ${userpath[1]} "0001-00000001-8B4E" "180."
checkBalance ${userpath[2]} "0001-00000001-8B4E" "180."
checkBalance ${userpath[3]} "0001-00000001-8B4E" "180."

checkBalance ${userpath[1]} "0002-00000000-XXXX" "99."
checkBalance ${userpath[2]} "0002-00000000-XXXX" "99."
checkBalance ${userpath[3]} "0002-00000000-XXXX" "99."

sendCash ${userpath[3]} "0001-00000001-8B4E" 99.8
sleep 30
checkBalance ${userpath[3]} "0001-00000001-8B4E" "280."
checkBalance ${userpath[2]} "0001-00000001-8B4E" "280."
checkBalance ${userpath[1]} "0001-00000001-8B4E" "280."








