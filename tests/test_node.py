import os


def get_node_path_dir(node_id):
    node_path = os.path.join("/tmp", node_id)
    if not os.path.exists(node_id):
        os.makedirs(node_path)
    return node_path


def create_node(node_id, office_port, server_port, addr="127.0.0.1", peer_port=None, key=None):
    options = [
        "addr=%s" %addr,
        "svid=%i" %node_id,
        "offi=%i" %office_port,
        "port=%i" %server_port
    ]
    if peer_port is not None:
        options.append("peer=%s:%i"%(addr, peer_port))

    node_path_dir = get_node_path_dir(node_id)
    with open(os.path.join(node_path_dir, "options.cfg"), 'w') as fh:
        fh.writelines(options)

    node_key_path_dir = os.path.join(node_path_dir, "key")
    os.makedirs(node_key_path_dir)

    os.system("chmod go-rx %s" %node_key_path_dir)
    if key is not None:
        node_key_path = os.path.join(node_key_path_dir, "key.txt")
        with open(node_key_path_dir, 'w') as fh:
            fh.write(key)
        os.system("chmod go-r %s" % node_key_path)


def func(x):
    create_node(1, 1000, 2000, peer_port=2000)
    return x + 1


def test_answer():
    assert func(3) == 5
