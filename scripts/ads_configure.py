#!/usr/bin/env python
from __future__ import print_function
import argparse
import json
import os
import re
import socket
import sys
from copy import copy
from shutil import copyfile
from os.path import expanduser


class AccountAddressError(Exception):
    pass


def save_config(filepath, settings):
    """
    Save config in ADS format:

    key=value
    (one per line)

    A value can be a list (eg. [val1, val2]). This will be written like this:

    key=val1
    key=val2

    :param filepath: Config file path.
    :param settings: Dictionary of settings.
    :return:
    """
    with open(filepath, 'w') as f:
        for k, v in sorted(settings.items()):
            if isinstance(v, list):
                for seq_el in v:
                    f.write("{0}={1}\n".format(k, seq_el))
            else:
                f.write("{0}={1}\n".format(k, v))


class AccountConfig(object):
    """
    Object for user/account config.
    """

    def __init__(self, identifier, loc_env):
        """
        Initialize the object with default values.

        :param identifier: User account identifier
        :param loc_env: Local node environment (ip, node identifier, etc.)
        """
        self.id = identifier

        self.port = 9001
        self.node_addr = loc_env['node_interface']
        self.address = None
        self.private_key = None

        self.node_env = loc_env

    def validate_address(self):
        """
        Checks if address is correct.

        :return:
        """
        # TODO: checksum verification
        return re.match('[0-9a-fA-F]{4}-[0-9a-fA-F]{8}-[0-9a-fA-FX]{4}', self.address)

    def save(self):
        """
        Save settings to file.
        """

        if not self.validate_address():
            raise AccountAddressError("Account address is not correct.")

        options = {'port': self.port,
                   'host': self.node_addr,
                   'address': self.address,
                   'secret': self.private_key}

        directory = os.path.join(self.node_env['data_dir'], 'user{0}'.format(self.id))

        if not os.path.exists(directory):
            os.makedirs(directory)

        filepath = os.path.join(directory, 'settings.cfg')

        save_config(filepath, options)
        print("{0} Saved settings to: {1}".format(self.address, filepath))


class NodeConfig(object):
    """
    Object for node config.
    """

    def __init__(self, loc_env):
        """
        Initialize the object with default values.

        :param loc_env: Local node environment (ip, node identifier, etc.)
        """
        self.svid = 0
        self.port = 8001
        self.offi = 9001
        self.addr = loc_env['node_interface']

        self.accounts = []

        self.peers = []

        self.node_env = loc_env

        self.public_key_file = None
        self.private_key_file = None

        self.signature = None

    def add_account(self, account_dict):
        """
        Add account associated with this config.

        :param account_dict: Account config from genesis file.
        :return:
        """
        # TODO: validation? logging?
        self.accounts.append(account_dict)

    def save(self, genesis_filepath):
        """
        Save settings to file.
        """

        options = {'svid': int(self.svid, 16),
                   'offi': self.offi,
                   'port': self.port,
                   'addr': self.addr,
                   'peer': self.peers}

        directory = os.path.join(self.node_env['data_dir'], 'node{0}'.format(self.svid))
        if not os.path.exists(directory):
            os.makedirs(directory)

        if not os.path.exists(os.path.join(directory, 'key')):
            os.makedirs(os.path.join(directory, 'key'))

        with open(os.path.join(directory, 'key', 'key.txt'), 'w') as f:
            f.write(self.private_key_file)

        copyfile(genesis_filepath, os.path.join(directory, 'genesis.json'))

        filepath = os.path.join(directory, 'options.cfg')

        save_config(filepath, options)
        print("{0} Saved options to: {1}".format(self.svid, filepath))


