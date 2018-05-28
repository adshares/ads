import time

from tests.consts import INIT_CLIENT_ID
from tests import utils as tests_utils

BLOCK_TIME = 0


def set_block_time():
    from tests.client.utils import get_time_block
    global BLOCK_TIME
    BLOCK_TIME = get_time_block() * 2


def test_send_again(init_node_process):
    response_1 = tests_utils.exec_esc_cmd(INIT_CLIENT_ID,
                                          {'run': 'get_accounts'},
                                          with_get_me=False)

    try:
        response_1['tx']['data']
    except KeyError as err:
        raise (err, KeyError)

    data = response_1['tx']['data']

    response_2 = tests_utils.exec_esc_cmd(INIT_CLIENT_ID,
                                          {'run': 'send_again',
                                           'data': data},  with_get_me=False)

    assert response_1 == response_2


def test_get_block(init_node_process):
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_block'})

    get_block = tests_utils.ValidateObject(response, kind='get_block')
    get_block.validate()

    try:
        block = tests_utils.ValidateObject(response['block'], kind='block')
    except KeyError as err:
        raise (err, response)
    else:
        block.validate()

    try:
        node = tests_utils.ValidateObject(response['block']['nodes'][0],
                                          kind='block_nodes')
    except KeyError as err:
        raise KeyError(err, response)
    else:
        node.validate()


def test_get_vipkeys(init_node_process):
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_block'})

    try:
        block = tests_utils.ValidateObject(response['block'], kind='block')
    except KeyError as err:
        raise (err, response)

    viphash = response['block']['viphash']

    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_vipkeys',
                                                         'viphash': viphash})

    fields = ['viphash', 'vipkeys']

    vipkyes = tests_utils.ValidateObject(response,
                                         kind='get_vipkyes',
                                         fields=fields)
    vipkyes.validate()


def _test_get_blocks(init_node_process):
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID,
                                        {
                                            "run": "get_blocks",
                                            "from": 0,
                                            "to": 0
                                        })
    assert not response


def test_send_and_get_broadcast(init_node_process):
    set_block_time()
    message = tests_utils.generate_message()
    # send a message
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID,
                                        {
                                            "run": "broadcast",
                                            "message": message
                                        })
    try:
        assert response['current_block_time']
    except KeyError as err:
        raise KeyError(err, response)

    current_time_hex = hex(int(response['current_block_time']))[2:]

    time_start = time.time()
    while True:
        response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID,
                                            {
                                                'run': 'get_broadcast',
                                                'from': current_time_hex
                                            })
        if response.get('broadcast'):
            break
        assert time.time() - time_start < BLOCK_TIME
        time.sleep(BLOCK_TIME)
    try:
        obj = tests_utils.ValidateObject(response['broadcast'][0],
                                         kind='broadcast')
    except KeyError as err:
        raise KeyError(err, response)

    else:
        obj.validate()

    assert response['broadcast'][0]['message'] == message


def test_get_transaction(init_node_process):
    from tests.client.utils import create_account
    create_account(5)
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID,
                                        {
                                            'run': 'get_log',
                                            'type': 'create_account'
                                        })

    try:
        log = tests_utils.ValidateObject(response['log'][0], kind='log')

    except KeyError as err:
        raise KeyError(err, response)
    else:
        log.validate()

    tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_blocks'})
    txid = response['log'][0]['id']
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID,
                                        {
                                            'run': 'get_transaction',
                                            'txid': txid
                                        })

    try:
        obj = tests_utils.ValidateObject(response['txn'],
                                         kind='get_transaction')
    except KeyError as err:
        raise KeyError(err, response)
    else:
        obj.validate()

    assert 'create_account' == response['txn']['type']


def test_get_signatures(init_node_process):
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID,
                                        {'run': 'get_signatures'})

    signatures_fields = ['signatures']

    signature_fields = ['node', 'signature']

    signatures = tests_utils.ValidateObject(response, kind='signatures',
                                            fields=signatures_fields)
    signatures.validate()

    try:
        signature = tests_utils.ValidateObject(response['signatures'][0],
                                               kind='signature',
                                               fields=signature_fields)

    except KeyError as err:
        raise KeyError(err, response)
    else:
        signature.validate()


def test_get_message_list(init_node_process):
    from tests.client.utils import create_account
    _, block_time = create_account(2, block_time=True)
    block_time_hex = hex(int(block_time)).split('x')[-1].upper()
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID,
                                        {'run': 'get_message_list',
                                         'block': block_time_hex})

    messages_fields = ['msghash', 'messages', 'confirmed']

    massage_fields = ['node', 'node_msid', 'hash']

    messages = tests_utils.ValidateObject(response,
                                          kind='messages',
                                          fields=messages_fields)
    messages.validate()

    message = tests_utils.ValidateObject(response['messages'][0],
                                         kind='messages',
                                         fields=massage_fields)

    message.validate()


def test_get_message(init_node_process):
    from tests.client.utils import create_account
    _, block_time = create_account(7, block_time=True)
    block_time_hex = hex(int(block_time)).split('x')[-1].upper()
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID,
                                        {'run': 'get_message_list',
                                         'block': block_time_hex})

    messages_fields = ['msghash', 'messages', 'confirmed']

    messages = tests_utils.ValidateObject(response,
                                          kind='messages',
                                          fields=messages_fields)
    messages.validate()

    node_msid = response['messages'][0]['node_msid']
    node_id = response['messages'][0]['node']
    h = response['messages'][0]['hash']

    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID,
                                        {'run': 'get_message',
                                         'node': node_id,
                                         'node_msid': node_msid,
                                         'block': block_time_hex},
                                        with_get_me=False)

    try:
        message = tests_utils.ValidateObject(response['transactions'][0],
                                             kind='get_message')
    except KeyError as err:
        raise KeyError(err, response)
    else:
        message.validate()

    assert response['hash'] == h


def test_retrieve_funds():
    from tests.node.utils import create_node_without_start

    local_account = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_me'})

    try:
        account = tests_utils.ValidateObject(local_account['account'],
                                             kind='account_init')
    except KeyError as err:
        raise KeyError(err, local_account)
    else:
        account.validate()

    local_address = local_account['account']['address']

    create_node_without_start()

    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID,
                                        {
                                            'run': 'get_accounts',
                                            'node': '2'
                                        })

    address = response['accounts'][0]['address']

    start_time = time.time()
    while True:
        response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID,
                                            {
                                                'run': 'retrieve_funds',
                                                'address': address}
                                            )
        if response.get('account'):
            break
        assert time.time() - start_time < BLOCK_TIME
        time.sleep(BLOCK_TIME)

    try:
        account = tests_utils.ValidateObject(response['account'],
                                             kind='account_init')
    except KeyError as err:
        raise KeyError(err, response)
    else:
        account.validate()

    assert response['account']['address'] == local_address
