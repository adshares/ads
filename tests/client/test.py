import time

from tests.utils import exec_esc_cmd, generate_keys, ValidateObject
from tests.consts import INIT_CLIENT_ID, INIT_NODE_ID, INIT_NODE_OFFICE_PORT
from tests.client.utils import (create_init_client,create_client_env,
                                get_user_address, update_user_env, create_account)


def test_get_me(init_node_process, client_id='1'):
    create_init_client()
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': "get_me"}, with_get_me=False)

    try:
        account = ValidateObject(response['account_init'])
    except KeyError as err:
        raise KeyError(err, response)
    else:
        account.validate()

    assert response['network_account']['node'] == INIT_NODE_ID


def test_create_account(init_node_process, client_id="1"):
    # As INIT user, create client with client_id
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "create_account", "node": "0001"})
    try:
        address = response['new_account']['address']
    except KeyError as err:
        raise KeyError(err, response)

    message = "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"

    # without money can't change key. For transactions use another tests.
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "send_one",
                                             "address": address,
                                             'message': message,
                                             "amount": 20})

    time_start = time.time()
    while True:
        time.sleep(10)
        response = exec_esc_cmd(INIT_CLIENT_ID, {'run': "get_accounts"}, with_get_me=False)
        accounts = len(response.get('accounts')) if response.get('accounts') else 0
        if accounts > 1:
            break
        assert time.time() - time_start < 70

    # Change user keys
    new_secret, new_pub_key, signature = generate_keys()

    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "change_account_key",
                                             "pkey": new_pub_key, "signature": signature},
                            cmd_extra=['--address', address])

    create_client_env(client_id, INIT_NODE_OFFICE_PORT,
                      address=address,
                      secret=new_secret)

    try:
        assert response['result'] == 'PKEY changed'
    except KeyError as err:
        raise KeyError(err, response)

    response = exec_esc_cmd(client_id, {'run': 'get_me'}, with_get_me=False)

    try:
        assert response['account']['address'] == address
    except KeyError as err:
        raise KeyError(err, response)


def test_key_changed(init_node_process, client_id="1"):
    new_secret, new_pub_key, signature = generate_keys()

    response = exec_esc_cmd(client_id, {'run': "get_me"}, with_get_me=False)

    try:
        account = ValidateObject(response['account'], kind='account')
    except KeyError as err:
        raise KeyError(err, response)
    else:
        account.validate()

    address = response['account']['address']
    balance = response['account']['balance']

    if float(balance) < 1:
        raise ValueError("Too low balance {} for change PKEY".format(balance))

    response = exec_esc_cmd(client_id, {"run": "change_account_key",
                                             "pkey": new_pub_key,
                                             "signature": signature})

    try:
        assert response['result'] == 'PKEY changed'
    except KeyError as err:
        raise KeyError(err, response)

    create_client_env(client_id, INIT_NODE_OFFICE_PORT, address=address, secret=new_secret)

    response = exec_esc_cmd(client_id, {'run': "get_me"}, with_get_me=False)

    try:
        assert response['account']['public_key'] == new_pub_key
    except KeyError as err:
        raise KeyError(err, response)


def test_get_accounts(init_node_process):
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_accounts', 'node': 1}, with_get_me=False)

    try:
        account = ValidateObject(response['accounts'][0])
    except KeyError as err:
        raise KeyError(err, response)
    else:
        account.validate()


def test_get_account(init_node_process, client_id="1"):
    address = get_user_address(client_id)

    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_account', 'address': address})

    try:
        account = ValidateObject(response['account'])
    except KeyError as err:
        raise KeyError(err, response)
    else:
        account.validate()

    assert response['account']['address'] == address


def test_set_account_status(init_node_process, client_id="1", status='10'):
    client_address = get_user_address(client_id)

    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'set_account_status',
                                             'address': client_address,
                                             'status': status})

    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_account', 'address': client_address})

    try:
        account = ValidateObject(response['account'])
    except KeyError as err:
        raise KeyError(err, response)
    else:
        account.validate()

    assert status == response['account']['status']


def test_unset_account_status(init_node_process, client_id="1", status='8'):
    client_address = get_user_address(client_id)

    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'unset_account_status',
                                             'address': client_address,
                                             'status': status})

    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_account', 'address': client_address})

    try:
        account = ValidateObject(response['account'])
    except KeyError as err:
        raise KeyError(err, response)
    else:
        account.validate()

    assert status == response['account']['status']


def test_create_account_on_another_node(init_node_process, client_id="3"):
    from tests.node.utils import create_node_and_run
    create_node_and_run("2", INIT_CLIENT_ID, port=8020, offi=8021)

    accounts = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_accounts', 'node': '2'}, with_get_me=False)

    assert 'accounts' in accounts

    response_create = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'create_account', 'node': '0002'})

    try:
        local_account = ValidateObject(response_create['account'])
    except KeyError as err:
        raise KeyError(err, response_create)
    else:
        local_account.validate()

    time_start = time.time()
    address = None

    while True:
        response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_accounts', 'node': '2'}, with_get_me=False)
        if 'accounts' in response:
            current_accounts = len(response['accounts'])
            if current_accounts > len(accounts):
                for user in response['accounts']:
                    if user['id'] != '0':
                        address = user['address']
                        break
        time.sleep(5)
        assert time.time() - time_start < 70

        update_user_env(client_id, address)



