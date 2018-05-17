import pytest
import subprocess
import threading
import time
from ..consts import (INIT_NODE_OFFICE_PORT, INIT_NODE_ID,
                      INIT_CLIENT_ID, ESCD_BIN_PATH, INIT_NODE_SERVER_PORT)

from .. import utils as tests_utils


@pytest.fixture(scope='session')
def init_node_process(init_blocks_counter=1):
    # Clean node per session
    tests_utils.clean_node_dir(INIT_NODE_ID)
    tests_utils.create_node_env(INIT_NODE_ID, INIT_NODE_OFFICE_PORT, INIT_NODE_SERVER_PORT)

    # Clean init client per session
    tests_utils.clean_client_dir(INIT_CLIENT_ID)
    tests_utils.create_init_client()

    node_dir = tests_utils.get_node_path_dir(INIT_NODE_ID)
    process = subprocess.Popen([ESCD_BIN_PATH, "--init", "1"],
                               cwd=node_dir, bufsize=1,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    blocks_counter = 0
    for line in process.stderr:
        print(line, "init")
        if b"NEW BLOCK created\n" in line:
            blocks_counter += 1

        if blocks_counter == init_blocks_counter:
            break

    yield process
    process.terminate()
    process.kill()


def manual_init_node_process(node_id, client_id, key, port,
                             offi, id_block, offset_block, init_blocks_counter=1):
    # Clean node per session
    tests_utils.clean_node_dir(node_id)
    tests_utils.create_node_env(node_id, port, offi, key=key, offset_block=offset_block, id_block=id_block)

    # Clean init client per session
    tests_utils.clean_client_dir(client_id)
    tests_utils.create_init_client()

    node_dir = tests_utils.get_node_path_dir(node_id)
    process = subprocess.Popen([ESCD_BIN_PATH, "-m", "1"],
                               cwd=node_dir, bufsize=1,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    for _ in process.stderr:
        pass


def create_node_without_start():
    count_blocks = len(tests_utils.exec_esc_cmd(INIT_CLIENT_ID,
                                    {"run": "get_block"}).get('block', '').get('nodes', ''))
    response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {"run": "create_node"})

    start_time = time.time()

    while True:
        response = tests_utils.exec_esc_cmd(INIT_CLIENT_ID, {"run": "get_block"})
        if response.get('block'):
            count = len(response['block']['nodes'])
            if count > count_blocks:
                break
        time.sleep(10)
        assert time.time() - start_time < 70

    return response


def create_node_and_run(node_id, client_id, port, offi):

    response = create_node_without_start()

    offset_block = response['block']['id'][:3]
    id_block = response['block']['id'][3:]

    SECRET = 'FF767FC8FAF9CFA8D2C3BD193663E8B8CAC85005AD56E085FAB179B52BD88DD6'

    thr = threading.Thread(target=manual_init_node_process, kwargs={'node_id':node_id, 'client_id': client_id,
                                                                    'key': SECRET, 'port': port, 'offi': offi,
                                                                    'offset_block': offset_block, 'id_block': id_block})
    thr.start()
    # thr.join()
