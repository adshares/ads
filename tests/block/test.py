import time

from tests.consts import INIT_CLIENT_ID
from tests import utils as tests_utils


def test_get_blocks(init_node_process):
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_blocks', 'from': 0, "to": 0})
    try:
        assert response == None
    except Exception as err:
        raise Exception(err, response)


def test_send_and_get_broadcast(init_node_process):
    message = "D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17"

    # send a message
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {"run": "broadcast",
                                                         "message": message})
    try:
        assert response['current_block_time']
    except KeyError as err:
        raise KeyError(err, response)

    current_time_hex = hex(int(response['current_block_time']))[2:]

    time_start = time.time()
    while True:
        response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_broadcast', 'from': current_time_hex})
        if response.get('broadcast'):
            break
        time.sleep(5)
        assert time.time() - time_start < 30
    try:
        obj = tests_utils.ValidateObject(response['broadcast'][0], kind='broadcast')
    except KeyError as err:
        raise KeyError(err, response)
    obj.validate()

    assert response['broadcast'][0]['message'] == message


def test_get_transaction(init_node_process):
    from tests.client.utils import create_account
    create_account(5)
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_log', 'type': 'create_account'})

    try:
        log = tests_utils.ValidateObject(response['log'], kind='log')
    except KeyError as err:
        raise KeyError(err, response)
    else:
        log.validate()

    tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_blocks'})
    txid = response['log'][0]['id']
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_transaction', 'txid': txid})

    try:
        obj = tests_utils.ValidateObject(response['network_tx'], 'transaction')
    except KeyError as err:
        raise KeyError(err, response)
    else:
        obj.validate()

    assert 'create_account' == response['network_tx']['type']


def test_get_signatures(init_node_process):
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_signatures'})

    signatures_fields = ['signatures', 'fork_signatures']

    signature_fields = ['node', 'signatures']

    signatures = tests_utils.ValidateObject(response, kind='signatures', fields=signatures_fields)
    signatures.validate()

    try:
        signature = tests_utils.ValidateObject(response['signatures'], kind='signature', fields=signature_fields)
    except KeyError as err:
        raise KeyError(err, response)
    else:
        signature.validate()


def test_get_message_list(init_node_process):
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_message_list'})

    message_fields = [
        'node', 'node_msid', 'hash'
    ]

    messages_fields = [
        'msghash', 'messages', 'confirmed'
    ]

    messages = tests_utils.ValidateObject(response, kind='messages', fields=messages_fields)
    messages.validate()

    message = tests_utils.ValidateObject(response['messages'][0], kind='message', fields=message_fields)
    message.validate()


def test_get_message(init_node_process):
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_message_list'})
    node_msid = response['messages'][0]['node_msid']
    node_id = response['messages'][0]['node']
    h = response['messages'][0]['hash']

    message_fields = ['node', 'node_msid', 'time', 'langth', 'hash', 'network_txs']

    network_tx_fields = ['id', 'type', 'abank', 'auser', 'amsid', 'ttime',
                         'bbank', 'buser', 'amount', 'message', 'signature', 'size']

    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID,
                                        {'run': 'get_message',
                                         'node': node_id,
                                         'node_msid': node_msid},
                                        with_get_me=False)

    message = tests_utils.ValidateObject(response, kind='message', fields=message_fields)
    message.validate()

    try:
        network_tx = tests_utils.ValidateObject(response['network_txs'],
                                                kind='network_tx', fields=network_tx_fields)
    except KeyError as err:
        raise KeyError(err, response)
    else:
        network_tx.validate()

    assert response['hash'] == h


def test_retrieve_funds():
    from tests.node.utils import create_node_without_start

    local_account = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_me'})

    try:
        account = tests_utils.ValidateObject(local_account['account'])
    except KeyError as err:
        raise KeyError(err, local_account)
    else:
        account.validate()

    local_address = local_account['account']['address']

    create_node_without_start()

    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_accounts', 'node': '2'})
    address = response['accounts'][0]['address']

    start_time = time.time()
    while True:
        response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'retrieve_funds', 'address': address})
        if response.get('account'):
            break
        assert time.time() - start_time < 20

    try:
        account = tests_utils.ValidateObject(response['account'])
    except KeyError as err:
        raise KeyError(err, response)
    else:
        account.validate()

    assert response['account']['address'] == local_address