import os


def get_node_path_dir(node_id, prefix="node"):
    node_path = os.path.join("tmp", "node", node_id)
    if not os.path.exists(node_id):
        os.makedirs(node_path)
    return node_path


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
        fh.writelines(options)

    node_key_path_dir = os.path.join(node_path_dir, "key")
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
        fh.writelines(options)


def func(x):
    create_node_env(1, 1000, 2000, peer_port=2000)
    create_client_env(1, 7191, "0001-00000001-XXXX", "5BF11F5D0130EC994F04B6C5321566A853B7393C33F12E162A6D765ADCCCB45C")
    return x + 1