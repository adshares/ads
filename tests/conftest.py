import pytest
import subprocess
import threading

from .node.utils import clean_node_dir, create_node_env, get_node_path_dir
from .consts import INIT_NODE_OFFICE_PORT, INIT_NODE_SERVER_PORT, INIT_NODE_ID, ESCD_BIN_PATH, INIT_CLIENT_ID


def write_out(process, file):
    for line in process.stderr:
        file.write(line)
    file.close()


@pytest.fixture(scope='session')
def init_node_process(init_blocks_counter=1):
    # Clean node per session
    clean_node_dir(INIT_NODE_ID)
    create_node_env(INIT_NODE_ID, INIT_NODE_OFFICE_PORT, INIT_NODE_SERVER_PORT)

    # Clean init client per session
    from .client.utils import create_init_client, clean_client_dir
    clean_client_dir(INIT_CLIENT_ID)
    create_init_client()

    node_dir = get_node_path_dir(INIT_NODE_ID)

    logfile = open('/tmp/node/test.txt', 'wb')
    process = subprocess.Popen([ESCD_BIN_PATH, "--init", "1"],
                               cwd=node_dir, bufsize=1,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    blocks_counter = 0
    for line in process.stderr:
        print(line, "init")
        logfile.write(line)
        if b"NEW BLOCK created\n" in line:
            blocks_counter += 1

        if blocks_counter == init_blocks_counter:
            thr = threading.Thread(target=write_out, args=(process, logfile))
            thr.start()
            break

    yield process
    process.terminate()
    process.kill()