import time

from tests import logger
from tests.utils import (exec_esc_cmd, generate_keys, ValidateObject,
                         generate_message)
from tests.consts import (INIT_CLIENT_ID, INIT_NODE_ID, INIT_NODE_OFFICE_PORT)
from tests.client.utils import (create_init_client, create_client_env,
                                get_user_address, update_user_env,
                                get_time_block)

BLOCK_TIME = 0


def test_get_me(init_node_process, client_id='1'):
    create_init_client()
    global BLOCK_TIME
    BLOCK_TIME = get_time_block() * 2
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': "get_me"},
                            with_get_me=False)

    try:
        account = ValidateObject(response['account'], kind='account_init')
    except KeyError as err:
        raise KeyError(err, response)
    else:
        account.validate()

    assert response['network_account']['node'] == INIT_NODE_ID


def test_create_account(init_node_process, client_id="1"):
    # As INIT user, create client with client_id
    response = exec_esc_cmd(INIT_CLIENT_ID,
                            {"run": "create_account", "node": "0001"})
    try:
        address = response['new_account']['address']
    except KeyError as err:
        raise KeyError(err, response)

    message = generate_message()

    # without money can't change key. For transactions use another tests.
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "send_one",
                                             "address": address,
                                             'message': message,
                                             "amount": 20})

    try:
        account = ValidateObject(response['account'], kind='account_init')
    except KeyError as err:
        raise KeyError(err, response)
    else:
        account.validate()

    time_start = time.time()
    while True:
        response = exec_esc_cmd(INIT_CLIENT_ID, {'run': "get_accounts"},
                                with_get_me=False)
        accounts = len(response.get('accounts')) \
            if response.get('accounts') \
            else 0

        if accounts > 1:
            break

        assert time.time() - time_start < BLOCK_TIME
        time.sleep(10)

    # Change user keys
    new_secret, new_pub_key, signature = generate_keys()

    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "change_account_key",
                                             "pkey": new_pub_key,
                                             "signature": signature},
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

    create_client_env(client_id, INIT_NODE_OFFICE_PORT, address=address,
                      secret=new_secret)

    response = exec_esc_cmd(client_id, {'run': "get_me"}, with_get_me=False)

    try:
        assert response['account']['public_key'] == new_pub_key
    except KeyError as err:
        raise KeyError(err, response)


def test_get_accounts(init_node_process):
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_accounts', 'node': 1},
                            with_get_me=False)

    try:
        account = ValidateObject(response['accounts'][1])
    except KeyError as err:
        raise KeyError(err, response)
    else:
        account.validate()


def test_get_account(init_node_process, client_id="1"):
    address = get_user_address(client_id)

    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_account',
                                             'address': address})

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
    block = response['current_block_time']
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_account',
                                             'address': client_address})

    time_start = time.time()
    while True:
        if block != response['current_block_time']:
            break
        response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_account',
                                                 'address': client_address})
        time.sleep(10)
        assert time.time() - time_start < BLOCK_TIME

    try:
        account = ValidateObject(response['account'])
    except KeyError as err:
        raise KeyError(err, response)
    else:
        account.validate()

    assert status == response['account']['status']


def test_unset_account_status(init_node_process, client_id="1", status='8'):
    client_address = get_user_address(client_id)
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_account',
                                             'address': client_address})

    current_status = int(response['account']['status']) - int(status)

    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'unset_account_status',
                                             'address': client_address,
                                             'status': status})

    block = response['current_block_time']
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_account',
                                             'address': client_address})

    time_start = time.time()
    while True:
        if block != response['current_block_time']:
            break
        response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_account',
                                                 'address': client_address})
        time.sleep(10)
        assert time.time() - time_start < BLOCK_TIME

    try:
        account = ValidateObject(response['account'])
    except KeyError as err:
        raise KeyError(err, response)
    else:
        account.validate()

    assert str(current_status) == response['account']['status']
