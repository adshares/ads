#!/usr/bin/env bash
folder_user_1="/tmp/client/1"
folder_user_2="/tmp/client/2"

cd $folder_user_1
#echo "USER: $folder_user_1"
#echo '-----------------------get_me_&_create_account---------------------------'
#(echo '{"run":"get_me"}'; echo '{"run":"create_account","node":"0001"}') | /home/denis/projects/hpx/build/esc/esc
#
#sleep 60
#
#echo '-----------------------get_accounts-----------------------------'
#echo '{"run":"get_accounts"}' | /home/denis/projects/hpx/build/esc/esc
#
#echo '-----------------------get_me_&_change_account_key-----------------------'
#(echo '{"run":"get_me"}'; echo '{"run":"change_account_key","pkey":"C9965A1417F52B22514559B7608E4E2C1238FCA3602382C535D42D1759A2F196","signature":"ED8479C0EDA3BB02B5B355E05F66F8161811F5AD9AE9473AA91E2DA32457EAB850BC6A04D6D4D5DDFAB4B192D2516D266A38CEA4251B16ABA1DF1B91558A4A05"}' )  | /home/denis/projects/hpx/build/esc/esc  --address 0001-00000001-8B4E
#
#cd $folder_user_1
#echo '----------------------get_me-------------------------------'
#echo '{"run":"get_me"}' | /home/denis/projects/hpx/build/esc/esc
#
#cd $folder_user_2
#
#echo "USER: $folder_user_2"
#
#echo 'host=127.0.0.1' > settings.cfg
#echo 'port=8000' >> settings.cfg
#echo 'address=0001-00000001-8B4E' >> settings.cfg
#echo 'secret=5BF11F5D0130EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C' >> settings.cfg
#chmod go-r settings.cfg
#
#echo '{"run":"get_me"}' | /home/denis/projects/hpx/build/esc/esc

   echo '.............................ADD node'

    cd $folder_user_1
    (echo '{"run":"get_me"}'; echo '{"run":"get_block"}') | /home/denis/projects/hpx/build/esc/esc

    cd ..

    sleep 60