class GenesisFile(object):
    """
    Genesis file object, with helper functions.
    """
    def __init__(self, genesis_file):
        """
        Read the file in.

        :param genesis_file: Genesis filepath
        """
        with open(genesis_file, 'r') as f:
            self.genesis = json.load(f)

        self.nodes = self.genesis['nodes']

    def node_count(self):
        """
        Get number of nodes in genesis file

        :return: Number of nodes
        """
        return len(self.nodes)

    def node_data(self, node_number):
        """
        Get keys and signatures associated with this node.

        :param node_number: Node index number.
        :return: Tuple of private key, public key and signature.
        """
        node_config = self.nodes[node_number]
        return node_config['_secret'], node_config['public_key'], node_config['_sign']

    def node_identifiers(self):
        """
        Get node identifiers from the first account identifier associated with this node.

        :return: List of identifiers.
        """
        ids = []
        for node in self.nodes:
            for account in node['accounts']:
                addr = account["_address"]
                if re.search('-0{8}-', addr):
                    ids.append(addr[:4])

        return ids


def validate_platform():
    """
    Check if you can run ADS. Currently supported only on 64 bit Linux.

    :return: Exits with status code 1 if platform is not valid, otherwise just prints out some system information.
    """

    # Platform check, linux or windows, or something else
    if not sys.platform.startswith('linux'):
        print("This platform is not supported for this configuration script.")
        print("I need 'linux', but this is '{0}'. See manual configuration instructions.".format(sys.platform))
        sys.exit(1)

    # Architecture check
    try:
        uname_data = os.uname()
        if uname_data[4] != 'x86_64':
            print("This architecture is not supported. I need *x86_64*.")
            sys.exit(1)
    except AttributeError:
        print("Can't detect the architecture. I need a *nix system to do that.")
        sys.exit(1)

    print("Detected: {0} {1}".format(sys.platform, os.uname()[4]))


def get_my_ip(remote_ip="8.8.8.8", remote_port=53):
    """
    Try to connect to remote ip, to get local interface address. Defaults to google DNS.

    :param remote_ip: Remote ip to connect to.
    :param remote_port: Remote port to connect to.
    :return: Local interface ip.
    """

    # https://stackoverflow.com/a/166589

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.connect((remote_ip, remote_port))
    my_ip = s.getsockname()[0]
    s.close()

    return my_ip


def configure(data_dir, interface, identifiers, genesis_file):
    """
    Configure nodes and their first account

    :return:
    """
    local_env = {'data_dir': data_dir, 'node_interface': interface}

    genesis_data = GenesisFile(genesis_file)

    if identifiers:
        chosen_identifiers = set(identifiers.split(',')).intersection(set(genesis_data.node_identifiers()))
        print("Configuring nodes: {0}".format(identifiers))
    else:
        print("Configuring all nodes found in the genesis file.")

    node_numerical_identifier = 1
    local_peers = []

    for index, node_identifier in enumerate(genesis_data.node_identifiers()):

        if identifiers and node_identifier not in chosen_identifiers:
            continue

        nconf = NodeConfig(local_env)

        nconf.svid = node_identifier
        nconf.port = 8000 + node_numerical_identifier
        nconf.offi = 9000 + node_numerical_identifier
        nconf.peers = copy(local_peers)

        # Add yourself as peer
        local_peers.append('{0}:{1}'.format(nconf.addr, nconf.port))

        node = genesis_data.nodes[index]

        nconf.public_key_file = node['public_key']
        nconf.private_key_file = node['_secret']

        nconf.signature = node['_sign']

        nconf.save(genesis_file)

        for account in node['accounts']:

            a_id = '{0}.{1}'.format(node_identifier, account['_address'][5:13])

            aconf = AccountConfig(a_id, local_env)

            aconf.port = nconf.offi
            aconf.address = account['_address']
            aconf.public_key = account['public_key']
            aconf.private_key = account['_secret']
            aconf.signature = account['_sign']

            aconf.save()
            break

        if len(chosen_identifiers) > 1:
            node_numerical_identifier += 1


if __name__ == '__main__':

    validate_platform()

    parser = argparse.ArgumentParser(description='Configure ADS nodes.')

    parser.add_argument('genesis', default=None, help='Genesis file')
    parser.add_argument('--identifiers', help='Configure only these specific node identifiers.')
    parser.add_argument('--data-dir', default='{0}/ads_data'.format(expanduser('~')), help='Writeable directory with node and accounts configurations.')
    parser.add_argument('--interface', default=get_my_ip(), help='Interface this node is bound to.')

    args = parser.parse_args()

    configure(args.data_dir, args.interface, args.identifiers, args.genesis)
