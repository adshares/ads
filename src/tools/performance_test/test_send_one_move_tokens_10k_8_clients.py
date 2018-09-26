# This test should be run using python3 from directory containing esc and key binaries (or symbolic links)
# Node 1 must be running

# 1) create 10k accounts
# 2) accounts 0..7 receive 1M esc, other accounts 1 esc
# 3) SEND_ONE TEST: accounts 0..7 send 10 esc to accounts 6001..7250 (10k transactions in total)
# 4) create token accounts for all users (10k) for token 0
# 5) account 0 adds 10M own tokens and moves 1M own tokens to accounts 1..7
# 6) MOVE_TOKENS TEST: accounts 0..7 move 10 tokens to accounts 6001..7250 (10k transactions in total)

from time import *
from multiprocessing import Process
import os


def get_account_address(account_id):
    return '0001-' + str(hex(account_id)[2:]).rjust(8, '0') + '-XXXX'


def create_account_directories(num_accounts):
    for i in range(num_accounts):
        path = 'user_' + str(i)
        if not os.path.exists(path):
            os.makedirs(path)
            os.chdir(path)
            os.symlink('../key', 'key')
            os.symlink('../esc', 'esc')
            settings = open('settings.cfg', 'w')
            settings.write('port=9091\n')
            settings.write('host=127.0.0.1\n')
            settings.write('address=%s\n' % get_account_address(i))
            settings.write('secret=14B183205CA661F589AD83809952A692DFA48F5D490B10FD120DA7BF10F2F4A0\n')
            os.chdir('..')


def create_1000_accounts(idx):
    for i in range(idx+1, idx+1000+1):
        amount = '1'
        if i < 8:
            amount = '1000000'

        cmd = """(echo '{"run" : "get_me"}'; echo '{"run":"create_account"}'; echo '{"run":"send_one","address":\"""" + get_account_address(i) + """\","amount":""" + amount + """}') | ./esc -w . > /dev/null 2>&1"""
        os.system(cmd)


def create_accounts(num_accounts):
    print('create_accounts')
    os.chdir("user_0")
    for i in range(0, int(num_accounts/1000)):
        create_1000_accounts(idx=i*1000)
    os.chdir("..")


def send_one_from_client(client_id, send_cmd):
    path = 'user_' + str(client_id)
    os.chdir(path)
    print('send_one from: ' + str(os.getcwd()))

    for cmd in send_cmd:
        os.system(cmd)


def build_send_cmd(num_accounts, num_clients):
    recipients = num_accounts//num_clients  # recipients is 1250
    rounds = recipients//1000  # rounds == 1
    offset = 6000

    send_cmd = []
    # accounts 6001..7000
    for i in range(0, rounds):
        send_cmd.append(build_send_one(offset + i*1000 + 1, offset+1000 + i*1000 + 1))

    # accounts 7001..7250
    send_cmd.append(build_send_one(offset+rounds*1000+1, offset+recipients+1))

    return send_cmd


def print_results(transaction_type, delta, num_transactions):
    print(transaction_type + ': %.2f s' % delta)
    print(transaction_type + ': %.2f transactions/s' % (num_transactions/delta))


def get_me_from_client(client_id, get_me_cmd):
    path = 'user_' + str(client_id)
    os.chdir(path)
    print('get_me from: ' + str(os.getcwd()))

    for cmd in get_me_cmd:
        os.system(cmd)


def build_get_me(number_cmd):
    cmd = '('

    for _ in range(number_cmd):
        cmd += """echo '{"run":"get_me"}'; """

    cmd += ') | ./esc -w . > /dev/null 2>&1'

    return cmd


def build_get_me_cmd(num_transactions, num_clients):
    num_get_me = num_transactions//num_clients  # num_get_me is 1250
    rounds = num_get_me//1000  # rounds == 1

    get_me_cmd = []
    #1k
    for i in range(0, rounds):
        get_me_cmd.append(build_get_me(1000))

    #250
    get_me_cmd.append(build_get_me(num_get_me - rounds*1000))

    return get_me_cmd


