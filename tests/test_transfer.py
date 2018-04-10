from . import exec_esc_cmd, create_client_env, create_init_client
from . import INIT_CLIENT_ID, INIT_NODE_OFFICE_PORT, INIT_NODE_ID


def create_account(client_id="2", node="0001"):
    # As INIT user, create client with client_id
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "create_account", "node": node})
    address = response['new_account']['address']

    import time
    accounts = 0
    while accounts <= 1:
        response = exec_esc_cmd(INIT_CLIENT_ID, {'run': "get_accounts"}, with_get_me=False)
        accounts = len(response.get('accounts')) if response.get('accounts') else 0
        time.sleep(5)

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

    return address


def test_send_cash_one_user(init_node_process, amount=100.0, client_id='2'):
    address_receiver = create_account()
    response_receiver = exec_esc_cmd(client_id,
                            {'run': "get_me"},
                            with_get_me=False)

    start_balance_receiver = response_receiver['account']['balance']

    message = "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"

    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "send_one",
                                             "address": address_receiver,
                                             'message': message,
                                             "amount": amount})

    assert float(response['tx']['deduct']) == float(amount)

    difference_receiver = float(start_balance_receiver) + float(amount)

    response_receiver = exec_esc_cmd(client_id, {"run": "get_me"}, with_get_me=False)

    assert difference_receiver == float(response_receiver['account']['balance'])


def test_send_cash_many_users(init_node_process, amount_1=35.0, amount_2=10.0):
    address_receivers_2 = create_account(client_id="3")

    response_receiver_2 = exec_esc_cmd("3",
                                     {'run': "get_me"},
                                     with_get_me=False)

    response_receiver_1 = exec_esc_cmd("2",
                                     {'run': "get_me"},
                                     with_get_me=False)

    start_balance_receiver_1 = response_receiver_1['account']['balance']
    start_balance_receiver_2 = response_receiver_2['account']['balance']

    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "send_many", "wires":{address_receivers_2: amount_2,
                                                                          response_receiver_1['account']['address']: amount_1}})

    assert float(response['tx']['deduct']) == amount_1 + amount_2

    difference_receiver_1 = float(start_balance_receiver_1) + float(amount_1)
    response_receiver_1 = exec_esc_cmd("2", {"run": "get_me"}, with_get_me=False)

    response_receiver_2 = exec_esc_cmd("3", {"run": "get_me"}, with_get_me=False)
    difference_receiver_2 = float(start_balance_receiver_2) + float(amount_2)

    assert difference_receiver_1 == float(response_receiver_1['account']['balance'])
    assert difference_receiver_2 == float(response_receiver_2['account']['balance'])


