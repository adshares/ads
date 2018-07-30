# Enterprise Service Chain

Enterprise Service Chain (ESCd) is a block chain based software tool facilitating 
high volumes of simple transactions that, in most cases, send tokens between accounts as in other crypto currencies. The name is derived from the concept of the Enterprise Service Bus in which a crypto currency is used as the protocol of communication.

The main features of the Enterprise Service Chain can be summarized as follows:
-	Delegated Proof of Stake as block consensus mechanism to reduce network maintenance costs
-	Small account and transaction identifiers, reduced transaction set and parallel processing of transactions to facilitate high transaction volumes (>100kHz one-one transactions, >1MHz one-many transactions)
-	Nodes are heavily penalized for double spends so most transactions can be trusted almost instantly
-	Small set of VIP nodes responsible for network integrity to facilitate incorporation of slow nodes with reduced transaction processing capabilities
-	Hash of all accounts is part of the block and enables instant synchronization with the block chain
-	Hierarchical organization of accounts and nodes facilitates KYC, AML , eID  supply and governance
-	Dividend payments to account holders and node managers to support the growth of the economy of the ESC system

## Details

A [white paper](docs/ESC.pdf) provides a more detailed description of the concept of the Enterprise Service Chain.

## Current status

The software is currently an academic proof of concept. It does not have a user-friendly class structure. It lacks comments. It has small bugs. It has a long ToDo list, but it can be deployed and tested. An instruction for the deployment will be provided shortly.

## Installation

The software is developed for Linux platform. There is no intent to support other platforms. The software was tested on Debian and Ubuntu. To compile the software You need boost and ssl libraries. Check the Makefile if anything else is missing.

```
sudo apt-get install openssl libboost-all-dev libssl-dev
```

Start with cloning the git directory.

```
git clone https://github.com/EnterpriseServiceChain/esc
```

Before compiling ESC You could check if the ed25519 cryptography software is working correctly and what compile option gives You better speed for Your CPU.

```
cd esc/ed25519
make -f Makefile.sse2
./mytest
make clean
make
./mytest
```

I our case compiling for sse2 reduces the speed of batch signature verifications by over 30% (it is not the case on AWS). The results of the 2 runs are shown below. The line stating with "BATC" reports number of signature verifications per second. Compiling against the regular Makefile (`make`) generates a code that can perform over 30k signature verifications per second.

```
START
HASH: 1295553.388/s [79.074MiB/s]
PKEY: 29027.926/s
SIGN: 28596.429/s [1.745MiB/s]
OPEN: 8721.537/s [0.532MiB/s]
BATC: 18608.159/s [1.136MiB/s]

START
HASH: 836288.155/s [51.043MiB/s]
PKEY: 48491.258/s
SIGN: 47754.419/s [2.915MiB/s]
OPEN: 14017.216/s [0.856MiB/s]
BATC: 30312.243/s [1.850MiB/s]
```

Keep the compiled object files in the directory and compile ESC.

```
cd ..
make
```

The default Makefile contains the DEBUG option. If You want to run production speed tests, then compile against Makefile.ndebug. The compilation should finish in few seconds.

When compiling, the compiler will detect if You have a recent kernel that supports the "FALLOC_FL_COLLAPSE_RANGE" option for fallocate(). This option is used to remove old (first) pages from log files and works only on ext4 file systems. If You don't have a recent kernel or run the software on a different file system, then user log files will not be purged (will grow forever). We will consider adding support for this case in the future.

## Setting up a local test net

Create a directory for Your first node and the first user and create links to executables for convenience.

```
mkdir /tmp/node1
ln -s $PWD/esc /tmp/node1/esc
mkdir /tmp/user0
ln -s $PWD/esc /tmp/user0/esc
ln -s $PWD/ed25519/key  /tmp/user0/key
cd /tmp/node1/
```

To start the first node enter

```
mkdir ../node1
cd ../node1
echo 'svid=1' > options.cfg
echo 'offi=9091' >> options.cfg
echo 'port=8091' >> options.cfg
./escd --init 1
```

The program will detect that it is in an empty directory and will create an initial setup with a single node and an administrator account for the node. The current version is compiled with temporary development setting with a much shorter block period (32s, defined in default.hpp). Keep the node running for at least 1 block period to initialize the block-chain correctly. You can stop a node by typing a dot (".") followed by enter. Currently, killing the node with a signal (^C) may destroy the database files. You can continue with the same block-chain by running the code again with the '--init 1' switch.

```
./escd --init 1
```

The first node will start with the default node secret key stored in key/key.txt . The secret key to the admin account is the same. Secret keys can be created by running the key executable with a selected brain-key-string (ed25519/key "brain-key-string"). Both keys can be changed later. In production the node key and the admin key should differ for security reasons.
To add more nodes and users open a new terminal and connect to the running node as admin (user with account number 0). We have created a directory for the user previously. Let's go there.

