import time

from . import exec_esc_cmd
from . import INIT_CLIENT_ID


def test_block_created(init_node_process, gen_blocks_count=1):
    # Check in log if blocks are created
    blocks_counter = 0
    for line in init_node_process.stderr:
        if b"NEW BLOCK created\n" in line:
            blocks_counter += 1

        if blocks_counter == gen_blocks_count:
            break

    assert gen_blocks_count == blocks_counter


def test_node_create_node(init_node_process, node_id="2"):
    count_blocks = len(exec_esc_cmd(INIT_CLIENT_ID,
                                    {"run": "get_block"}).get('block', '').get('nodes', ''))
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "create_node"})

    start_time = time.time()

    while True:
        response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "get_block"})
        if response.get('block'):
            count = len(response['block']['nodes'])
            if count > count_blocks:
                break
        time.sleep(10)
        assert time.time() - start_time < 70

    assert '000' in response['block']['nodes'][-1]['id']

    node_id = response['block']['nodes'][-1]['id']

    NEW_PKEY = "D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17"
    NEW_PRIV_KEY = "FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6"

    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "change_node_key", "pkey": NEW_PKEY, "node": node_id})

    assert response['result'] == 'Node key changed'


def __test_set_node_status(init_node_process, node_id='2'):
    # TODO: This feature dose not work. Test will continue when feature will be fixes
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'set_node_status', 'node': 2, 'status': '8'})

    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_block'})

    pass
