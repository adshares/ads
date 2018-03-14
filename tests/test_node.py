from . import clean_node_dir, create_node_env, get_node_path_dir
from . import INIT_NODE_ID,  ESCD_BIN_PATH

import subprocess


def test_first_node_start(gen_blocks_count=2):
    clean_node_dir(INIT_NODE_ID)
    create_node_env(INIT_NODE_ID, 8000, 8001, 8002)
    node_dir = get_node_path_dir(INIT_NODE_ID)

    process = subprocess.Popen(["stdbuf", "-oL", ESCD_BIN_PATH, "--init", "1"],
                               cwd=node_dir, bufsize=1, stdout=subprocess.PIPE)

    # Check in log if blocks are created
    blocks_counter = 0
    for line in process.stdout:
        print(line, blocks_counter)
        if line == b"NEW BLOCK created\n":
            blocks_counter += 1

        if blocks_counter == gen_blocks_count:
            break

    process.terminate()
    process.kill()


def test_add_node():
    pass


def test_change_node_key():
    pass