```
cd /tmp/user0
```

Let's create the file containing the connection setting for the user.

```
echo 'port=9091' > settings.cfg
echo 'host=127.0.0.1' >> settings.cfg
echo 'address=0001-00000000-XXXX' >> settings.cfg
echo 'secret=14B183205CA661F589AD83809952A692DFA48F5D490B10FD120DA7BF10F2F4A0' >> settings.cfg
chmod go-r settings.cfg
```

This is the account address of our user 0001-00000000-XXXX. Last 4 characters should be hex characters defining the checksum. If 'XXXX' is provided the checksum is not tested. Let's try to connect to the node and get the current status of our user.

```
echo '{"run":"get_me"}' | ./esc 2>err.txt
```

This command should list the current status of the user. We should get something like this:

```
{
    "tx": {
        "data": "1001000000000001000000000045975E5960A14655AC8341861CECEAB040054058CC3EF62708565B75C4173FB80D60BD74D7DFCFE3EC3C441A95B48D703E6578BFBC161AF3CC1A0F118B32D278336C5704",
        "account_msid": "0"
    },
    "account": {
        "address": "0001-00000000-9B6F",
        "node": "1",
        "id": "0",
        "msid": "1",
        "time": "1499368832",
        "date": "2017-07-06 21:20:32",
        "status": "0",
        "paired_node": "1",
        "paired_id": "0",
        "local_change": "1499368832",
        "remote_change": "1499371296",
        "balance": "4611617760.726175776",
        "public_key": "7D21F4EE7DE72EEDDC2EBFFEC5E7F33F140A975A629EE312075BB04610A9CFFF",
        "hash": "000000000100FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
    },
    "network_account": {
        "address": "0001-00000000-9B6F",
        "node": "1",
        "id": "0",
        "msid": "1",
        "time": "1499368832",
        "date": "2017-07-06 21:20:32",
        "status": "0",
        "paired_node": "1",
        "paired_id": "0",
        "local_change": "1499368832",
        "remote_change": "1499371296",
        "balance": "4611617760.726175776",
        "public_key": "7D21F4EE7DE72EEDDC2EBFFEC5E7F33F140A975A629EE312075BB04610A9CFFF",
        "hash": "000000000100FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
        "checksum": "true"
    }
}
```

No standard output means the connection failed. You can try to examine err.txt for some clues. Correct standard output show the correct account number for the admin of the first node, which is "0001-00000000-9B6F" (the checksum is 9B6F).
Now let's change our secret key and create a new account with a new key. First let's generate 2 keys.

```
./key "user-0-0"
```

```
SK: FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6
PK: D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17
SG: 7A1CA8AF3246222C2E06D2ADE525A693FD81A2683B8A8788C32B7763DF6037A5DF3105B92FEF398AF1CDE0B92F18FE68DEF301E4BF7DB0ABC0AEA6BE24969006
```

```
./key "user-0-1"
```

```
SK: 5BF11F5D0130EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C
PK: C9965A1417F52B22514559B7608E4E2C1238FCA3602382C535D42D1759A2F196
SG: ED8479C0EDA3BB02B5B355E05F66F8161811F5AD9AE9473AA91E2DA32457EAB850BC6A04D6D4D5DDFAB4B192D2516D266A38CEA4251B16ABA1DF1B91558A4A05
```

The secret keys are printed in the lines starting with "SK:". The line starting with "SG:" contains the signature of an empty phrase signed with the secret key. This signature is uses as checksum when creating a new account. Let's change the key for the admin account now:

```
(echo '{"run":"get_me"}'; echo '{"run":"change_account_key",
"pkey":"D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17",
"signature
":"7A1CA8AF3246222C2E06D2ADE525A693FD81A2683B8A8788C32B7763DF6037A5DF3105B92FEF398AF1CDE0B92F18FE68DEF301E4BF7DB0ABC0AEA6BE24969006"}') | ./esc
```

After this the admin needs a new secret key to connect to its account, so let's fix the settings.txt file.

```
echo 'port=9091' > settings.cfg
echo 'host=127.0.0.1' >> settings.cfg
echo 'address=0001-00000000-9B6F' >> settings.cfg
echo 'secret= FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6' >> settings.cfg
```

And confirm that we can connect again with the new key.

```
echo '{"run":"get_me"}' | ./esc 2>err.txt
```

The output should indicate that our transaction id was incremented and is now equal 2 ("msid": "2",). Let's now create the second user.

```
(echo '{"run":"get_me"}'; echo '{"run":"create_account","node":"0001"}') | ./esc
```

