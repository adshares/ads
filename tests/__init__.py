import asyncio
import os
import shutil
import subprocess
import json


HPX_ROOT = os.path.dirname(os.path.dirname(__file__))
ESC_BIN_PATH = os.path.join(HPX_ROOT, "build", "esc", "esc")
ESCD_BIN_PATH = os.path.join(HPX_ROOT, "build", "escd", "escd")


INIT_NODE_ID = '1'
INIT_NODE_OFFICE_PORT = 8003
INIT_NODE_SERVER_PORT = 8005


INIT_CLIENT_ID = '1'
INIT_CLIENT_ADDRESS = "0001-00000000-XXXX"
INIT_CLIENT_SECRET = "14B183205CA661F589AD83809952A692DFA48F5D490B10FD120DA7BF10F2F4A0"

COMMANDS = ({'run': 'get_me'},
            {'run': 'broadcast', 'message': 'D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17'},
            # {"run": "change_node_key", "pkey": "2D1FC97FA56B785E0FDAE5752DE613BAD7FBBB5EBBB46DAEE5DBFA822F976B63"},
            {"run": "create_account"},
            {"run": "create_node"},
            {"run": "get_account", "address": INIT_CLIENT_ADDRESS},
            {"run": "get_blocks", "from": 0, "to": 0},
            {"run": "get_broadcast", "from": 1491210824},
            {"run": "get_log", "from": 1491210824},
            {"run": "get_transaction", "txid": "0001:00000002:0001"},
            {"run": "get_vipkeys", "viphash": "D3FD529F6305F574BA22F3BDF761B4778094CB38958300ACDF21D35BE03BDC4F"},
            {"run": "get_signatures", "block": "1508317920"},
            {"run": "get_block", "block": "1508317920"},
            {"run": "get_accounts", "block": "1508317920", "node": 1},
            {"run": "get_message_list", "block": "1508317920"},
            {"run": "get_message", "block": "1508317920", "node": 1, "node_msid": 2},
            {"run": "retrieve_funds", "address": INIT_CLIENT_ADDRESS},
            {"run": "send_again",
             "data": "05010000000000010000004A3CC9580200020000000000204E0000000000000300000000003075000000000000521B9E6932FD4973EC8364662B898249635C777BB0AA801F7DA5E9423C920EAECC39AD7B519FF6C6D27E43B9B294C0504816CE20735F11E9D8A252CF8A686806"},
            {"run": "send_one", "address": "0003-00000000-XXXX", "amount": 2.1,
             "message": "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"},
            {"run":"send_many", "wires": {"0002-00000000-XXXX": 20000.0, "0003-00000000-XXXX": 0.003}},
            {"run": "set_account_status", "address": "0001-00000000-XXXX", "status": "10"},
            {"run": "set_node_status", "node": "1", "status": "8"},
            {"run": "unset_account_status", "address": "0001-00000000-XXXX", "status": "10"},
            {"run": "unset_node_status", "node": "1", "status": "8"})


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
                    peer_port=None, key=None, id_block=None, offset_block=None):
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
        with open(node_key_path, 'w') as fh:
            fh.write(key)
        os.system("chmod go-r %s" % node_key_path)

        if offset_block and id_block:
            prev_node = str(int(node_id) - 1)
            prev_node_dir = get_node_path_dir(prev_node)
            shutil.copy(os.path.join(prev_node_dir, 'blk', offset_block, id_block,  'servers.srv'), node_path_dir)


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
        esc_cmd.extend(cmd_extra)

    cmds = [js_command]
    if with_get_me:
        cmds.insert(0, {'run': "get_me"})

    process = subprocess.Popen(esc_cmd, cwd=client_dir, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, stdin=subprocess.PIPE)

    for cmd in cmds:
        process.stdin.write(str.encode(json.dumps(cmd) + "\n"))

    stdout, stderr = process.communicate(timeout=timeout)

    raw_response = stdout.decode("utf-8")
    raw_response = raw_response.replace("}\n", "}").replace("}{", "},{")
    responses = None
    try:
        responses = json.loads("[%s]" %raw_response)
    except json.decoder.JSONDecodeError:
        import pdb
        pdb.set_trace()

    return responses[-1]
