#!/bin/bash


function prepareNode
{
    echo '.............................prepareNode'$1

        mkdir $nodename
	ln -s $current/escd $nodename/escd

        cd $nodename
        echo 'svid='$1 > options.cfg
	echo 'offi='$2 >> options.cfg
	echo 'port='$3 >> options.cfg
	echo 'addr=127.0.0.1' >> options.cfg
        mkdir key
        chmod go-rx key/
	if [ $i -gt 1 ] 
	then
            echo 'peer=127.0.0.1:'$4 >> options.cfg
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
        echo 'host=127.0.0.1'> settings.cfg
        echo 'port=9091' >> settings.cfg
        echo 'address=0001-00000000-XXXX' >> settings.cfg
        echo 'secret=14B183205CA661F589AD83809952A692DFA48F5D490B10FD120DA7BF10F2F4A0' >> settings.cfg
    }
    fi

    if [ $2 == 2 ]
    then
    {
        echo 'host=127.0.0.1' > settings.cfg
        echo 'port=9091' >> settings.cfg
        echo 'address=0001-00000001-XXXX' >> settings.cfg
        echo 'secret=5BF11F5D0130EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C' >> settings.cfg
    }
    fi

    if [ $2 == 3 ]
    then
    {
        echo 'host=127.0.0.1' > settings.cfg
        echo 'port=9092' >> settings.cfg
        echo 'address=0002-00000000-XXXX' >> settings.cfg
        echo 'secret=E4882F10389E14D7E55050E2D05EF50701A8A5FD8D9AE188D9AE8F377E410254' >> settings.cfg
    }
    fi
    cd ..
}

function initFirstNode
{
    echo '.............................initFirstNode\n'

    cd 'node1'
    nohup ./escd --init 1 > nodeout.txt &
    ##./escd --init 1 > nodeout.txt &

    cd ..
    sleep 60

    echo 'INITALIZATION NODE 1 FINISHED'
}

function prepareClient
{
        echo '.............................prepareClient'$1

        username='user'$1
	mkdir $username 
	ln -s $current/esc $username/esc
	ln -s $current/ed25519/key $username/key

        prepareClientKeys $username $1
}


function setUpUser1
{
    echo '.............................setUpUser1'

    cd 'user1'

    echo '{"run":"get_me"}' | ./esc

    echo '.............................change_account_key for User 1'

    (echo '{"run":"get_me"}'; echo '{"run":"change_account_key","pkey":"D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17","signature":"7A1CA8AF3246222C2E06D2ADE525A693FD81A2683B8A8788C32B7763DF6037A5DF3105B92FEF398AF1CDE0B92F18FE68DEF301E4BF7DB0ABC0AEA6BE24969006"}') | ./esc

    sleep 60

    echo 'host=127.0.0.1'> settings.cfg
    echo 'port=9091' >> settings.cfg
    echo 'address=0001-00000000-9B6F' >> settings.cfg
    echo 'secret=FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6' >> settings.cfg

    echo '{"run":"get_me"}' | ./esc
    cd ..

    sleep 25
}

function createAccountForUser2
{
    echo '.............................createAccountForUser2'

    cd 'user1'

    echo '----------------------------------'

    (echo '{"run":"get_me"}'; echo '{"run":"create_account","node":"0001"}') | ./esc

    sleep 60

    echo '{"run":"get_account","address":"0001-00000001-8B4E"}' | ./esc

    echo '----------------------------------'

    sleep 60

    (echo '{"run":"get_me"}'; echo '{"run":"send_one","address":"0001-00000001-8B4E","amount":0.1,"message":"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"}') | ./esc

    echo '{"run":"get_account","address":"0001-00000001-8B4E"}' | ./esc | grep balance

    cd ..
}



function changeKeysforUser2
{
    echo '.............................changeKeysforUser2'

    cd 'user1'

    (echo '{"run":"get_me"}'; echo '{"run":"change_account_key","pkey":"C9965A1417F52B22514559B7608E4E2C1238FCA3602382C535D42D1759A2F196","signature":"ED8479C0EDA3BB02B5B355E05F66F8161811F5AD9AE9473AA91E2DA32457EAB850BC6A04D6D4D5DDFAB4B192D2516D266A38CEA4251B16ABA1DF1B91558A4A05"}' )  | ./esc  --address 0001-00000001-8B4E

    cd ..

    cd 'user2'

    echo 'host=127.0.0.1' > settings.cfg
    echo 'port=9092' >> settings.cfg
    echo 'address=0001-00000001-8B4E' >> settings.cfg
    echo 'secret=5BF11F5D0130EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C' >> settings.cfg
    chmod go-r settings.cfg

    sleep 60

    echo '{"run":"get_me"}' | ./esc

    cd ..
}


function sendCash
{
    echo '.............................sendCash'

    cd 'user1'

    (echo '{"run":"get_me"}'; echo '{"run":"send_one","address":"'$1'","amount":'$2',"message":"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"}') | ./esc

    cd ..

    sleep 25
}

function checkBalance
{
    echo "'$PWD'"
    echo "'$1'"

    cd $1

    balance=$(echo '{"run":"get_account","address":"'$2'"}' | ./esc | grep balance)

    echo $balance

    if [[ "$balance" == *"$3"* ]]
    then
        echo "TEST OK USER:'$1'"
    else
        echo "ERROR: WRONG BALANCE: $balance USER '$1'"
        cd ..
        exit 1
    fi

    cd ..
}

function addNode
{
    cd 'user1'
    (echo '{"run":"get_me"}'; echo '{"run":"create_node"}') | ./esc

    cd ..
}

function changeNode2Key
{
    cd 'node2'

    echo 'FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6' > key/key.txt
    cd ..

    cd 'user1'

    echo '.............................change_node_key'

    (echo '{"run":"get_me"}'; echo '{"run":"change_node_key","pkey":"D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17","node":"2"}') | ./esc

    cd ..
}


pkill -f "escd"
pkill -f "escd"
deploypath="deployment"
current=$PWD

cd ..
rm -rf $deploypath
mkdir $deploypath
cd $deploypath

nodename='node'

officeport=9090
serverport=8090
peerport=8090

for i in 1 2 3
do
	nodename='node'$i
	echo 'test'
	echo $nodename	
        let "officeport = $officeport + 1"
        let "serverport = $serverport + 1"
        let "peerport   = $serverport-1"

	prepareNode $i $officeport $serverport $peerport
	prepareClient $i        	
done


initFirstNode
setUpUser1
createAccountForUser2
changeKeysforUser2

for i in {1..4}
do
    sendCash "0001-00000001-8B4E" 70.0
done

addNode
checkBalance "user1" "0001-00000001-8B4E" "280."

sleep 60

changeNode2Key

sleep 120

cd 'node2'

for file in $(find ../node1/blk -name servers.srv | sort -rn)
do
cp ../node1/$file ./
echo $file
break;
done


echo '.............................nohup node2'

nohup ./escd -m 1 -f 1 > nodeout.txt &
cd ..

echo 'server started'

sleep 60

checkBalance "user1" "0001-00000001-8B4E" "280."
#checkBalance "user1" "0001-00000001-8B4E" "280."
#checkBalance "user2" "0001-00000001-8B4E" "281."






