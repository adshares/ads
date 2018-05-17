import time
from .utils import create_client_env, exec_esc_cmd, get_user_address, get_balance_user

HOST = '85.10.197.15'
PORT = 7191

USER_1 = {'client_id': 1,
          'address': '0001-00000000-9B6F',
          'host': HOST,
          'port': 7191,
          'secret': 'FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6'}
USER_2 = {'client_id': 2,
          'address': '0001-00000001-8B4E',
          'host': HOST,
          'port': 7191,
          'secret': '5BF11F5D0130EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C'}
USER_3 = {'client_id': 3,
          'address': '0002-00000000-75BD',
          'host': HOST,
          'port': 7192,
          'secret': 'FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6',
          }
USER_4 = {'client_id': 4,
          'address': '0002-00000001-659C',
          'host': HOST,
          'port': 7192,
          'secret': 'FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6'
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

MESSAGE = "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"


def collect_all_users_node_1():
    response = exec_esc_cmd(USER_1['client_id'], {'run': 'get_accounts'}, with_get_me=False)
    for account in response['accounts']:
        USERS_NODE_1.append({'client_id': account['id'], 'address': account['address']})
        create_client_env(account['id'], address=account['address'], secret=SECRET, host=HOST, port=7191)


collect_all_users_node_1()


def test_create_new_user():
    response = exec_esc_cmd(USER_1['client_id'], {'run': 'create_account'})

    assert 'new_account' in response

    address = response['new_account']['address']
    client_id = response['new_account']['id']

    response = exec_esc_cmd(USER_1['client_id'], {'run': 'send_one',
                                                  "address": address,
                                                  'message': MESSAGE,
                                                  "amount": 20})
    # Change user keys
    done = False
    time_start = time.time()
    while True:
        accounts = exec_esc_cmd(USER_1['client_id'], {'run': 'get_accounts'}, with_get_me=False)
        if 'accounts' in accounts:
            for account in accounts['accounts']:
                if account['address'] == address:
                    done = True
        if done:
            break
        time.sleep(10)
    assert time.time() - 60 < time_start


def test_send_money():
    start_bablance = get_balance_user(USER_1['client_id'])
    amount = 10
    _set = {}

    for user in USERS_NODE_1:
        balance = get_balance_user(user['client_id'])
        _set.update({
            user['client_id']: { 'start_balance': balance}
        })

    for user in USERS_NODE_1:
        balance = get_balance_user(user['client_id'])
        _set.update({
            user['client_id']: {'start_balance': balance}
        })

    for user in USERS_NODE_1:
        response = exec_esc_cmd(USER_1['client_id'], {'run': 'send_one',
                                                      'address': user['address'],
                                                      'message': MESSAGE,
                                                      'amount': amount})

    for user in USERS_NODE_1:
        balance = get_balance_user(user['client_id'])
        _set.get(user['client_id']).update({'finish_balance': balance})

    for user in _set:
        assert float(user['start_balance']) + amount == user['finish_balance']