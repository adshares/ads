import asyncio
import random
import time

from . import exec_esc_cmd
from . import INIT_CLIENT_ID
from .utils import update_user_env


NODES = []

loop = asyncio.get_event_loop()


async def create_user(id_user):
    random.shuffle(NODES)
    id_node = NODES[random.randint(0, len(NODES)-1)]
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "create_account", "node": id_node})
    address = response['new_account']['address']

    await asyncio.sleep(60)

    update_user_env(client_id=id_user, address=address)


def create_users(count=10):
    # As INIT user, create client with client_id
    s_time = time.time()
    users = [create_user(i) for i in range(1, count+1)]
    loop.run_until_complete(asyncio.wait(users))
    loop.close()
    f_time = time.time()
    print("Spend time {} for create {} users".format(f_time - s_time, count))


async def create_node(id_node):
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "create_node"})

    hex_id = hex(id_node).split('x')[-1]
    inside_id_node = ''.join(['0' for _ in range(4-len(hex_id))] + [hex_id])

    start_time = time.time()
    while True:
        response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "get_block"})
        if inside_id_node in [node.get('id') for node in response['block']['nodes']]:
            break
        await asyncio.sleep(10)
        assert time.time() - start_time < 70

    NODES.append(inside_id_node)

    NEW_PKEY = "D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17"

    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "change_node_key", "pkey": NEW_PKEY, "node": id_node})


def create_nodes(count=2):
    s_time = time.time()
    nodes = [create_node(i) for i in range(2, count+1)]
    loop.run_until_complete(asyncio.wait(nodes))
    f_time = time.time()
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "get_block"})
    print("Spend time {} for create {} Nodes".format(f_time - s_time,
                                                     len(response['block']['nodes']) - 1))
    assert len(response['block']['nodes']) - 1 == count
    for node in response['block']['nodes']:
        node_id = node.get("id")
        if node_id != "0000":
            NODES.append(node_id)


def test_create_nodes(init_node_process):
    create_nodes(20)


def __test_create_users(init_node_process):
    create_users(2)
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': "get_accounts"}, with_get_me=False)