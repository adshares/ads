#!/bin/bash


function prepareNode
{
        mkdir $nodename
	ln -s $current/escd $nodename/escd

	cd $nodename
	echo 'svid='$1 > options.cfg
	echo 'offi='$2 >> options.cfg
	echo 'port='$3 >> options.cfg
	echo 'addr=127.0.0.1' >> options.cfg
	if [ $i -gt 1 ] 
	then
            echo 'peer=127.0.0.1:'$4 >> options.cfg
	fi
	cd ..
}

function prepareClientKeys
{
    cd $1

    if [ $2 == 1 ]
    then
    {
        echo 'address=0001-00000000-XXXX' >> settings.cfg
        echo 'secret=14B183205CA661F589AD83809952A692DFA48F5D490B10FD120DA7BF10F2F4A0' >> settings.cfg
    }
    fi

    if [ $2 == 2 ]
    then
    {
        echo 'address=0002-00000000-XXXX' >> settings.cfg
        echo 'secret=5BF11F5D0130EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C' >> settings.cfg
    }
    fi

    if [ $2 == 3 ]
    then
    {
        echo 'address=0003-00000000-XXXX' >> settings.cfg
        echo 'secret=E4882F10389E14D7E55050E2D05EF50701A8A5FD8D9AE188D9AE8F377E410254' >> settings.cfg
    }
    fi
    cd ..
}

function initFirstNode
{
    echo 'server starting'

    cd 'node1'
    nohup ./escd --init 1 > nodeout.txt &
    ##./escd --init 1 > nodeout.txt &

    cd ..
    sleep 40

    echo 'INITALIZATION NODE 1 FINISHED'
}

function prepareClient
{
        username='user'$1
	mkdir $username 
	ln -s $current/esc $username/esc
	ln -s $current/ed25519/key $username/key

        prepareClientKeys $username $1
}



deploypth="deployment"

current=$PWD



cd ..
mkdir $deploypth
cd $deploypth

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
        let "peerport = $serverport-1"

	prepareNode $i $officeport $serverport $peerport
	prepareClient $i        	
done

initFirstNode

cd 'user1'

echo '{"run":"get_me"}' | ./esc
(echo '{"run":"get_me"}'; echo '{"run":"change_account_key","pkey":"D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17","signature":"7A1CA8AF3246222C2E06D2ADE525A693FD81A2683B8A8788C32B7763DF6037A5DF3105B92FEF398AF1CDE0B92F18FE68DEF301E4BF7DB0ABC0AEA6BE24969006"}') | ./esc

sleep 1m

rm settings.cfg
echo 'address=0001-00000000-9B6F' >> settings.cfg
echo 'secret= FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6' >> settings.cfg

echo '{"run":"get_me"}' | ./esc

sleep 1m

echo '----------------------------------'

(echo '{"run":"get_me"}'; echo '{"run":"create_account","node":"0001"}') | ./esc

sleep 1m

echo '{"run":"get_account","address":"0001-00000001-8B4E"}' | ./esc

echo '----------------------------------'

sleep 1m

(echo '{"run":"get_me"}'; echo '{"run":"send_one","address":"0001-00000001-8B4E","amount":0.1,"message":"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"}') | ./esc

echo '{"run":"get_account","address":"0001-00000001-8B4E"}' | ./esc | grep balance













echo 'server started'



