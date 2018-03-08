#!/bin/bash

deploypath="testuser"
current=$PWD


officeport=7190


if [ $# -eq 1 ]
then
officeport=$1
fi

echo $officeport


let "user1office = $officeport + 1"
let "user2office = $officeport + 1"
let "user3office = $officeport + 2"

userpath[1]="user1"
userpath[2]="user2"
userpath[3]="user3"
userpath[4]="user4"

echo ${userpath[1]}
echo ${userpath[2]}
echo ${userpath[3]}
echo ${userpath[3]}



function prepareClientKeys
{
    echo '.............................prepareClientKeys'$1

    cd $1

    if [ $2 == 1 ]
    then
    {
        echo 'host=127.0.0.1'> settings.cfg
        echo 'port='$user1office >> settings.cfg
        echo 'address=0001-00000000-XXXX' >> settings.cfg
        echo 'secret=FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6' >> settings.cfg
    }
    fi

    if [ $2 == 2 ]
    then
    {
        echo 'host=127.0.0.1' > settings.cfg
        echo 'port='$user2office >> settings.cfg
        echo 'address=0001-00000001-XXXX' >> settings.cfg
        echo 'secret=5BF11F5D0130EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C' >> settings.cfg
    }
    fi

    if [ $2 == 3 ]
    then
    {
        echo 'host=127.0.0.1' > settings.cfg
        echo 'port='$user3office >> settings.cfg
        echo 'address=0002-00000000-75BD' >> settings.cfg
        echo 'secret=FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6' >> settings.cfg
    }
    fi

    if [ $2 == 4 ]
    then
    {
        echo 'host=127.0.0.1' > settings.cfg
        echo 'port='$user3office >> settings.cfg
        echo 'address=0002-00000001-659C' >> settings.cfg
        echo 'secret=FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6' >> settings.cfg
    }
    fi

    cd ..
}


function prepareClient
{
        echo '.............................prepareClient'$1

        username=${userpath[$i]}
	mkdir $username 
	ln -s $current/esc $username/esc
	ln -s $current/ed25519/key $username/key

        prepareClientKeys $username $1
}



cd ..
rm -rf $deploypath
mkdir $deploypath
cd $deploypath


for i in 1 2 3 4
do
    prepareClient $i
done



