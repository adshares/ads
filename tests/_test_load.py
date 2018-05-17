import asyncio
import random
import time

from . import exec_esc_cmd
from . import INIT_CLIENT_ID, INIT_CLIENT_ADDRESS
from .utils import update_user_env, get_balance_user, get_user_address


USERS = []


def create_user(id_user, id_node):
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "create_account", "node": id_node})
    new_account = response['new_account']
    return new_account['address'], new_account['id']


async def send_money_async(amount=10):
    if not USERS:
        return
    print("===============================================================================")
    random.shuffle(USERS)
    address_sender = USERS[-1]
    USERS.remove(address_sender)
    address_receiver = USERS[0]
    USERS.remove(address_receiver)
    print("SENDING MONEY FROM {} TO {}".format(address_sender[1], address_receiver[1]))
    start_balance_receiver = get_balance_user(address_receiver[0])
    await asyncio.sleep(1)
    start_balance_sender = get_balance_user(address_sender[0])
    print("STAR BALANCE FOR SENDER {}: {}. FOR RECEIVER {}: {}".format(address_sender[1], start_balance_sender,
                                                                 address_receiver[1], start_balance_receiver))

    message = "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"

    response = exec_esc_cmd(address_sender[0], {'run': 'send_one',
                                             'address': address_receiver[1],
                                             'message': message,
                                             'amount': amount})

    print("USER {} SEND {} TO {}".format(address_sender[1], amount, address_receiver[1]))
    assert float(response['tx']['deduct']) == float(amount)

    finish_balance_receiver = get_balance_user(address_receiver[0])
    await asyncio.sleep(1)
    finish_balance_sender = get_balance_user(address_sender[0])

    print("FINISH BALANCE FOR SENDER {}: {}. FOR RECEIVER {}: {}".format(address_sender[1], finish_balance_sender,
                                                                 address_receiver[1], finish_balance_receiver))

    await asyncio.sleep(1)

    assert float(response['tx']['deduct']) == float(amount)

    difference_receiver = float(start_balance_receiver) + float(amount)

    assert difference_receiver == float(get_balance_user(address_receiver[0]))


async def send_many_money_async(addresses, sender_id, amount=1):
    print("SEND MONEY TO MANY FROM {}".format(sender_id))
    start_balance_receivers = [{'id': client[0],
                                'address': client[1],
                                'start_balance': get_balance_user(client[0])}
                               for client in addresses]

    response = exec_esc_cmd(sender_id, {
        "run": "send_many",
        "wires": dict({client['address']: amount for client in start_balance_receivers})})

    assert float(response['tx']['deduct']) == len(start_balance_receivers) * amount

    await asyncio.sleep(2)
    for receiver in start_balance_receivers:
        must_be = float(receiver['start_balance']) + float(amount)
        assert must_be == float(get_balance_user(receiver['id']))


def create_users(count=10):
    # As INIT user, create client with client_id
    node = '0001'
    for i in range(1, count + 1):
        print('CREATE USER {}'.format(i))
        create_user(i, node)
    time.sleep(70)
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_accounts', 'node': node})['accounts']
    address_list = [(client['id'], client['address']) for client in response if client['id'] != '0']
    for _id, address in address_list:
        update_user_env(client_id=_id, address=address)
    return address_list


async def create_node(id_node):
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "create_node"})

    hex_id = hex(id_node).split('x')[-1]
    inside_id_node = ''.join(['0' for _ in range(4-len(hex_id))] + [hex_id])
    start_time = time.time()

    while True:
        response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "get_block"})
        if inside_id_node in [node.get('id') for node in response['block']['nodes']]:
            break
        await asyncio.sleep(20)
        if time.time() - start_time > 60:
            break
    create_node(inside_id_node)
    NEW_PKEY = "D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17"

    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "change_node_key", "pkey": NEW_PKEY, "node": id_node})


def create_nodes(count=2):
    nodes = [create_node(i) for i in range(2, count+2)]
    loop.run_until_complete(asyncio.wait(nodes))
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "get_block"})
    assert len(response['block']['nodes']) - 1 == count
    for node in response['block']['nodes']:
        node_id = node.get("id")
        if node_id != "0000":
            NODES.append(node_id)

# def test_create_nodes(init_node_process, count=10):
#     create_nodes(count)
#     response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_block'})
#     count_created = int(response.get('block').get('node_count'))
#     assert count_created - 1 == count


# def test_multi_send_one_money(init_node_process, count_transactions=20):
#     global USERS
#     USERS = create_users(count_transactions)
#     tasks = [send_money_async() for _ in range(count_transactions)]
#     loop = asyncio.get_event_loop()
#     loop.run_until_complete(asyncio.wait(tasks))
#     assert True


def test_vanish_zero_account(init_node_process):
    address, id_client = create_user(1, id_node=1)
    time.sleep(60)
    update_user_env(client_id=id_client, address=address)

    response = exec_esc_cmd(id_client, {'run': 'get_me'}, with_get_me=False)

    start_block = response['current_block_time']
    start_balance = response['account']['balance']

    count = 0
    while True:
        response = exec_esc_cmd(id_client, {'run': 'get_me'}, with_get_me=False)
        new_block = response['current_block_time']
        if new_block != start_block:
            start_block = new_block
            count += 1
        if count == 4:
            break
        time.sleep(5)

    response = exec_esc_cmd(id_client, {'run': 'get_me'}, with_get_me=False)

    assert float(start_balance) != float(response['account']['balance'])







