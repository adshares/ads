from . import exec_esc_cmd, ensure_init_client
from . import INIT_CLIENT_ID, INIT_NODE_OFFICE_PORT, INIT_CLIENT_ADDRESS, INIT_CLIENT_SECRET

import json
import pytest


def test_block_created(init_node_process, gen_blocks_count=1):
    # Check in log if blocks are created
    blocks_counter = 0
    for line in init_node_process.stdout:
        if line == b"NEW BLOCK created\n":
            blocks_counter += 1

        if blocks_counter == gen_blocks_count:
            break

    assert gen_blocks_count == blocks_counter


def _test_add_node():
    ensure_init_client()
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "create_node"})


def test_change_node_key():
    pass



