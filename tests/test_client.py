from . import exec_esc_cmd, create_client_env, create_init_client
from . import INIT_CLIENT_ID, INIT_NODE_ID, INIT_NODE_OFFICE_PORT


def test_get_me(init_node_process):
    create_init_client()
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run':"get_me"}, with_get_me=False)
    assert response['network_account']['node'] == INIT_NODE_ID


def test_key_changed(init_node_process):
    new_public_key = 'D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17'
    new_secret_key = 'FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6'
    signature = '7A1CA8AF3246222C2E06D2ADE525A693FD81A2683B8A8788C32B7763DF6037A5DF3105B92FEF398AF1CDE0B92F18FE68DEF301E4BF7DB0ABC0AEA6BE24969006'

    response = exec_esc_cmd(INIT_CLIENT_ID, {'run':"get_me"}, with_get_me=False)
    address = response['account']['address']

    response = exec_esc_cmd(INIT_CLIENT_ID,
                            {"run":"change_account_key", "pkey":new_public_key, "signature":signature},
                            loads_json=False)
    assert response == 'PKEY changed2\n'

    create_client_env(INIT_CLIENT_ID, INIT_NODE_OFFICE_PORT,
                      address=address,
                      secret=new_secret_key)

    response = exec_esc_cmd(INIT_CLIENT_ID, {'run':"get_me"}, with_get_me=False)
    assert response['account']['public_key'] == new_public_key


def test_create_account(client_id="2"):
    # As INIT user, create client with client_id
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run":"create_account", "node":INIT_NODE_ID})

    assert 'account' in response
    assert 'address' in response['account']

    address = response['account']['address']
    response = exec_esc_cmd(INIT_NODE_ID, {'run':"get_account", "address":address}, with_get_me=False)

    # Change user keys
    new_pub_key = 'C9965A1417F52B22514559B7608E4E2C1238FCA3602382C535D42D1759A2F196'
    new_secret = '5BF11F5D0130EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C'
    signature = 'ED8479C0EDA3BB02B5B355E05F66F8161811F5AD9AE9473AA91E2DA32457EAB850BC6A04D6D4D5DDFAB4B192D2516D266A38CEA4251B16ABA1DF1B91558A4A05'

    response = exec_esc_cmd(INIT_CLIENT_ID,
                            {"run":"change_account_key", "pkey":new_pub_key, "signature":signature},
                            cmd_extra=['--address', address], loads_json=False)

    create_client_env(client_id, INIT_NODE_OFFICE_PORT,
                      address=address,
                      secret=new_secret)

    assert response == 'PKEY changed2\n'


