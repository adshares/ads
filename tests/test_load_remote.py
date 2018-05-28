import time
import threading
from tests.client.utils import (create_client_env, exec_esc_cmd,
                                get_balance_user)
from tests.utils import generate_message


HOST = '85.10.197.15'
PORT = 7191

USER_1 = {'client_id': 1,
          'address': '0001-00000000-9B6F',
          'host': HOST,
          'port': 7191,
          'secret': 'FF767FC8FAF9CFA8D2C3BD193663E8'
                    'B8CAC85005AD56E085FAB179B52BD88DD6'}

USER_2 = {'client_id': 2,
          'address': '0001-00000001-8B4E',
          'host': HOST,
          'port': 7191,
          'secret': '5BF11F5D0130EC994F04B6C5321566A853B'
                    '7393C33F12E162A6D765ADCCCB45C'}

USER_3 = {'client_id': 3,
          'address': '0002-00000000-75BD',
          'host': HOST,
          'port': 7192,
          'secret': 'FF767FC8FAF9CFA8D2C3BD193663E8B8CAC8'
                    '5005AD56E085FAB179B52BD88DD6',

          }
USER_4 = {'client_id': 4,
          'address': '0002-00000001-659C',
          'host': HOST,
          'port': 7192,
          'secret': 'FF767FC8FAF9CFA8D2C3BD193663E8B8CAC8'
                    '5005AD56E085FAB179B52BD88DD6'
          }

NODE_1 = '0001'
NODE_2 = '0002'
NODE_3 = '0003'
NODE_4 = '0004'

create_client_env(**USER_1)
create_client_env(**USER_2)
create_client_env(**USER_3)
create_client_env(**USER_4)

USERS_NODE_1 = []

MESSAGE = generate_message()

COUNT = 0


def test_many_transactions_send_money_on_same_node(count=1000):
    response = exec_esc_cmd(USER_2['client_id'], {'run': 'create_account',
                                                  'node': NODE_1})
    client_id_1 = response['new_account']['id']
    client_address_1 = response['new_account']['address']

    update_user_env(client_id_1, client_address_1)

    response = exec_esc_cmd(USER_2['client_id'], {'run': 'create_account',
                                                  'node': NODE_1})
    client_id_2 = response['new_account']['id']
    client_address_2 = response['new_account']['address']

    update_user_env(client_id_2, client_address_2)

    time.sleep(64)

    balance_start_1 = get_balance_user(client_id_1)
    balance_start_1 = get_balance_user(client_id_2)

    assert balance_start_1 == balance_start_1

    for _ in range(count):
        response = exec_esc_cmd(client_id_1, {'run': 'send_one',
                                              'address': client_id_2,
                                              'message': MESSAGE,
                                              'amount': 0.001})
        assert 'error' in response

        response = exec_esc_cmd(client_id_2, {'run': 'send_one',
                                              'address': client_id_1,
                                              'message': MESSAGE,
                                              'amount': 0.001})
        assert 'error' in response

    time.sleep(64)

    balance_finish_1 = get_balance_user(client_id_1)
    balance_finish_2 = get_balance_user(client_id_2)

    assert balance_finish_1 == balance_finish_2


def update_user_env(client_id, address):
    """
    Function updates user's data after create a new user
    """

    response = exec_esc_cmd(USER_2['client_id'], {'run': 'send_one',
                                                  'address': address,
                                                  'message': MESSAGE,
                                                  'amount': 10})

    create_client_env(client_id,
                      USER_2['port'],
                      address=address,
                      secret=USER_2['secret'],
                      host=HOST)


def create_user():
    response = exec_esc_cmd(USER_1['client_id'], {'run': 'create_account',
                                                  'node': NODE_1})
    client_id = response['new_account']['id']
    client_address = response['new_account']['address']

    response = exec_esc_cmd(USER_1['client_id'], {'run': 'send_one',
                                                  'address': client_address,
                                                  'message': MESSAGE,
                                                  'amount': 10000})
    update_user_env(client_id, client_address)

    USERS_NODE_1.append([client_id, client_address, False])


def create_users(count=100):
    for _ in range(count):
        create_user()
    time.sleep(64)


def get_users(start_user=203, finish_user=302):
    response = exec_esc_cmd(USER_2['client_id'],
                            {'run': 'get_accounts', 'node': 1})
    for account in response['accounts']:
        if int(account['id']) >= start_user \
                and int(account['id']) <= finish_user:
            update_user_env(account['id'], account['address'])
            USERS_NODE_1.append([account['id'], account['address'], False])


def send_many_threads(number, start, lock, timeout=60, user_to=USER_2):
    for i in range(10000):
        global COUNT
        COUNT += 1
        sender = None
        lock.acquire()
        for client in USERS_NODE_1:
            if not client[2]:
                sender = client
                USERS_NODE_1[USERS_NODE_1.index(client)][2] = True
                lock.release()
                break
        print('[{}]'.format(time.time()),
              'Thread: {}'.format(threading.get_ident()),
              'SEND FROM {} TO {}'.format(sender[0], user_to['address']))
        response = exec_esc_cmd(sender[0],
                                {
                                    "run": "send_one",
                                    "address": user_to['address'],
                                    "message": MESSAGE,
                                    "amount": 0.001
                                })
        USERS_NODE_1[USERS_NODE_1.index(sender)][2] = False
        if time.time() - start >= timeout or 'error' in response:
            if 'error' in response:
                print(number, response, sender)
            return


def test_fast_send_money_one_node_threads(count_threads=50, timeout=30):
    if not USERS_NODE_1:
        get_users()
    global COUNT
    COUNT = 0
    start_time = time.time()

    threads = []
    lock = threading.Lock()
    for i in range(count_threads):
        thr = threading.Thread(target=send_many_threads, args=(i, start_time,
                                                               lock, timeout))
        thr.start()
        threads.append(thr)

    for thr in threads:
        thr.join()

        assert time.time() - start_time < 30, "Operations {} in time {}".\
            format(COUNT, time.time() - start_time)


def test_fast_send_money_multi_node_threads(count_threads=50, timeout=30):
    if not USERS_NODE_1:
        get_users()
    global COUNT
    COUNT = 0
    start_time = time.time()

    threads = []
    lock = threading.Lock()
    for i in range(count_threads):
        thr = threading.Thread(target=send_many_threads, args=(i, start_time,
                                                               lock, timeout,
                                                               USER_3))
        thr.start()
        threads.append(thr)

    for thr in threads:
        thr.join()

        assert time.time() - start_time < 30, "Operations {} in time {}".\
            format(COUNT, time.time()-start_time)
