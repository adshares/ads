import os
import shutil
import subprocess
import json


HPX_ROOT = os.path.dirname(os.path.dirname(__file__))
ESC_BIN_PATH = os.path.join(HPX_ROOT, "build", "esc", "esc")
ESCD_BIN_PATH = os.path.join(HPX_ROOT, "build", "escd", "escd")


INIT_NODE_ID = '1'
INIT_NODE_OFFICE_PORT = 8000
INIT_NODE_SERVER_PORT = 8001


INIT_CLIENT_ID = '1'


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
    with open(os.path.join(client_path_dir, "settings.cfg"), 'w') as fh:
        fh.write("\n".join(options))


def exec_esc_cmd(client_id, json_commands, esc_params=None):
    client_dir = get_client_path_dir(client_id)

    esc_cmd = [ESC_BIN_PATH]
    if esc_params:
        esc_cmd = esc_cmd + esc_params

    process = subprocess.Popen(esc_cmd, cwd=client_dir, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, stdin=subprocess.PIPE, shell=True)

    if type(json_commands) != list:
        json_commands = [json_commands]

    for cmd in json_commands:
        process.stdin.write(str.encode(json.dumps(cmd)+"\n"))

    raw_response = process.communicate(timeout=10)[0].decode("utf-8")

    responses = []
    for cmd in json_commands[::-1]:
        cmd_str = json.dumps(cmd)
        resp_index = raw_response.find(json.dumps(cmd))
        if resp_index == -1:
            print("Response not found for cmd %s in %s" %(cmd_str, raw_response))
            return None
        responses.insert(0, raw_response[resp_index + len(cmd_str):])
        raw_response = raw_response[0:resp_index-1]

    if len(json_commands) == 1:
        responses = responses[0]

    return responses
