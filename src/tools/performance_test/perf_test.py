"""
This test should be run using python3 from directory containing esc binary (or symbolic link)
Node 1 must be running

1) create 10k accounts
2) accounts 0..7 receive 1M esc, other accounts 1 esc
3) SEND_ONE TEST: accounts 0..7 send 10 esc to accounts 6001..7250 (10k transactions in total)
4) create token accounts for all users (10k) for token 0
5) account 0 adds 10M own tokens and moves 1M own tokens to accounts 1..7
6) MOVE_TOKENS TEST: accounts 0..7 move 10 tokens to accounts 6001..7250 (10k transactions in total)
"""

from time import *
from multiprocessing import Process
import os


def get_account_address(account_id):
    return '0001-' + str(hex(account_id)[2:]).rjust(8, '0') + '-XXXX'


def prepare_accounts(num_accounts, num_esc):
    create_account_directories(num_esc)
    create_accounts(num_esc, num_accounts)


def create_account_directories(num_accounts):
    for i in range(num_accounts):
        path = 'user_' + str(i)
        if not os.path.exists(path):
            os.makedirs(path)
            os.chdir(path)
            os.symlink('../esc', 'esc')
            settings = open('settings.cfg', 'w')
            settings.write('port=9091\n')
            settings.write('host=127.0.0.1\n')
            settings.write('address=%s\n' % get_account_address(i))
            settings.write('secret=14B183205CA661F589AD83809952A692DFA48F5D490B10FD120DA7BF10F2F4A0\n')
            os.chdir('..')


def create_accounts(num_esc, num_accounts):
    print('create_accounts')
    os.chdir("user_0")
    p = os.popen('./esc -w . > /dev/null 2>&1', 'w')

    p.write('{"run":"get_me"}\n')
    for i in range(1, num_accounts+1):
        amount = '1000000' if i < num_esc else '1'
        p.write('{"run":"create_account"}\n')
        p.write('{"run":"send_one","address":"' + get_account_address(i) + '","amount":"' + amount + '"}\n')

    p.close()
    os.chdir('..')


def prepare_token_accounts(num_accounts, num_esc):
    create_token_accounts(num_accounts)
    add_tokens_to_first_account()
    move_tokens_to_first_accounts(num_esc)


def send_one_from_client(client_id, send_cmd):
    os.chdir('user_' + str(client_id))
    print('send_one from: ' + str(os.getcwd()))

    p = os.popen('./esc -w . > /dev/null 2>&1', 'w')
    p.write('{"run":"get_me"}\n')

    for cmd in send_cmd:
        p.write(cmd)

    p.close()


def print_results(transaction_type, delta, num_transactions):
    print(transaction_type + ': %.2f s' % delta)
    print(transaction_type + ': %.2f transactions/s\n' % (num_transactions/delta))


def get_me_from_client(client_id, txs_per_client):
    os.chdir('user_' + str(client_id))
    print('get_me from: ', str(os.getcwd()))
    p = os.popen('./esc -w . > /dev/null 2>&1', 'w')

    for _ in range(txs_per_client):
        p.write('{"run":"get_me"}\n')

    p.close()


def test_get_me(num_transactions, num_clients, num_exec):
    txs_per_client = num_transactions//num_clients
    print("per client: ", txs_per_client)

    for _ in range(num_exec):
        threads = []

        for i in range(num_clients):
            threads.append(Process(target=get_me_from_client, args=(i, txs_per_client,)))

        start = time()

        for i in threads:
            i.start()

        for i in threads:
            i.join()

        end = time()
        delta = end - start
        print_results('get_me', delta, num_transactions)


def build_send_one(txs_per_client):
    send_cmd = []

    offset = 6000

    for i in range(offset, offset + txs_per_client):
        send_cmd.append('{"run":"send_one","address":"' + get_account_address(i) + '","amount":1}\n')

    return send_cmd


def test_send_one(num_transactions, num_esc, num_exec):
    send_cmd = build_send_one(num_transactions//num_esc)

    for _ in range(num_exec):
        threads = []

        for i in range(num_esc):
            threads.append(Process(target=send_one_from_client, args=(i, send_cmd)))

        start = time()

        for i in threads:
            i.start()

        for i in threads:
            i.join()

        end = time()
        delta = end - start
        print_results('send_one', delta, num_transactions)


def create_token_accounts(num_accounts):
    print('create_token_accounts')
    os.chdir('user_0')

    for i in range(num_accounts+1):
        p = os.popen('./esc -w . --address ' + get_account_address(i) + ' > /dev/null 2>&1', 'w')
        p.write('{"run":"get_me"}\n')
        p.write('{"run":"create_token_account", "token":"00000000"}\n')
        p.close()

    os.chdir('..')


def add_tokens_to_first_account():
    os.chdir("user_0")

    p = os.popen('./esc -w . > /dev/null 2>&1', 'w')
    p.write('{"run":"get_me"}\n')
    p.write('{"run":"add_tokens", "amount":"20000000"}\n')
    p.close()

    os.chdir("..")


def move_tokens_to_first_accounts(num_esc):
    os.chdir('user_0')

    p = os.popen('./esc -w . > /dev/null 2>&1', 'w')
    p.write('{"run":"get_me"}\n')

    for i in range(1, num_esc):
        p.write('{"run":"move_tokens", "token":"00000000", "to":"' + get_account_address(i) + '", "amount":"1000000"}\n')

    p.close()
    os.chdir('..')


def build_move_tokens(txs_per_client):
    move_cmd = []

    offset = 6000

    for i in range(offset, offset + txs_per_client):
        move_cmd.append('{"run":"move_tokens", "token":"00000000", "to":"' + get_account_address(i) + '","amount":1}\n')

    return move_cmd


def move_tokens_from_client(client_id, move_cmd):
    os.chdir('user_' + str(client_id))
    print('move_tokens from: ' + str(os.getcwd()))

    p = os.popen('./esc -w . > /dev/null 2>&1', 'w')
    p.write('{"run":"get_me"}\n')

    for cmd in move_cmd:
        p.write(cmd)

    p.close()


def test_move_tokens(num_transactions, num_esc, num_exec):
    move_cmd = build_move_tokens(num_transactions//num_esc)

    for _ in range(num_exec):
        threads = []
        for i in range(num_esc):
            threads.append(Process(target=move_tokens_from_client, args=(i, move_cmd)))

        start = time()

        for i in threads:
            i.start()

        for i in threads:
            i.join()

        end = time()
        delta = end - start
        print_results('move_tokens', delta, num_transactions)


def run_tests():
    num_accounts = 10000
    num_transactions = 10000
    num_esc = 16

    prepare_accounts(num_accounts, num_esc)
    test_get_me(num_transactions, num_esc, num_exec=1)
    test_send_one(num_transactions, num_esc, num_exec=10)

    prepare_token_accounts(num_accounts, num_esc)
    test_move_tokens(num_transactions, num_esc, num_exec=1)


run_tests()
