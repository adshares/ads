import os
import shutil
import subprocess
import threading
import time

from tests.consts import (INIT_CLIENT_ID, D_BIN_PATH)

from tests.utils import exec_esc_cmd, generate_keys


def get_node_path_dir(node_id, prefix="node"):
    node_id = str(node_id)
    node_path = os.path.join("/tmp", prefix, node_id)
    if not os.path.exists(node_path):
        os.makedirs(node_path)
    return node_path


def clean_node_dir(node_id):
    node_id = str(node_id)
    node_dir = get_node_path_dir(node_id)
    shutil.rmtree(node_dir, ignore_errors=True)


def create_node_env(node_id, office_port, server_port, addr="127.0.0.1",
                    peer_port=None, key=None, id_block=None,
                    offset_block=None):
    node_id = str(node_id)

    options = [
        "addr=%s" % addr,
        "svid=%s" % node_id,
        "offi=%i" % office_port,
        "port=%i" % server_port
    ]
    if peer_port is not None:
        options.append("peer=%s:%i" % (addr, peer_port))

    node_path_dir = get_node_path_dir(node_id)
    with open(os.path.join(node_path_dir, "options.cfg"), 'w') as fh:
        fh.write("\n".join(options))

    node_key_path_dir = os.path.join(node_path_dir, "key")
    if not os.path.exists(node_key_path_dir):
        os.makedirs(node_key_path_dir)

    os.system("chmod go-rx %s" % node_key_path_dir)
    if key is not None:
        node_key_path = os.path.join(node_key_path_dir, "key.txt")
        with open(node_key_path, 'w') as fh:
            fh.write(key)
        os.system("chmod go-r %s" % node_key_path)

        if offset_block and id_block:
            prev_node = str(int(node_id) - 1)
            prev_node_dir = get_node_path_dir(prev_node)
            shutil.copy(os.path.join(prev_node_dir, 'blk', offset_block,
                                     id_block,  'servers.srv'), node_path_dir)


def manual_init_node_process(node_id, client_id, key, port,
                             offi, id_block, offset_block,
                             init_blocks_counter=1):
    # Clean node per session
    clean_node_dir(node_id)
    create_node_env(node_id, port, offi, key=key, offset_block=offset_block,
                    id_block=id_block)

    # Clean init client per session
    from tests.client.utils import clean_client_dir, create_init_client
    clean_client_dir(client_id)
    create_init_client()

    node_dir = get_node_path_dir(node_id)
    process = subprocess.Popen([D_BIN_PATH, "-m", "1"],
                               cwd=node_dir, bufsize=1,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    for _ in process.stderr:
        pass


def create_node_without_start():
    count_blocks = len(exec_esc_cmd(INIT_CLIENT_ID, {"run": "get_block"}).
                       get('block', '').get('nodes', ''))
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

    return response


def create_node_and_run(node_id, client_id, port, offi):

    response = create_node_without_start()

    offset_block = response['block']['id'][:3]
    id_block = response['block']['id'][3:]

    new_secret, new_pub_key, signature = generate_keys()

    thr = threading.Thread(target=manual_init_node_process,
                           kwargs={
                               'node_id': node_id,
                               'client_id': client_id,
                               'key': new_secret,
                               'port': port,
                               'offi': offi,
                               'offset_block': offset_block,
                               'id_block': id_block
                           })
    thr.start()
