import time

from .conftest import exec_esc_cmd, create_client_env, create_init_client, manual_init_node_process
from .conftest import INIT_CLIENT_ID, INIT_NODE_ID, INIT_NODE_OFFICE_PORT
from .utils import create_node, update_user_env


def test_get_me(init_node_process):
    create_init_client()
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': "get_me"}, with_get_me=False)
    assert response['network_account']['node'] == INIT_NODE_ID


def test_key_changed(init_node_process):
    new_public_key = 'D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17'
    new_secret_key = 'FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6'
    signature = '7A1CA8AF3246222C2E06D2ADE525A693FD81A2683B8A8788C32B7763DF6037A5DF3105B92FEF398AF1CDE0B92F18FE68DEF301E4BF7DB0ABC0AEA6BE24969006'

    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': "get_me"}, with_get_me=False)
    address = response['account']['address']

    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "change_account_key",
                                             "pkey": new_public_key,
                                             "signature": signature})
    assert response['result'] == 'PKEY changed'

    create_client_env(INIT_CLIENT_ID, INIT_NODE_OFFICE_PORT, address=address, secret=new_secret_key)

    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': "get_me"}, with_get_me=False)

    assert response['account']['public_key'] == new_public_key


def test_create_account(init_node_process, client_id="2"):
    # As INIT user, create client with client_id
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "create_account", "node": "0001"})
    assert 'new_account' in response
    assert 'address' in response['new_account']
    address = response['new_account']['address']

    time_start = time.time()
    while True:
        time.sleep(10)
        response = exec_esc_cmd(INIT_CLIENT_ID, {'run': "get_accounts"}, with_get_me=False)
        accounts = len(response.get('accounts')) if response.get('accounts') else 0
        if accounts > 1:
            break
        assert time.time() - time_start < 70

    message = "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"

    # without money can't change key. For transactions use another tests.
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "send_one",
                                             "address": address,
                                             'message': message,
                                             "amount": 20})

    # Change user keys
    new_pub_key = 'C9965A1417F52B22514559B7608E4E2C1238FCA3602382C535D42D1759A2F196'
    new_secret = '5BF11F5D0130EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C'
    signature = 'ED8479C0EDA3BB02B5B355E05F66F8161811F5AD9AE9473AA91E2DA32457EAB850BC6A04D6D4D5DDFAB4B192D2516D266A38CEA4251B16ABA1DF1B91558A4A05'

    response = exec_esc_cmd(INIT_CLIENT_ID, {"run":"change_account_key", "pkey":new_pub_key, "signature": signature},
                            cmd_extra=['--address', address])

    create_client_env(client_id, INIT_NODE_OFFICE_PORT,
                      address=address,
                      secret=new_secret)

    assert response['result'] == 'PKEY changed'

    response = exec_esc_cmd(client_id, {'run': 'get_me'}, with_get_me=False)

    assert response['account']['address'] == address


def test_get_accounts(init_node_process):
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_accounts', 'node': 1}, with_get_me=False)
    assert len(response['accounts']) == 2





# def test_get_account(init_node_process, client_id="2"):
#     response = exec_esc_cmd(client_id, {'run': 'get_me'}, with_get_me=False)
#     address = response['account']['address']
#
#     response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_account', 'address': address})
#     import pdb
#     pdb.set_trace()
#     assert response['account']['address'] == address
#     assert response['account']['id'] == client_id


# def test_create_account_on_another_node(init_node_process, client_id="3"):
#     create_node("2", INIT_CLIENT_ID, port=8020, offi=8021)
#     accounts = len(exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_accounts', 'node': '2'}, with_get_me=False)['accounts'])
#     response_create = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'create_account', 'node': '0002'})
#
#     time_start = time.time()
#     while True:
#         response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_accounts', 'node': '2'}, with_get_me=False)
#         response_log = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_log'})
#         import pdb
#         pdb.set_trace()
#         current_accounts = len(response['accounts'])
#         if current_accounts > accounts:
#             print(accounts)
#         time.sleep(5)
#         # assert time.time() - time_start < 70
#
#     update_user_env(client_id, address)



