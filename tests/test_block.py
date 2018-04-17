import time

from . import exec_esc_cmd
from . import INIT_CLIENT_ID
from .utils import create_node_without_start


def test_get_blocks(init_node_process):
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_blocks', 'from': 0, "to": 0})
    assert len(response['blocks']) > 0


def test_send_and_get_broadcast(init_node_process):
    message = "D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17"

    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "broadcast",
                                             "message": message})

    assert response['current_block_time']

    current_time_hex = hex(int(response['current_block_time']))[2:]

    time_start = time.time()
    while True:
        response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_broadcast', 'from': current_time_hex})
        if response.get('broadcast'):
            break
        time.sleep(5)
        assert time.time() - time_start < 30

    assert response['broadcast'][0]['message'] == message


def test_get_transaction(init_node_process):
    pass


def test_get_signatures(init_node_process):
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_signatures'})
    assert len(response['signatures']) > 0


def test_get_message_list(init_node_process):
    response =exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_message_list'})
    assert len(response['messages']) > 0


def test_get_message(init_node_process):
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_message_list'})
    block_time = response['block_time']
    node_msid = response['messages'][0]['node_msid']
    node_id = response['messages'][0]['node_id']
    h = response['messages'][0]['hash']

    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_message', 'node': node_id, 'node_msid': node_msid},
                            with_get_me=False)

    assert response['hash'] == h


def test_retrieve_funds():
    local_account = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_me'})['account']['address']
    create_node_without_start()
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_accounts', 'node': '2'})
    address = response['accounts'][0]['address']

    start_time = time.time()
    while True:
        response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'retrieve_funds', 'address': address})
        if response.get('account'):
            break
        assert time.time() - start_time < 20

    assert response['account']['address'] == local_account
