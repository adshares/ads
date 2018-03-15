from . import exec_esc_cmd, create_node_env
from . import INIT_CLIENT_ID, INIT_NODE_SERVER_PORT


def test_block_created(init_node_process, gen_blocks_count=1):
    # Check in log if blocks are created
    blocks_counter = 0
    for line in init_node_process.stdout:
        if line == b"NEW BLOCK created\n":
            blocks_counter += 1

        if blocks_counter == gen_blocks_count:
            break

    assert gen_blocks_count == blocks_counter


def test_change_node_key(init_node_process, node_id="2"):
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "create_node"})

    #create_node_env(node_id, 8100, 8101, INIT_NODE_SERVER_PORT)



