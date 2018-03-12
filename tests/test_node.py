import os
import subprocess
import shutil

HPX_ROOT = os.path.dirname(os.path.dirname(__file__))
ESC_BIN_PATH = os.path.join(HPX_ROOT, "esc", "esc")
ESCD_BIN_PATH = os.path.join(HPX_ROOT, "escd", "escd")


def get_node_path_dir(node_id, prefix="node"):
    node_path = os.path.join("/tmp", prefix, node_id)
    if not os.path.exists(node_path):
        os.makedirs(node_path)
    return node_path


def clean_node_dir(node_id):
    node_dir = get_node_path_dir(node_id)
    shutil.rmtree(node_id, ignore_errors=True)


def get_client_path_dir(client_id):
    return get_node_path_dir(client_id, "client")


def create_node_env(node_id, office_port, server_port, addr="127.0.0.1", peer_port=None, key=None):
    node_id = str(node_id)

    options = [
        "addr=%s" %addr,
        "svid=%s" %node_id,
        "offi=%i" %office_port,
        "port=%i" %server_port
    ]
    if peer_port is not None:
        options.append("peer=%s:%i"%(addr, peer_port))

    node_path_dir = get_node_path_dir(node_id)
    with open(os.path.join(node_path_dir, "options.cfg"), 'w') as fh:
        fh.write("\n".join(options))

    node_key_path_dir = os.path.join(node_path_dir, "key")
    if not os.path.exists(node_key_path_dir):
        os.makedirs(node_key_path_dir)

    os.system("chmod go-rx %s" %node_key_path_dir)
    if key is not None:
        node_key_path = os.path.join(node_key_path_dir, "key.txt")
        with open(node_key_path_dir, 'w') as fh:
            fh.write(key)
        os.system("chmod go-r %s" % node_key_path)


def create_client_env(client_id, port, address, secret, host="127.0.0.1"):
    client_id = str(client_id)

    client_path_dir = get_client_path_dir(client_id)

    options = [
        "host=%s" %host,
        "port=%i" %port,
        "address=%s" %address,
        "secret=%s" %secret
    ]
    with open(os.path.join(client_path_dir, "options.cfg"), 'w') as fh:
        fh.write("\n".join(options))


def create_dummy_nodes():
    pass


def create_dummy_clients():
    pass



def test_init_first_node(node_id="init", gen_blocks_count=100):
    clean_node_dir(node_id)
    node_dir = get_node_path_dir(node_id)

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


def test_answer():
    create_node_env(1, 1000, 2000, peer_port=2000)
    create_client_env(1, 7191, "0001-00000001-XXXX", "5BF11F5D0130EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C")
    assert 4 == 4
