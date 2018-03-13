from . import clean_node_dir, create_node_env, get_node_path_dir
from . import INIT_NODE_ID,  ESCD_BIN_PATH

import subprocess
import os
import time


def test_first_node_start(gen_blocks_count=4):
    clean_node_dir(INIT_NODE_ID)
    create_node_env(INIT_NODE_ID, 8000, 8001, 8002)
    node_dir = get_node_path_dir(INIT_NODE_ID)

    log_fd = os.open(os.path.join(node_dir, "log.txt"), os.O_RDWR | os.O_CREAT)

    process = subprocess.Popen(["stdbuf", "-oL", ESCD_BIN_PATH, "--init", "1"],
                               cwd=node_dir, bufsize=1, stdout=log_fd)

    # Check in log if blocks are created
    blocks_counter = 0
    with open(os.path.join(node_dir, "log.txt"), 'r') as handler:

        while True:
            line = handler.readline()
            if line == "NEW BLOCK created\n":
                print(line)

                blocks_counter += 1

            if blocks_counter == gen_blocks_count:
                break

            if not line:
                time.sleep(0.5)

    os.close(log_fd)
    process.terminate()
    process.kill()


def test_add_node():
    pass


def test_change_node_key():
    pass



