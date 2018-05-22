import time

from tests.utils import exec_esc_cmd, generate_keys, ValidateObject
from tests.consts import INIT_CLIENT_ID

BLOCK_TIME = 0


def test_block_created(init_node_process, gen_blocks_count=1):
    # Check in log if blocks are created
    from tests.client.utils import get_time_block

    global BLOCK_TIME
    BLOCK_TIME = get_time_block() * 2

    blocks_counter = 0
    for line in init_node_process.stderr:
        if b"NEW BLOCK created\n" in line:
            blocks_counter += 1

        if blocks_counter == gen_blocks_count:
            break

    assert gen_blocks_count == blocks_counter


def test_node_create_node(init_node_process, node_id="2"):
    count_blocks = len(exec_esc_cmd(INIT_CLIENT_ID, {"run": "get_block"}).
                       get('block', '').get('nodes', ''))
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "create_node"})

    try:
        account = ValidateObject(response['account'])
    except KeyError as err:
        raise KeyError(err, response)
    else:
        account.validate()

    start_time = time.time()

    while True:
        response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "get_block"})

        try:
            block = ValidateObject(response['block'], kind='block')
        except KeyError as err:
            raise KeyError(err, response)
        else:
            block.validate()

        count = len(response['block']['nodes'])
        if count > count_blocks:
            break
        assert time.time() - start_time < BLOCK_TIME
        time.sleep(BLOCK_TIME)

    node = ValidateObject(response['block']['nodes'], kind='block_nodes')
    node.validate()

    assert '000' in response['block']['nodes'][-1]['id']

    node_id = response['block']['nodes'][-1]['id']

    new_secret, new_pub_key, signature = generate_keys()

    response = exec_esc_cmd(INIT_CLIENT_ID,
                            {
                                "run": "change_node_key",
                                "pkey": new_pub_key,
                                "node": node_id
                            })

    try:
        assert response['result'] == 'Node key changed'
    except KeyError as err:
        raise KeyError(err, response)


def test_set_node_status(init_node_process, node_id='1', status='8'):

    response = exec_esc_cmd(INIT_CLIENT_ID,
                            {
                                'run': 'set_node_status',
                                'node': 1,
                                'status': status
                            })
    current_block = response['current_block_time']

    while True:
        response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_block'})
        if current_block != response['current_block_time']:
            break

    try:
        for node in response['block']['nodes']:
            if node['id'] == '000{}'.format(node_id):
                assert node['status'] == status
    except KeyError as err:
        raise KeyError(err, response)


def test_unset_node_status(init_node_process, node_id='1', status='16'):

    response = exec_esc_cmd(INIT_CLIENT_ID,
                            {
                                'run': 'unset_node_status',
                                'node': 1,
                                'status': status
                            })
    current_block = response['current_block_time']

    while True:
        response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_block'})
        if current_block != response['current_block_time']:
            break

    try:
        for node in response['block']['nodes']:
            if node['id'] == '000{}'.format(node_id):
                assert node['status'] == status
    except KeyError as err:
        raise KeyError(err, response)
