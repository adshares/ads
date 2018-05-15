#!/bin/bash

PATH=$PATH:/$PWD

nodebasename='escd'
deploypath="deployment"
current=$PWD

testresult=0


officeport=7190
serverport=8190
peerport=8190
stopservice="FALSE"


hostaddr1=85.10.197.15
hostaddr2=5.9.56.132


if [ $# -eq 4 ]
then
officeport=$1
serverport=$2
peerport=$3
stopservice=$4
fi

echo $officeport
echo $serverport
echo $peerport

nodename='escdnodesrv'

let "user1office = $officeport + 1"
let "user2office = $officeport + 1"
let "user3office = $officeport + 2"
let "user4office = $officeport + 3"
let "user5office = $officeport + 4"

userpath[1]="user1"
userpath[2]="user2"
userpath[3]="user3"
userpath[4]="user4"
userpath[5]="user5"

echo ${userpath[1]}
echo ${userpath[2]}
echo ${userpath[3]}
echo ${userpath[4]}
echo ${userpath[5]}



function prepareNode
{
    echo '.............................prepareNode'$1
    echo '.............................prepareNodedir'$5

        mkdir $5
        #ln -s $current/escd $5/escd

        cd $5
        echo 'svid='$1 > options.cfg
        echo 'offi='$2 >> options.cfg
        echo 'port='$3 >> options.cfg

        if [ $i -gt 3 ]
        then
            echo 'addr='$hostaddr2 >> options.cfg
        else
            echo 'addr='$hostaddr1 >> options.cfg
        fi

        mkdir key
        chmod go-rx key/
        if [ $i -gt 1 ] && [ $i -lt 5 ]
        then
            echo 'peer='$hostaddr1':'$4 >> options.cfg
        else
            echo 'peer='$hostaddr2':'$4 >> options.cfg
        fi

        if [ $i == 2 ]
        then
            echo '5BF11F5D0130EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C' > key/key.txt
            chmod go-r key/key.txt
        fi
        cd ..
}

function prepareClientKeys
{
    echo '.............................prepareClientKeys'$1

    cd $1

    if [ $2 == 1 ]
    then
    {
        echo 'host='$hostaddr1> settings.cfg
        echo 'port='$user1office >> settings.cfg
        echo 'address=0001-00000000-XXXX' >> settings.cfg
        echo 'secret=14B183205CA661F589AD83809952A692DFA48F5D490B10FD120DA7BF10F2F4A0' >> settings.cfg
    }
    fi

    if [ $2 == 2 ]
    then
    {
        echo 'host='$hostaddr1 > settings.cfg
        echo 'port='$user2office >> settings.cfg
        echo 'address=0001-00000001-XXXX' >> settings.cfg
        echo 'secret=5BF11F5D01CMAKE_PROJECT_CONFIG30EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C' >> settings.cfg
    }
    fi

    if [ $2 == 3 ]
    then
    {
        echo 'host='$hostaddr1 > settings.cfg
        echo 'port='$user3office >> settings.cfg
        echo 'address=0002-00000000-75BD' >> settings.cfg
        echo 'secret=FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6' >> settings.cfg
    }
    fi

    if [ $2 == 4 ]
    then
    {
        echo 'host='$hostaddr1 > settings.cfg
        echo 'port='$user3office >> settings.cfg
        echo 'address=0002-00000001-659C' >> settings.cfg
        echo 'secret=FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6' >> settings.cfg
    }
    fi

    if [ $2 == 5 ]
    then
    {
        echo 'host='$hostaddr2 > settings.cfg
        echo 'port='$user4office >> settings.cfg
        echo 'address=0004-00000000-XXXX' >> settings.cfg
        echo 'secret=D2B8F62A7E335BBD5576C8422844760F22EC378009EEEA790C41E4DC45F23C33' >> settings.cfg
    }
    fi

    cd ..
}

function initFirstNode
{
    echo '.............................initFirstNode\n'

    stopnode ${nodename[1]}

    cd ${nodename[1]}

    exec -a ${nodename[1]} escd --init 1 &> nodeout.txt &
    ##escd --init 1 > nodeout.txt &

    cd ..
    sleep 60

    echo 'INITALIZATION NODE 1 FINISHED'
}

function prepareClient
{
        echo '.............................prepareClient'$1

        username='user'$1
        mkdir $username

        prepareClientKeys $username $1
}


function setUpUser1
{
    echo '.............................setUpUser1 '${userpath[1]}

    cd ${userpath[1]}

    echo '{"run":"get_me"}' | esc

    echo '.............................change_account_key for User 1'

    (echo '{"run":"get_me"}'; echo '{"run":"change_account_key","pkey":"D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17","signature":"7A1CA8AF3246222C2E06D2ADE525A693FD81A2683B8A8788C32B7763DF6037A5DF3105B92FEF398AF1CDE0B92F18FE68DEF301E4BF7DB0ABC0AEA6BE24969006"}') | esc

    sleep 60

    echo 'host='$hostaddr1> settings.cfg
    echo 'port='$user1office >> settings.cfg
    echo 'address=0001-00000000-9B6F' >> settings.cfg
    echo 'secret=FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6' >> settings.cfg

    echo '{"run":"get_me"}' | esc
    cd ..

    echo '.............................finished'

    sleep 30
}

function createAccountForUser2
{
    echo '.............................createAccountForUser2'

    cd ${userpath[1]}

    echo '----------------------------------'
    echo '{"run":"get_me"}' | esc

    (echo '{"run":"get_me"}'; echo '{"run":"create_account","node":"0001"}') | esc

    sleep 60

    echo '{"run":"get_account","address":"0001-00000001-8B4E"}' | esc

    echo '----------------------------------'

    sleep 60

    (echo '{"run":"get_me"}'; echo '{"run":"send_one","address":"0001-00000001-8B4E","amount":0.5,"message":"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"}') | esc

    echo '{"run":"get_account","address":"0001-00000001-8B4E"}' | esc | grep balance

    cd ..
}


function createAccount
{
    echo '.............................create Account User'$1$2

    cd $1

    (echo '{"run":"get_me"}'; echo '{"run":"create_account","node":"'$2'"}') | esc

    sleep 60

    cd ..
}



function changeKeysforUser2
{
    echo '.............................changeKeysforUser2'

    cd ${userpath[1]}

    (echo '{"run":"get_me"}'; echo '{"run":"change_account_key","pkey":"C9965A1417F52B22514559B7608E4E2C1238FCA3602382C535D42D1759A2F196","signature":"ED8479C0EDA3BB02B5B355E05F66F8161811F5AD9AE9473AA91E2DA32457EAB850BC6A04D6D4D5DDFAB4B192D2516D266A38CEA4251B16ABA1DF1B91558A4A05"}' )  | esc  --address 0001-00000001-8B4E

    cd ..

    cd ${userpath[2]}

    echo '.............................changeKeysUser2 config'${userpath[2]}

    echo 'host='$hostaddr1 > settings.cfg
    echo 'port='$user2office >> settings.cfg
    echo 'address=0001-00000001-8B4E' >> settings.cfg
    echo 'secret=5BF11F5D0130EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C' >> settings.cfg
    chmod go-r settings.cfg

    sleep 60

    echo '{"run":"get_me"}' | esc

    cd ..
}


function sendCash
{
    echo '.............................sendCash'$1

    cd $1

    (echo '{"run":"get_me"}'; echo '{"run":"send_one","address":"'$2'","amount":'$3',"message":"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"}') | esc

    cd ..

    sleep 25
}

function sendCashToMany
{
    echo '.............................sendCashtomany'$1

    cd $1

    (echo '{"run":"get_me"}'; echo '{"run":"send_many","wires":{"'$2'":'$3',"'$4'":'$5'}}') | esc

    cd ..

    sleep 25

}

function checkBalance
{
    cd $1

    balance=$(echo '{"run":"get_account","address":"'$2'"}' | esc | grep balance)

    echo $2

    if [[ "$balance" == *"$3"* ]]
    then
        echo "TEST OK. USER:'$1' ACCOUNT '$2' BALANCE '$3'"
    else
        echo "ERROR: WRONG BALANCE: $balance USER '$1' ACCOUNT '$2' BALANCE '$3' diffrent than $balance"
        echo " "
        echo $balance
        finishTest 2
    fi

    cd ..
}


function getMe
{
    cd $1

    getmeinfo=$(echo '{"run":"get_me"}' | esc | grep address)

    echo $getmeinfo

    if [[ "$getmeinfo" == *"$3"* ]]
    then
        echo "TEST OK. USER:'$1'"
    else
        echo "ERROR: WRONG getMe: $getmeinfo USER '$1'"
        finishTest 1
    fi

    cd ..
}

function addNode
{
    echo '.............................ADD node'

    cd ${userpath[1]}
    (echo '{"run":"get_me"}'; echo '{"run":"create_node"}') | esc

    cd ..

    sleep 60
}

function changeNode2Key
{
    cd ${nodename[2]}

    echo 'FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6' > key/key.txt
    cd ..

    cd ${userpath[1]}

    echo '.............................change_node_key'

    (echo '{"run":"get_me"}'; echo '{"run":"change_node_key","pkey":"D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17","node":"2"}') | esc

    cd ..

    sleep 30
}

function changeNode3Key
{
    cd ${nodename[3]}

    echo 'FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6' > key/key.txt
    #echo 'A84CFE8A8631A26C5AC192EF5C781DAF48C6739B7E1A388057B2B2218D945A8B' > key/key.txt
    cd ..

    cd ${userpath[1]}

    echo '.............................change_node_key'

    (echo '{"run":"get_me"}'; echo '{"run":"change_node_key","pkey":"D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17","node":"3"}') | esc
    #(echo '{"run":"get_me"}'; echo '{"run":"change_node_key","pkey":"74B1D277044007B071FCF277A3CC5194EAA0BCA28548F6621FEBF3C00810C331","node":"3"}') | esc

    cd ..
    sleep 30
}

function changeNode4Key
{
    cd ${nodename[4]}

    echo 'D2B8F62A7E335BBD5576C8422844760F22EC378009EEEA790C41E4DC45F23C33' > key/key.txt
    cd ..

    cd ${userpath[1]}

    echo '.............................change_node_key'

    (echo '{"run":"get_me"}'; echo '{"run":"change_node_key","pkey":"BB6D774EA23DFB4D6510F04EFFA79FCA281C046CB39143B101CB451D0919AFA9","node":"4"}') | esc

    cd ..
    sleep 30
}

function changeNode5Key
{
    cd ${nodename[5]}

    echo 'D2B8F62A7E335BBD5576C8422844760F22EC378009EEEA790C41E4DC45F23C33' > key/key.txt
    cd ..

    cd ${userpath[1]}

    echo '.............................change_node_key'

    (echo '{"run":"get_me"}'; echo '{"run":"change_node_key","pkey":"BB6D774EA23DFB4D6510F04EFFA79FCA281C046CB39143B101CB451D0919AFA9","node":"4"}') | esc

    cd ..
    sleep 30
}

function getMeMultipleTest
{
    for i in $(seq 1 $1);
    do
        getMe ${userpath[1]} "0001-00000000-9B6F" "0001-00000000-9B6F"
    done
}

function getBalanceMultipleTest
{
    for i in $(seq 1 $1);
    do
        checkBalance ${userpath[1]} "0001-00000001-8B4E" "280."
    done
}

function copyserverconf
{
    cd $1

    for file in $(find ../${nodename[1]}/blk -name servers.srv | sort -rn)
    do
        cp ../${nodename[1]}/$file ./
        echo $file
        break;
    done

    cd ..
}

function stopnode
{
    echo '.............................stoptnode'$1

    pkill -f $1
    sleep 8

    ps -aux | grep $1

    echo '.............................node shoule not run '$1

    pkill -9 -f $1
    sleep 5
}

function stopAllNodes
{
    stopnode ${nodename[1]}
    stopnode ${nodename[2]}
    stopnode ${nodename[3]}
    echo "STOP"
}


function startnode
{
    stopnode $3

    cd $1

    echo '.............................startnode'$3

    ps -aux | grep $3

    exec -a $3 escd $2 &>> nodeout.txt &
    cd ..

    ps -aux | grep $3
    echo 'server started'
}


function finishTest
{
    if [ $1 == 0 ]
    then
        echo "------------------------------TEST OK -----------------------------"
        echo "´´´´´´´´´´´´´´´´´´´´´¶¶¶¶¶¶¶¶¶…….."
        echo "´´´´´´´´´´´´´´´´´´´´¶¶´´´´´´´´´´¶¶……"
        echo "´´´´´´¶¶¶¶¶´´´´´´´¶¶´´´´´´´´´´´´´´¶¶………."
        echo "´´´´´¶´´´´´¶´´´´¶¶´´´´´¶¶´´´´¶¶´´´´´¶¶………….."
        echo "´´´´´¶´´´´´¶´´´¶¶´´´´´´¶¶´´´´¶¶´´´´´´´¶¶….."
        echo "´´´´´¶´´´´¶´´¶¶´´´´´´´´¶¶´´´´¶¶´´´´´´´´¶¶….."
        echo "´´´´´´¶´´´¶´´´¶´´´´´´´´´´´´´´´´´´´´´´´´´¶¶…."
        echo "´´´´¶¶¶¶¶¶¶¶¶¶¶¶´´´´´´´´´´´´´´´´´´´´´´´´¶¶…."
        echo "´´´¶´´´´´´´´´´´´¶´¶¶´´´´´´´´´´´´´¶¶´´´´´¶¶…."
        echo "´´¶¶´´´´´´´´´´´´¶´´¶¶´´´´´´´´´´´´¶¶´´´´´¶¶…."
        echo "´¶¶´´´¶¶¶¶¶¶¶¶¶¶¶´´´´¶¶´´´´´´´´¶¶´´´´´´´¶¶…"
        echo "´¶´´´´´´´´´´´´´´´¶´´´´´¶¶¶¶¶¶¶´´´´´´´´´¶¶…."
        echo "´¶¶´´´´´´´´´´´´´´¶´´´´´´´´´´´´´´´´´´´´¶¶….."
        echo "´´¶´´´¶¶¶¶¶¶¶¶¶¶¶¶´´´´´´´´´´´´´´´´´´´¶¶…."
        echo "´´¶¶´´´´´´´´´´´¶´´¶¶´´´´´´´´´´´´´´´´¶¶…."
        echo "´´´¶¶¶¶¶¶¶¶¶¶¶¶´´´´´¶¶´´´´´´´´´´´´¶¶….."
        echo "´´´´´´´´´´´´´´´´´´´´´´´¶¶¶¶¶¶¶¶¶¶¶……."
    else
        echo "´´´´´´´´´´´´´´´´´´´ ¶¶¶¶¶¶¶¶¶¶¶¶¶¶¶¶¶¶¶´´´´´´´´´´´´´´´´´´´´"
        echo "´´´´´´´´´´´´´´´´´¶¶¶¶¶¶´´´´´´´´´´´´´¶¶¶¶¶¶¶´´´´´´´´´´´´´´´´"
        echo "´´´´´´´´´´´´´´¶¶¶¶´´´´´´´´´´´´´´´´´´´´´´´¶¶¶¶´´´´´´´´´´´´´´"
        echo "´´´´´´´´´´´´´¶¶¶´´´´´´´´´´´´´´´´´´´´´´´´´´´´´¶¶´´´´´´´´´´´´"
        echo "´´´´´´´´´´´´¶¶´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´¶¶´´´´´´´´´´´"
        echo "´´´´´´´´´´´¶¶´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´¶¶´´´´´´´´´´´"
        echo "´´´´´´´´´´¶¶´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´¶¶´´´´´´´´´´"
        echo "´´´´´´´´´´¶¶´¶¶´´´´´´´´´´´´´´´´´´´´´´´´´´´´´¶¶´¶¶´´´´´´´´´´"
        echo "´´´´´´´´´´¶¶´¶¶´´´´´´´´´´´´´´´´´´´´´´´´´´´´´¶¶´´¶´´´´´´´´´´"
        echo "´´´´´´´´´´¶¶´¶¶´´´´´´´´´´´´´´´´´´´´´´´´´´´´´¶¶´´¶´´´´´´´´´´"
        echo "´´´´´´´´´´¶¶´´¶¶´´´´´´´´´´´´´´´´´´´´´´´´´´´´¶¶´¶¶´´´´´´´´´´"
        echo "´´´´´´´´´´¶¶´´¶¶´´´´´´´´´´´´´´´´´´´´´´´´´´´¶¶´´¶¶´´´´´´´´´´"
        echo "´´´´´´´´´´´¶¶´¶¶´´´¶¶¶¶¶¶¶¶´´´´´¶¶¶¶¶¶¶¶´´´¶¶´¶¶´´´´´´´´´´´"
        echo "´´´´´´´´´´´´¶¶¶¶´¶¶¶¶¶¶¶¶¶¶´´´´´¶¶¶¶¶¶¶¶¶¶´¶¶¶¶¶´´´´´´´´´´´"
        echo "´´´´´´´´´´´´´¶¶¶´¶¶¶¶¶¶¶¶¶¶´´´´´¶¶¶¶¶¶¶¶¶¶´¶¶¶´´´´´´´´´´´´´"
        echo "´´´´¶¶¶´´´´´´´¶¶´´¶¶¶¶¶¶¶¶´´´´´´´¶¶¶¶¶¶¶¶¶´´¶¶´´´´´´¶¶¶¶´´´"
        echo "´´´¶¶¶¶¶´´´´´¶¶´´´¶¶¶¶¶¶¶´´´¶¶¶´´´¶¶¶¶¶¶¶´´´¶¶´´´´´¶¶¶¶¶¶´´"
        echo "´´¶¶´´´¶¶´´´´¶¶´´´´´¶¶¶´´´´¶¶¶¶¶´´´´¶¶¶´´´´´¶¶´´´´¶¶´´´¶¶´´"
        echo "´¶¶¶´´´´¶¶¶¶´´¶¶´´´´´´´´´´¶¶¶¶¶¶¶´´´´´´´´´´¶¶´´¶¶¶¶´´´´¶¶¶´"
        echo "¶¶´´´´´´´´´¶¶¶¶¶¶¶¶´´´´´´´¶¶¶¶¶¶¶´´´´´´´¶¶¶¶¶¶¶¶¶´´´´´´´´¶¶"
        echo "¶¶¶¶¶¶¶¶¶´´´´´¶¶¶¶¶¶¶¶´´´´¶¶¶¶¶¶¶´´´´¶¶¶¶¶¶¶¶´´´´´´¶¶¶¶¶¶¶¶"
        echo "´´¶¶¶¶´¶¶¶¶¶´´´´´´¶¶¶¶¶´´´´´´´´´´´´´´¶¶¶´¶¶´´´´´¶¶¶¶¶¶´¶¶¶´"
        echo "´´´´´´´´´´¶¶¶¶¶¶´´¶¶¶´´¶¶´´´´´´´´´´´¶¶´´¶¶¶´´¶¶¶¶¶¶´´´´´´´´"
        echo "´´´´´´´´´´´´´´¶¶¶¶¶¶´¶¶´¶¶¶¶¶¶¶¶¶¶¶´¶¶´¶¶¶¶¶¶´´´´´´´´´´´´´´"
        echo "´´´´´´´´´´´´´´´´´´¶¶´¶¶´¶´¶´¶´¶´¶´¶´¶´¶´¶¶´´´´´´´´´´´´´´´´´"
        echo "´´´´´´´´´´´´´´´´¶¶¶¶´´¶´¶´¶´¶´¶´¶´¶´¶´´´¶¶¶¶¶´´´´´´´´´´´´´´"
        echo "´´´´´´´´´´´´¶¶¶¶¶´¶¶´´´¶¶¶¶¶¶¶¶¶¶¶¶¶´´´¶¶´¶¶¶¶¶´´´´´´´´´´´´"
        echo "´´´´¶¶¶¶¶¶¶¶¶¶´´´´´¶¶´´´´´´´´´´´´´´´´´¶¶´´´´´´¶¶¶¶¶¶¶¶¶´´´´"
        echo "´´´¶¶´´´´´´´´´´´¶¶¶¶¶¶¶´´´´´´´´´´´´´¶¶¶¶¶¶¶¶´´´´´´´´´´¶¶´´´"
        echo "´´´´¶¶¶´´´´´¶¶¶¶¶´´´´´¶¶¶¶¶¶¶¶¶¶¶¶¶¶¶´´´´´¶¶¶¶¶´´´´´¶¶¶´´´´"
        echo "´´´´´´¶¶´´´¶¶¶´´´´´´´´´´´¶¶¶¶¶¶¶¶¶´´´´´´´´´´´¶¶¶´´´¶¶´´´´´´"
        echo "´´´´´´¶¶´´¶¶´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´¶¶´´¶¶´´´´´´"
        echo "´´´´´´´¶¶¶¶´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´´¶¶¶¶´´´´´´´"

        echo "------------------------------TEST FAILED -------------------------"
    fi


    if [ $stopservice == "TRUE" ]
    then
    {
        stopAllNodes
    }
    fi

    exit $1
}

cd ..
rm -rf $deploypath
mkdir $deploypath
cd $deploypath



for i in 1 2 3 4 5
do
        echo 'PREPARE NODE'
        let "officeport = $officeport + 1"
        let "serverport = $serverport + 1"
        let "peerport   = $serverport - 1"

        nodename[$i]=$nodebasename$i'_'$officeport

        echo ${nodename[$i]}

        prepareNode $i $officeport $serverport $peerport ${nodename[$i]}
        echo 'PREPARE NODE END'
done

for i in 1 2 3 4 5
do
    prepareClient $i
done

echo '............NODENAMMMEEEEEEE'
echo ${nodename[1]}
echo ${nodename[2]}
echo ${nodename[3]}
echo ${nodename[4]}
echo ${nodename[5]}

stopAllNodes

initFirstNode
setUpUser1
createAccountForUser2
#createAccount ${userpath[1]} "0001"
changeKeysforUser2

for i in {1..4}
do
    sendCash ${userpath[1]} "0001-00000001-8B4E" 70.0
done


addNode
checkBalance ${userpath[1]} "0001-00000001-8B4E" "280."
changeNode2Key
addNode
changeNode3Key
addNode
changeNode4Key
addNode
changeNode5Key

addNode
addNode
addNode

sleep 60

copyserverconf ${nodename[2]}
startnode ${nodename[2]} "-m 1 -f 1" ${nodename[2]}

echo 'server started'

sleep 120

checkBalance ${userpath[1]} "0001-00000001-8B4E" "280."

startnode ${nodename[2]} "-m 1" ${nodename[2]}
sleep 120

createAccount ${userpath[3]} "0002"
sleep 60

copyserverconf ${nodename[3]}
copyserverconf ${nodename[4]}
copyserverconf ${nodename[5]}
startnode ${nodename[3]} "-m 1 -f 1" ${nodename[3]}
sleep 120
startnode ${nodename[3]} "-m 1" ${nodename[3]}
sleep 120
startnode ${nodename[1]} "-m 1" ${nodename[1]}
sleep 120


echo 'server started one more time'


#stime=$(date +%s%N |cut -b1-13)echo $stime
#endtime=$(date +%s%N)
#let "period = $endtime - $stime"
#echo 'PERIOD:'$period
#echo $stime
#echo $stime
#endtime=$(date +%s%N)
#let "period = $endtime - $stime"
#echo 'PERIOD:'$period

echo "----------------TEST-----------------"


checkBalance ${userpath[1]} "0001-00000001-8B4E" "280."
checkBalance ${userpath[1]} "0002-00000000-75BD" "68."
checkBalance ${userpath[3]} "0002-00000000-75BD" "68."

checkBalance ${userpath[1]} "0002-00000001-659C" "0."
checkBalance ${userpath[3]} "0002-00000001-659C" "0."


sendCash ${userpath[2]} "0002-00000001-659C" 100.001
sleep 120

checkBalance ${userpath[1]} "0001-00000001-8B4E" "180."
checkBalance ${userpath[2]} "0001-00000001-8B4E" "180."

checkBalance ${userpath[2]} "0002-00000001-659C" "100."
checkBalance ${userpath[3]} "0002-00000001-659C" "100."

sendCash ${userpath[4]} "0001-00000001-8B4E" 99.8
sleep 120

checkBalance ${userpath[3]} "0001-00000001-8B4E" "280."
checkBalance ${userpath[2]} "0001-00000001-8B4E" "280."
checkBalance ${userpath[1]} "0002-00000001-659C" "0."


sendCashToMany ${userpath[1]} "0001-00000001-8B4E" 50  "0002-00000001-659C" 60
sleep 120

checkBalance ${userpath[2]} "0001-00000001-8B4E" "330."
checkBalance ${userpath[4]} "0002-00000001-659C" "60."


#getMeMultipleTest 10
#getBalanceMultipleTest 10


sendCash ${userpath[1]} "0004-00000000-XXXX" 111.1234
sendCash ${userpath[1]} "0005-00000000-XXXX" 221.1234

echo "-------------------------------------------------------------------"


finishTest 0


