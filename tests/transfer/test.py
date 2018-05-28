import time

from tests.consts import INIT_CLIENT_ID
from tests.utils import generate_message, exec_esc_cmd, ValidateObject

BLOCK_TIME = 0


def set_block_time():
    from tests.client.utils import get_time_block
    global BLOCK_TIME
    BLOCK_TIME = get_time_block() * 2


def test_send_many_to_unknown_address(init_node_process):
    set_block_time()

    unknown_address = '0001-00000000-9AAA'
    message = generate_message()

    response = exec_esc_cmd(INIT_CLIENT_ID,
                                            {
                                                "run": "send_one",
                                                "address": unknown_address,
                                                "message": message,
                                                "amount": 1
                                            })

    assert 'error' not in response


def test_send_cash_one_user(init_node_process, amount=100.0, client_id='2'):
    from tests.client import utils as client_utils

    address_receiver = client_utils.create_account()

    start_balance_receiver = client_utils.get_balance_user(client_id)

    message = generate_message()

    response = client_utils.exec_esc_cmd(INIT_CLIENT_ID,
                                         {
                                             "run": "send_one",
                                             "address": address_receiver,
                                             'message': message,
                                             "amount": amount
                                         })

    assert float(response['tx']['deduct']) == float(amount)

    difference_r = float(start_balance_receiver) + float(amount)

    try:
        assert difference_r == float(client_utils.get_balance_user(client_id))
    except KeyError:
        raise Exception(response)


def test_send_cash_many_users(init_node_process, amount_1=35.0, amount_2=10.0):
    from tests.client import utils as client_utils

    address_receivers_1 = client_utils.get_user_address("2")
    address_receivers_2 = client_utils.create_account(client_id="3")

    start_balance_receiver_1 = client_utils.get_balance_user("2")
    start_balance_receiver_2 = client_utils.get_balance_user("3")

    response = client_utils.exec_esc_cmd(INIT_CLIENT_ID, {
        "run": "send_many",
        "wires":
        {
            address_receivers_2: amount_2,
            address_receivers_1: amount_1
        }
    })

    try:
        assert float(response['tx']['deduct']) == amount_1 + amount_2
    except KeyError:
        raise Exception(response)

    end_balance_receiver_1 = client_utils.get_balance_user("2")
    end_balance_receiver_2 = client_utils.get_balance_user("3")

    difference_receiver_1 = float(start_balance_receiver_1) + float(amount_1)
    difference_receiver_2 = float(start_balance_receiver_2) + float(amount_2)

    try:
        assert difference_receiver_1 == float(end_balance_receiver_1)
        assert difference_receiver_2 == float(end_balance_receiver_2)
    except KeyError:
        raise Exception(response)