The new user is managed by our node so the creation process will be fast and the node will report the new account number for the local user in the paired_id field ("paired_id": "1"). Let's read the status of the new user account.

```
echo '{"run":"get_account","address":"0001-00000001-XXXX"}' | ./esc
```

We should see that the correct new account address is "0001-00000001-8B4E". The balance of the new user is too small to make any transactions so let's send him some funds.

```
(echo '{"run":"get_me"}'; echo '{"run":"send_one","address":"0001-00000001-8B4E","amount":0.1,"message":"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"}') | ./esc
```

The new balance shold be 0.362144000

```
echo '{"run":"get_account","address":"0001-00000001-8B4E"}' | ./esc 2>/dev/null | grep balance
```

Let's change the public key of the new user by connecting as the new user with the current coppied key. Do not forget to use the "--address 0001-00000001-8B4E" here, otherwise You will change Your own public key. In normal cases You don't know the corresponding secret key so You will loose Your account. 

```
(echo '{"run":"get_me"}'; echo '{"run":"change_account_key","pkey":"C9965A1417F52B22514559B7608E4E2C1238FCA3602382C535D42D1759A2F196","signature":"ED8479C0EDA3BB02B5B355E05F66F8161811F5AD9AE9473AA91E2DA32457EAB850BC6A04D6D4D5DDFAB4B192D2516D266A38CEA4251B16ABA1DF1B91558A4A05"}' )  | ./esc  --address 0001-00000001-8B4E
```

The output should indicate that the public key was changed.
Let's connect as the new user after setting up the new environment.

```
mkdir ../user1
cd ../user1
echo 'port=9091' > settings.cfg
echo 'host=127.0.0.1' >> settings.cfg
echo 'address=0001-00000001-8B4E ' >> settings.cfg
echo 'secret= 5BF11F5D0130EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C ' >> settings.cfg
chmod go-r settings.cfg
echo '{"run":"get_me"}' | ../user0/esc
```

The output should indicate that You have successfully connected to the node as user "0001-00000001-8B4E". You don't have enough funds to create a new node. User0 will help You.

```
cd ../user0
(echo '{"run":"get_me"}'; echo '{"run":"send_one","address":"0001-00000001-8B4E","amount":70.0,"message":"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"}') | ./esc
```

Let's now try to create a new node. The new node will get the public key of the requesting user.

```
(echo '{"run":"get_me"}'; echo '{"run":"create_node"}') | ../user0/esc
```

It will take at least 1 block time for the network to create a new node. You can examine the log of the first node. Before block creation the node should show now info about 3 nodes (the first one is the unused node number 0). You should see lines like these

```
NOD: 00000000 00000000 ffff0000 00000000 595E8D80 0 0000000000000000 0
NOD: eef4217d c08c88e1 8936fa16 0000003C 595EA5BB 6 3FFFC1DB71A5379A 2
NOD: 145a96c9 e186f4ad ffff0002 00000000 595EA5A0 0 0000000FFFF08000 1
```

When the new node is created You can send some funds to the new admin account (0002-00000000-XXXX) if You plan to perform any transaction. We will skip it because we will only try to connect a new node.

Let's create the directory and the files for the new node

```
mkdir ../node2
cd ../node2
echo 'svid=2' > options.cfg
echo 'offi=9092' >> options.cfg
echo 'port=8092' >> options.cfg
echo 'addr=127.0.0.1' >> options.cfg
echo 'peer=127.0.0.1:8091' >> options.cfg
mkdir key
chmod go-rx key/
echo '5BF11F5D0130EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C' > key/key.txt
chmod go-r key/key.txt
```

The configuration file (options.txt) indicates an initial peer address that we want to start syncing from. To connect to the network we also need a recent network topology file that already includes our new node. This file can be found in a recent block directory of the running node (and all nodes on the network). The blocks are in ../node1/blk/xxx/xxxxx. The block numbers correspond to unix time. Let's take a servers.txt file from ../node1/blk/595/EA9C0/ .

```
cp ../node1/blk/595/EA9C0/servers.txt ./
```

Now we should be able to connect the new node to the network.

```
../node1/escd -m 1 -f 1
```

The connection should be established shortly. You can stop the node again by pressing '.' and enter. The -m switch indicates that we need only 1 signature to trust the blocks (we have only 1 node). The -f switch indicates that we want to start from the current status of the block-chain. After stopping the second node, we should start it again without the -f option to load the missing blocks.

```
../node1/escd -m 1
```

Connecting more nodes can be done iteratively . The nodes broadcast their IPs and ports on the network so there is no need to provide many peers in the options.cfg file.

Please remember that the current version is just a proof of concept. We will create a production version of the block-chain in the coming months.

