import asyncio
import random
import time

from tests import logger

from tests.utils import exec_esc_cmd, generate_message
from tests.consts import INIT_CLIENT_ID
from tests.client.utils import (update_user_env, get_balance_user,
                                get_time_block)


USERS = []
NODES = []
BLOCK_TIME = 0


def create_user(id_user, id_node):
    response = exec_esc_cmd(INIT_CLIENT_ID,
                            {"run": "create_account", "node": id_node})
    new_account = response['new_account']
    return new_account['address'], new_account['id']


async def send_money_async(amount=10):
    if not USERS:
        return
    random.shuffle(USERS)
    address_sender = USERS[-1]
    USERS.remove(address_sender)
    address_receiver = USERS[0]
    USERS.remove(address_receiver)
    logger.info("SENDING MONEY FROM {} TO {}".format(address_sender[1],
                                                     address_receiver[1]))
    start_balance_receiver = get_balance_user(address_receiver[0])
    await asyncio.sleep(1)
    start_balance_sender = get_balance_user(address_sender[0])
    logger.info("STAR BALANCE FOR SENDER {}: {}. FOR RECEIVER {}: {}".format(
        address_sender[1], start_balance_sender,
        address_receiver[1], start_balance_receiver))

    message = generate_message()

    response = exec_esc_cmd(address_sender[0], {'run': 'send_one',
                                                'address': address_receiver[1],
                                                'message': message,
                                                'amount': amount})

    logger.info("USER {} SEND {} TO {}".format(address_sender[1],
                                               amount,
                                               address_receiver[1]))

    assert float(response['tx']['deduct']) == float(amount)

    finish_balance_receiver = get_balance_user(address_receiver[0])
    await asyncio.sleep(1)
    finish_balance_sender = get_balance_user(address_sender[0])

    logger.info("FINISH BALANCE FOR SENDER {}: {}. FOR RECEIVER {}: {}".format(
        address_sender[1], finish_balance_sender,
        address_receiver[1], finish_balance_receiver)
    )

    await asyncio.sleep(1)

    assert float(response['tx']['deduct']) == float(amount)

    difference_receiver = float(start_balance_receiver) + float(amount)

    assert difference_receiver == float(get_balance_user(address_receiver[0]))


async def send_many_money_async(addresses, sender_id, amount=1):
    logger.info("SEND MONEY TO MANY FROM {}".format(sender_id))
    start_balance_receivers = [{'id': client[0],
                                'address': client[1],
                                'start_balance': get_balance_user(client[0])}
                               for client in addresses]

    response = exec_esc_cmd(sender_id, {
        "run": "send_many",
        "wires": dict({client['address']: amount
                       for client in start_balance_receivers})})

    assert (float(response['tx']['deduct']) ==
            len(start_balance_receivers) * amount)

    await asyncio.sleep(2)
    for receiver in start_balance_receivers:
        must_be = float(receiver['start_balance']) + float(amount)
        assert must_be == float(get_balance_user(receiver['id']))


def create_users(count=10):
    # As INIT user, create client with client_id
    node = '0001'
    for i in range(1, count + 1):
        logger.info('CREATE USER {}'.format(i))
        create_user(i, node)
    time.sleep(BLOCK_TIME)
    response = exec_esc_cmd(INIT_CLIENT_ID,
                            {'run': 'get_accounts', 'node': node})['accounts']
    address_list = [(client['id'], client['address'])
                    for client in response if client['id'] != '0']
    for _id, address in address_list:
        update_user_env(client_id=_id, address=address)
    return address_list


def create_node(id_node):
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "create_node"})
    hex_id = hex(id_node).split('x')[-1]
    internal_id_node = ''.join(['0' for _ in range(4 - len(hex_id))] +
                               [hex_id])


def create_nodes(count=2):
    for i in range(2, count + 2):
        create_node(i)
    start_time = time.time()

    while True:
        response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "get_block"})
        count_nodes = len(response['block']['nodes'])
        if len(NODES) == count_nodes - 1:
            break
        if time.time() - start_time > BLOCK_TIME:
            break
        time.sleep(BLOCK_TIME)

    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "get_block"})
    assert len(response['block']['nodes']) - 1 == count


def test_create_nodes(init_node_process, count=10):
    global BLOCK_TIME
    BLOCK_TIME = get_time_block() * 2
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_block'})
    count_start = int(response.get('block').get('node_count'))
    create_nodes(count)
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_block'})
    count_created = int(response.get('block').get('node_count'))
    assert count_created - 1 - count_start == count


def test_multi_send_one_money(init_node_process, count_transactions=16):
    global USERS
    USERS = create_users(count_transactions)
    tasks = [send_money_async() for _ in range(count_transactions)]
    loop = asyncio.get_event_loop()
    loop.run_until_complete(asyncio.wait(tasks))
