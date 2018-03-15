from . import exec_esc_cmd, create_client_env
from . import INIT_CLIENT_ID, INIT_NODE_ID, INIT_NODE_OFFICE_PORT

import json
import pytest


DEFAULT_ADDRESS = "0001-00000000-XXXX"
DEFAULT_SECRET = "14B183205CA661F589AD83809952A692DFA48F5D490B10FD120DA7BF10F2F4A0"


def test_init_client_get_me(init_node_process):
    create_client_env(INIT_CLIENT_ID, INIT_NODE_OFFICE_PORT,
                      address=DEFAULT_ADDRESS,
                      secret=DEFAULT_SECRET)

    response = json.loads(exec_esc_cmd(INIT_CLIENT_ID, {'run':"get_me"}))
    assert response['network_account']['node'] == INIT_NODE_ID


def test_init_client_key_changed(init_node_process):
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


def test_create_account():
    responses = exec_esc_cmd(INIT_CLIENT_ID, [
            {'run': "get_me"},
            {"run":"create_account", "node":"0001"}
        ])

    get_me_response, create_account_response = json.loads(responses[0]), json.loads(responses[1])

    assert 'account' in create_account_response
    assert 'address' in create_account_response['account']

    address = create_account_response['account']['address']

    print("$$$$$"*100, address)
    #echo '{"run":"get_account","address":"0001-00000001-8B4E"}' | esc


def test_change_key():
    pass
