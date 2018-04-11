import time

from . import INIT_CLIENT_ID, INIT_NODE_OFFICE_PORT, create_client_env, exec_esc_cmd


def update_user_env(client_id, address):
    """
    Function updates user's data after create a new user
    """
    message = "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"

    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "send_one",
                                             "address": address,
                                             'message': message,
                                             "amount": 20})

    new_pub_key = 'C9965A1417F52B22514559B7608E4E2C1238FCA3602382C535D42D1759A2F196'
    new_secret = '5BF11F5D0130EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C'
    signature = 'ED8479C0EDA3BB02B5B355E05F66F8161811F5AD9AE9473AA91E2DA32457EAB850BC6A04D6D4D5DDFAB4B192D2516D266A38CEA4251B16ABA1DF1B91558A4A05'

    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "change_account_key", "pkey": new_pub_key, "signature": signature},
                            cmd_extra=['--address', address])

    create_client_env(client_id, INIT_NODE_OFFICE_PORT,
                      address=address,
                      secret=new_secret)


def create_account(client_id="2", node="0001"):
    # As INIT user, create client with client_id
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "create_account", "node": node})
    address = response['new_account']['address']

    accounts = 0
    time_start = time.time()
    while accounts <= 1:
        response = exec_esc_cmd(INIT_CLIENT_ID, {'run': "get_accounts"}, with_get_me=False)
        accounts = len(response.get('accounts')) if response.get('accounts') else 0
        time.sleep(10)
        assert time.time() - time_start < 70

    update_user_env(client_id, address)

    return address


def get_user_address(client_id):
    response = exec_esc_cmd(client_id, {"run": "get_me"}, with_get_me=False)
    return response['account']['address']
