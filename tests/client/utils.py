import os
import shutil
import time

from tests.consts import INIT_CLIENT_ID, INIT_NODE_OFFICE_PORT, INIT_CLIENT_ADDRESS, INIT_CLIENT_SECRET
from tests.utils import exec_esc_cmd, generate_keys


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


def get_client_dir(client_id):
    from tests.node.utils import get_node_path_dir
    return get_node_path_dir(client_id, "client")


def clean_client_dir(client_id):
    client_id = str(client_id)
    client_dir = get_client_dir(client_id)
    shutil.rmtree(client_dir, ignore_errors=True)


def get_user_address(client_id):
    response = exec_esc_cmd(client_id, {"run": "get_me"}, with_get_me=False)
    try:
        return response['account']['address']
    except KeyError:
        raise Exception(response)


def get_balance_user(client_id):
    """
    Function returns user's balance in str format
    """
    response_receiver = exec_esc_cmd(client_id, {"run": "get_me"}, with_get_me=False)
    try:
        return response_receiver['account']['balance']
    except KeyError as err:
        raise KeyError(err, response_receiver)


def update_user_env(client_id, address):
    """
    Function updates user's data after create a new user
    """

    new_secret, new_pub_key, signature = generate_keys()

    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_account', 'address': address}, with_get_me=False)

    balance = response['account']['balance']
    if float(balance) < 1:
        raise ValueError('Too lowe balance {} for change PKEY'.format(balance))

    response = exec_esc_cmd(INIT_CLIENT_ID, {
        "run": "change_account_key",
        "pkey": new_pub_key, "signature": signature},
        cmd_extra=['--address', address])

    create_client_env(client_id,
                      INIT_NODE_OFFICE_PORT,
                      address=address,
                      secret=new_secret)


def create_account(client_id="2", node="0001"):
    # As INIT user, create client with client_id
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "create_account", "node": node})
    try:
        address = response['new_account']['address']
    except KeyError as err:
        raise KeyError(err, response)

    time_start = time.time()
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': "get_accounts",  "node": node}, with_get_me=False)
    count_users = len(response.get('accounts'))

    message = "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"
    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "send_one",
                                             "address": address,
                                             'message': message,
                                             "amount": 20})

    while True:
        response = exec_esc_cmd(INIT_CLIENT_ID, {'run': "get_accounts", "node": node}, with_get_me=False)
        accounts = len(response.get('accounts')) if response.get('accounts') else 0
        if accounts > count_users:
            break
        time.sleep(10)
        assert time.time() - time_start < 70

    update_user_env(client_id, address)

    return address