from . import exec_esc_cmd
from . import INIT_CLIENT_ID


def test_get_blocks(init_node_process):
    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_blocks', 'from': 0, "to": 0})
    assert len(response['blocks']) > 0


def test_send_and_get_broadcast(init_node_process):
    message = "D69BCCF69C2D0F6CED025A05FA7F3BA687D1603AC1C8D9752209AC2BBF2C4D17"

    response = exec_esc_cmd(INIT_CLIENT_ID, {"run": "broadcast",
                                             "message": message})

    assert response['current_block_time']

    current_time_hex = hex(int(response['current_block_time']))[2:]

    response = exec_esc_cmd(INIT_CLIENT_ID, {'run': 'get_broadcast', 'from': current_time_hex})

    assert response['broadcast'][0]['message'] == message

