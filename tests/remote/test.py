import random
import time
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
        accounts = exec_esc_cmd(USER_1['client_id'], {'run': 'get_accounts'},
                                with_get_me=False)
        if 'accounts' in accounts:
            for account in accounts['accounts']:
                if account['address'] == address:
                    done = True
        if done:
            break
        time.sleep(10)
    assert time.time() - 60 < time_start


def test_send_money(count=100):
    start_balance_1 = get_balance_user(USER_1['client_id'])
    start_balance_2 = get_balance_user(USER_2['client_id'])
    amount = 10

    sent_user_1 = []
    sent_user_2 = []

    for _ in range(count):
        amount_1 = random.randint(1, amount)
        amount_2 = random.randint(1, amount)


        response = exec_esc_cmd(USER_1['client_id'],
                                {
                                    "run": "send_one",
                                    "address": USER_2['address'],
                                    "message": MESSAGE,
                                    "amount": amount_1
                                })
        sent_user_1.append(amount_1)

        response = exec_esc_cmd(USER_2['client_id'],
                                {
                                    "run": "send_one",
                                    "address": USER_1["address"],
                                    "message": MESSAGE,
                                    "amount": amount_2
                                })

        sent_user_2.append(amount_2)

    time.sleep(60)

    finish_balance_1 = get_balance_user(USER_1["client_id"])
    finish_balance_2 = get_balance_user(USER_2["client_id"])

    assert (float(finish_balance_1) ==
            float(start_balance_1) - sum(sent_user_1) + sum(sent_user_2))

    assert (float(finish_balance_2) ==
            float(start_balance_2) - sum(sent_user_2) + sum(sent_user_1))
