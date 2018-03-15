from . import exec_esc_cmd, create_client_env
from . import INIT_CLIENT_ID, INIT_NODE_ID, INIT_NODE_OFFICE_PORT

import json
import pytest


DEFAULT_ADDRESS = "0001-00000000-XXXX"
DEFAULT_SECRET = "14B183205CA661F589AD83809952A692DFA48F5D490B10FD120DA7BF10F2F4A0"


def test_get_me(init_node_process):
    create_client_env(INIT_CLIENT_ID, INIT_NODE_OFFICE_PORT,
                      address=DEFAULT_ADDRESS,
                      secret=DEFAULT_SECRET)

    response = json.loads(exec_esc_cmd(INIT_CLIENT_ID, {'run':"get_me"}))
    assert response['network_account']['node'] == INIT_NODE_ID


def test_key_changed(init_node_process):
    new_public_key = 'D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17'
    new_secret_key = 'FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6'
    signature = '7A1CA8AF3246222C2E06D2ADE525A693FD81A2683B8A8788C32B7763DF6037A5DF3105B92FEF398AF1CDE0B92F18FE68DEF301E4BF7DB0ABC0AEA6BE24969006'

    responses = exec_esc_cmd(INIT_CLIENT_ID, [
            {'run': "get_me"},
            {"run":"change_account_key", "pkey":new_public_key, "signature":signature}
        ])

    get_me_response, change_key_response = json.loads(responses[0]), responses[1]
    assert change_key_response == 'PKEY changed2\n'

    # Updating clinet settings
    create_client_env(INIT_CLIENT_ID, INIT_NODE_OFFICE_PORT,
                      address=get_me_response['account']['address'],
                      secret=new_secret_key)

    response = json.loads(exec_esc_cmd(INIT_CLIENT_ID, {'run':"get_me"}))
    assert response['account']['public_key'] == new_public_key


def test_create_account(client_id="2"):
    # As INIT user, create client with client_id
    responses = exec_esc_cmd(INIT_CLIENT_ID, [
            {'run': "get_me"},
            {"run":"create_account", "node":INIT_NODE_ID}
        ])

    get_me_response, create_account_response = json.loads(responses[0]), json.loads(responses[1])

    assert 'account' in create_account_response
    assert 'address' in create_account_response['account']

    address = create_account_response['account']['address']
    get_account_response = json.loads(exec_esc_cmd(INIT_NODE_ID, {'run':"get_account", "address":address}))

    # Change user keys
    new_pub_key = 'C9965A1417F52B22514559B7608E4E2C1238FCA3602382C535D42D1759A2F196'
    new_secret = '5BF11F5D0130EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C'
    signature = 'ED8479C0EDA3BB02B5B355E05F66F8161811F5AD9AE9473AA91E2DA32457EAB850BC6A04D6D4D5DDFAB4B192D2516D266A38CEA4251B16ABA1DF1B91558A4A05'

    responses = exec_esc_cmd(INIT_CLIENT_ID, [
            {'run': "get_me"},
            {"run":"change_account_key", "pkey":new_pub_key, "signature":signature}
        ])

    get_me_response, change_key_response = json.loads(responses[0]), responses[1]
    assert change_key_response == 'PKEY changed2\n'

    create_client_env(client_id, INIT_NODE_OFFICE_PORT,
                      address=address,
                      secret=new_secret)


def test_change_key():
    pass
