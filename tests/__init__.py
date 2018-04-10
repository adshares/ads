import os
import shutil
import subprocess
import json


HPX_ROOT = os.path.dirname(os.getcwd())
ESC_BIN_PATH = os.path.join(HPX_ROOT, "build", "esc", "esc")
ESCD_BIN_PATH = os.path.join(HPX_ROOT, "build", "escd", "escd")


INIT_NODE_ID = '1'
INIT_NODE_OFFICE_PORT = 8000
INIT_NODE_SERVER_PORT = 8001


INIT_CLIENT_ID = '1'
INIT_CLIENT_ADDRESS = "0001-00000000-XXXX"
INIT_CLIENT_SECRET = "14B183205CA661F589AD83809952A692DFA48F5D490B10FD120DA7BF10F2F4A0"


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


def get_client_dir(client_id):
    return get_node_path_dir(client_id, "client")


def clean_client_dir(client_id):
    client_id = str(client_id)
    client_dir = get_client_dir(client_id)
    shutil.rmtree(client_dir, ignore_errors=True)


def create_client_env(client_id, port, address, secret, host="127.0.0.1"):
    client_id = str(client_id)

    client_path_dir = get_client_dir(client_id)

    options = [
        "host=%s" %host,
        "port=%i" %port,
        "address=%s" %address,
        "secret=%s" %secret
    ]
    with open(os.path.join(client_path_dir, "settings.cfg"), 'w') as fh:
        fh.write("\n".join(options))


def ensure_init_client():
    if not os.path.exists(get_client_dir(INIT_CLIENT_ID)):
        create_init_client()


def create_init_client():
    clean_client_dir(INIT_CLIENT_ID)
    create_client_env(INIT_CLIENT_ID, INIT_NODE_OFFICE_PORT,
                      address=INIT_CLIENT_ADDRESS,
                      secret=INIT_CLIENT_SECRET)


def exec_esc_cmd(client_id, js_command, with_get_me=True, cmd_extra=None, timeout=10):
    client_dir = get_client_dir(client_id)

    esc_cmd = [ESC_BIN_PATH]
    if cmd_extra:
        esc_cmd = esc_cmd + cmd_extra
        esc_cmd = ' '.join(esc_cmd)

    process = subprocess.Popen(esc_cmd, cwd=client_dir, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, stdin=subprocess.PIPE, shell=True)

    cmds = [js_command]
    if with_get_me:
        cmds.insert(0, {'run': "get_me"})

    for cmd in cmds:
        process.stdin.write(str.encode(json.dumps(cmd)+"\n"))

    stdout, stderr = process.communicate(timeout=timeout)

    raw_response = stdout.decode("utf-8")
    raw_response = raw_response.replace("}\n", "}").replace("}{", "},{")

    responses = json.loads("[%s]" %raw_response)

    return responses[-1]
