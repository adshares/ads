import pytest


def test_block_created(init_node_process, gen_blocks_count=1):
    # Check in log if blocks are created
    blocks_counter = 0
    for line in init_node_process.stdout:
        print(line, blocks_counter)
        if line == b"NEW BLOCK created\n":
            blocks_counter += 1

        if blocks_counter == gen_blocks_count:
            break

    assert gen_blocks_count == blocks_counter


def test_add_node():
    pass


def test_change_node_key():
    pass



