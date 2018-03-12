from . import get_node_path_dir, clean_node_dir
from . import ESCD_BIN_PATH, INIT_NODE_ID

import subprocess


def test_init_first_node(gen_blocks_count=100):
    clean_node_dir(INIT_NODE_ID)
    node_dir = get_node_path_dir(INIT_NODE_ID)

    print("%s --init 1" %ESCD_BIN_PATH)
    process = subprocess.Popen(["%s --init 1" %ESCD_BIN_PATH], cwd=node_dir,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)

    blocks_counter = 0
    for line in process.stdout:
        if line == b"NEW BLOCK created\n":
            blocks_counter+=1

        if blocks_counter == gen_blocks_count:
            break

    process.terminate()
    assert blocks_counter == gen_blocks_count


