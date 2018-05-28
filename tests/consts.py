import os

HPX_ROOT = os.path.dirname(os.path.dirname(__file__))
HPX_PROJECT_CONFIG = os.environ.get('DCMAKE_PROJECT_CONFIG', 'hpx')

DCMAKE_PROJECT_CONFIG_D = "{}d".format(HPX_PROJECT_CONFIG)
BIN_PATH = os.path.join(HPX_ROOT, "build", 'esc',
                        HPX_PROJECT_CONFIG)
D_BIN_PATH = os.path.join(HPX_ROOT, "build", 'escd',
                          DCMAKE_PROJECT_CONFIG_D)


INIT_NODE_ID = '1'
INIT_NODE_OFFICE_PORT = 8003
INIT_NODE_SERVER_PORT = 8005


INIT_CLIENT_ID = '0'
FIRST_CLIENT = '1'
INIT_CLIENT_ADDRESS = "0001-00000000-XXXX"
INIT_CLIENT_SECRET = "14B183205CA661F589AD83809952A692DF" \
                     "A48F5D490B10FD120DA7BF10F2F4A0"

BLOCK_TIME = 0

ACCOUNT_FIELDS = ['id', 'address', 'node', 'msid', 'time', 'date',
                  'status', 'paired_node', 'paired_id', 'paired_address',
                  'local_change', 'remote_change', 'balance', 'public_key',
                  'hash']

ACCOUNT_INIT_USER_FIELDS = ['id', 'address', 'node', 'msid', 'time', 'date',
                            'status', 'paired_node', 'paired_id',

                            'local_change', 'remote_change', 'balance',
                            'public_key', 'hash']

TX_FIELDS = ['data', 'account_hashin', 'account_hashout',
             'deduct', 'fee', 'account_msid', 'account_public_key']

TRANSACTION_FIELDS = ['id', 'block_id', 'node_id', 'node_msid',
                      'position', 'len', 'hash_path_len', 'hexstring',
                      'hashpath', 'type', 'abank', 'auser', 'amsid',
                      'ttime', 'bbank', 'buser', 'amount', 'message'
                      'signature']

BROADCAST_FIELDS = ['block_time', 'block_date', 'node', 'account',
                    'address', 'account_msid', 'time', 'date', 'message',
                    'data', 'signature', 'input_hash', 'public_key',
                    'verify', 'node_msid', 'node_mpos', 'id']

BLOCK_FIELDS = ['id', 'time', 'message_count', 'oldhash', 'minhash', 'msghash',
                'nodhash', 'viphash', 'nowhash', 'vote_yes', 'vote_no',
                'vote_total', 'node_count', 'dividend_balance', 'devident_pay']

BLOCK_NODE_FIELDS = ['id', 'public_key', 'public_key', 'message_hash', 'msid',
                     'mtim', 'balance', 'status', 'account_count', 'port',
                     'ipv4']

LOG_BASE_FIELDS = ['type', 'date', 'type_no', 'confirmed']


GET_BLOCK_FIELDS = ['block_time', 'block_time_hex', 'block_prev', 'block_next',
                    'block']

