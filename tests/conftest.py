from . import create_node_env, clean_node_dir, get_node_path_dir
from . import INIT_NODE_ID, INIT_NODE_OFFICE_PORT, INIT_NODE_SERVER_PORT, ESCD_BIN_PATH

import subprocess
import pytest


@pytest.fixture(scope='session')
def init_node_process(init_blocks_counter = 1):
    clean_node_dir(INIT_NODE_ID)
    create_node_env(INIT_NODE_ID, INIT_NODE_OFFICE_PORT, INIT_NODE_SERVER_PORT)
    node_dir = get_node_path_dir(INIT_NODE_ID)

    process = subprocess.Popen([ESCD_BIN_PATH, "--init", "1"],
                            cwd=node_dir, bufsize=1, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    blocks_counter = 0
    for line in process.stdout:
        print(line, "init")
        if line == b"NEW BLOCK created\n":
            blocks_counter += 1

        #if line == b"OFFICE online 1\n":
        #    break

        if blocks_counter == init_blocks_counter:
            break

    yield process

    process.terminate()
    process.kill()
