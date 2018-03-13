from . import clean_node_dir, node_process_ready
from . import INIT_NODE_ID


def test_first_node_start(gen_blocks_count=100):
    clean_node_dir(INIT_NODE_ID)
    process = node_process_ready(INIT_NODE_ID)

    blocks_counter = 0
    for line in process.stdout:
        print(line)
        if line == b"NEW BLOCK created\n":
            blocks_counter+=1

        if blocks_counter == gen_blocks_count:
            break

    process.terminate()
    assert blocks_counter == gen_blocks_count


def test_add_node():
    pass


def test_change_node_key():
    pass



