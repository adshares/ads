from . import exec_esc_cmd, create_client_env, create_init_client
from . import INIT_CLIENT_ID
from .utils import create_account, get_user_address


def get_balance_user(client_id):
    """
    Function returns user's balance in str format
    """
    response_receiver = exec_esc_cmd(client_id, {"run": "get_me"}, with_get_me=False)
    return response_receiver['account']['balance']


def test_send_cash_one_user(init_node_process, amount=100.0, client_id='2'):
    address_receiver = create_account()

    start_balance_receiver = get_balance_user(client_id)

    message = "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"

    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "send_one",
                                             "address": address_receiver,
                                             'message': message,
                                             "amount": amount})

    assert float(response['tx']['deduct']) == float(amount)

    difference_receiver = float(start_balance_receiver) + float(amount)

    assert difference_receiver == float(get_balance_user(client_id))


def test_send_cash_many_users(init_node_process, amount_1=35.0, amount_2=10.0):
    address_receivers_2 = create_account(client_id="3")
    address_receivers_1 = get_user_address("2")

    start_balance_receiver_1 = get_balance_user("3")
    start_balance_receiver_2 = get_balance_user("2")

    response = exec_esc_cmd(INIT_CLIENT_ID, {
        "run": "send_many",
        "wires": {address_receivers_2: amount_2, address_receivers_1: amount_1}})

    assert float(response['tx']['deduct']) == amount_1 + amount_2

    end_balance_receiver_1 = get_balance_user("2")
    end_balance_receiver_2 = get_balance_user("3")

    difference_receiver_1 = float(start_balance_receiver_1) + float(amount_1)
    difference_receiver_2 = float(start_balance_receiver_2) + float(amount_2)

    assert difference_receiver_1 == float(end_balance_receiver_1)
    assert difference_receiver_2 == float(end_balance_receiver_2)