def test_get_me(num_transactions, num_clients):
    get_me_cmd = build_get_me_cmd(num_transactions, num_clients)

    threads = []

    for i in range(num_clients):
        threads.append(Process(target=get_me_from_client, args=(i, get_me_cmd,)))

    start = time()

    for i in threads:
        i.start()

    for i in threads:
        i.join()

    end = time()
    delta = end - start
    print_results('get_me', delta, num_transactions)


def test_send_one(num_accounts, num_clients):
    send_cmd = build_send_cmd(num_accounts, num_clients)

    threads = []

    for i in range(num_clients):
        threads.append(Process(target=send_one_from_client, args=(i, send_cmd,)))

    start = time()

    for i in threads:
        i.start()

    for i in threads:
        i.join()

    end = time()
    delta = end - start
    print_results('send_one', delta, num_accounts)


def build_send_one(beg, end):
    cmd = """(echo '{"run":"get_me"}'; """

    for j in range(beg, end):
        cmd += """echo '{"run":"send_one","address":\"""" + get_account_address(j) + """\","amount":10}';"""

    cmd += """)| ./esc -w . > /dev/null 2>&1"""

    return cmd


def create_token_accounts(num_accounts):
    os.chdir('user_0')
    for i in range(0, num_accounts+1):
        create_token_account_cmd = """(echo '{"run":"get_me"}'; echo '{"run":"create_token_account", "token":"00000000"}') | ./esc -w . --address """ + get_account_address(i) + """ > /dev/null 2>&1"""
        os.system(create_token_account_cmd)
    os.chdir('..')


def add_tokens_to_first_account():
    os.chdir("user_0")
    add_tokens_cmd = """(echo '{"run":"get_me"}'; echo '{"run":"add_tokens", "amount":"10000000"}') | ./esc -w . > /dev/null 2>&1"""
    os.system(add_tokens_cmd)
    os.chdir("..")


def move_tokens_to_first_accounts(num_accounts):
    os.chdir('user_0')
    for i in range(1, num_accounts):
        cmd = """(echo '{"run":"get_me"}'; echo '{"run":"move_tokens", "token":"00000000", "to":\"""" + get_account_address(i) + """\", "amount":"1000000"}') | ./esc -w . > /dev/null 2>&1"""
        os.system(cmd)
    os.chdir('..')


def build_move_tokens(beg, end):
    cmd = """(echo '{"run":"get_me"}'; """

    for j in range(beg, end):
        cmd += """echo '{"run":"move_tokens", "token":"00000000", "to":\"""" + get_account_address(j) + """\", "amount":"10"}'; """

    cmd += """)| ./esc -w . > /dev/null 2>&1"""

    return cmd


def move_tokens_from_client(client_id, move_cmd):
    path = 'user_' + str(client_id)
    os.chdir(path)
    print('move_tokens from: ' + str(os.getcwd()))

    for cmd in move_cmd:
        os.system(cmd)


def build_move_cmd(num_accounts, num_clients):
    recipients = num_accounts//num_clients  # recipients == 1250
    rounds = recipients//1000  # rounds == 1
    offset = 6000

    move_cmd = []
    # accounts 6001..7000
    for i in range(0, rounds):
        move_cmd.append(build_move_tokens(offset + i*1000 + 1, offset+1000 + i*1000 + 1))

    # accounts 7001..7250
    move_cmd.append(build_move_tokens(offset+rounds*1000+1, offset+recipients+1))

    return move_cmd


def test_move_tokens(num_accounts, num_clients):
    move_cmd = build_move_cmd(num_accounts, num_clients)

    threads = []

    for i in range(num_clients):
        threads.append(Process(target=move_tokens_from_client, args=(i, move_cmd)))

    start = time()

    for i in threads:
        i.start()

    for i in threads:
        i.join()

    end = time()
    delta = end - start
    print_results('move_tokens', delta, num_accounts)


def run_tests():
    num_accounts = 10000
    num_clients = 8

    create_account_directories(num_clients)
    create_accounts(num_accounts)
    test_get_me(num_accounts, num_clients)
    test_send_one(num_accounts, num_clients)
    create_token_accounts(num_accounts)
    add_tokens_to_first_account()
    move_tokens_to_first_accounts(num_clients)
    test_move_tokens(num_accounts, num_clients)


run_tests()
