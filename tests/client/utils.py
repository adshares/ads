import time

from ..consts import INIT_CLIENT_ID, INIT_NODE_OFFICE_PORT
from .. import utils as tests_utils


def get_user_address(client_id):
    response = tests_utils.exec_esc_cmd(client_id, {"run": "get_me"}, with_get_me=False)
    try:
        return response['account']['address']
    except KeyError:
        raise Exception(response)


def get_balance_user(client_id):
    """
    Function returns user's balance in str format
    """
    response_receiver = tests_utils.exec_esc_cmd(client_id, {"run": "get_me"}, with_get_me=False)
    try:
        return response_receiver['account']['balance']
    except KeyError:
        raise Exception(response_receiver)


def update_user_env(client_id, address):
    """
    Function updates user's data after create a new user
    """
    message = "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {"run": "send_one",
                                             "address": address,
                                             'message': message,
                                             "amount": 20})

    new_pub_key = 'C9965A1417F52B22514559B7608E4E2C1238FCA3602382C535D42D1759A2F196'
    new_secret = '5BF11F5D0130EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C'
    signature = 'ED8479C0EDA3BB02B5B355E05F66F8161811F5AD9AE9473AA91E2DA32457EAB850BC6A04D6D4D5DDFAB4B192D2516D266A38CEA4251B16ABA1DF1B91558A4A05'

    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {
        "run": "change_account_key",
        "pkey": new_pub_key, "signature": signature},
        cmd_extra=['--address', address])

    tests_utils.create_client_env(client_id,
                                  INIT_NODE_OFFICE_PORT,
                                  address=address,
                                  secret=new_secret)


def create_account(client_id="2", node="0001"):
    # As INIT user, create client with client_id
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {"run": "create_account", "node": node})
    address = response['new_account']['address']

    time_start = time.time()
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': "get_accounts",  "node": node}, with_get_me=False)
    count_users = len(response.get('accounts'))

    while True:
        response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': "get_accounts", "node": node}, with_get_me=False)
        accounts = len(response.get('accounts')) if response.get('accounts') else 0
        if accounts > count_users:
            break
        time.sleep(10)
        assert time.time() - time_start < 70

    update_user_env(client_id, address)

    return address