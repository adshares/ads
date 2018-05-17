import time

from ..consts import INIT_CLIENT_ID
from .. import utils as tests_utils
from ..node import utils as node_utils
from ..client import utils as client_utils


def test_get_blocks(init_node_process):
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_blocks', 'from': 0, "to": 0})
    assert len(response['blocks']) > 0


def test_send_and_get_broadcast(init_node_process):
    message = "D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17"

    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {"run": "broadcast",
                                                         "message": message})
    try:
        assert response['current_block_time']
    except KeyError:
        raise Exception(response)

    current_time_hex = hex(int(response['current_block_time']))[2:]

    time_start = time.time()
    while True:
        response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_broadcast', 'from': current_time_hex})
        if response.get('broadcast'):
            break
        time.sleep(5)
        assert time.time() - time_start < 30

    try:
        assert response['broadcast'][0]['message'] == message
    except KeyError:
        raise Exception(response)


def test_get_transaction(init_node_process):
    client_utils.create_account(5)
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_log', 'type': 'create_account'})
    tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_blocks'})
    txid = response['log'][0]['id']
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_transaction', 'txid': txid})
    try:
        assert 'create_account' == response['txn']['type']
    except KeyError:
        raise Exception(response)


def test_get_signatures(init_node_process):
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_signatures'})
    try:
        assert len(response['signatures']) > 0
    except KeyError:
        raise Exception(response)


def test_get_message_list(init_node_process):
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_message_list'})
    try:
        assert len(response['messages']) > 0
    except KeyError:
        raise Exception(response)


def test_get_message(init_node_process):
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_message_list'})
    node_msid = response['messages'][0]['node_msid']
    node_id = response['messages'][0]['node_id']
    h = response['messages'][0]['hash']

    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID,
                                        {'run': 'get_message',
                                         'node': node_id,
                                         'node_msid': node_msid},
                                        with_get_me=False)

    try:
        assert response['hash'] == h
    except KeyError:
        raise Exception(response)


def test_retrieve_funds():
    local_account = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_me'})['account']['address']
    node_utils.create_node_without_start()
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_accounts', 'node': '2'})
    address = response['accounts'][0]['address']

    start_time = time.time()
    while True:
        response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'retrieve_funds', 'address': address})
        if response.get('account'):
            break
        assert time.time() - start_time < 20

    try:
        assert response['account']['address'] == local_account
    except KeyError:
        raise Exception(response)