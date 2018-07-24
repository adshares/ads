#!/usr/bin/env python
from __future__ import print_function

import argparse
import json
import getpass
import os
import re
import socket
import subprocess
import sys
from copy import copy
from os.path import expanduser

try:
    from urllib2 import urlopen
except ImportError:
    from urllib.request import urlopen


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

    def save(self):
        """
        Save settings to file.
        """

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

    def save(self, genesis):
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

        genesis.save(os.path.join(directory, 'genesis.json'))

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

        :param genesis_file: Genesis filepath or URL
        """

        if genesis_file.startswith('http'):
            genesis_response = urlopen(genesis_file)
            self.genesis = json.loads(genesis_response.read())
        else:
            with open(genesis_file, 'r') as f:
                self.genesis = json.load(f)

        self._genesis_dict()

    def _genesis_dict(self):
        self.nodes = {}
        for index, node in enumerate(self.genesis['nodes']):

            node_ident = self.node_identifier_from_accounts(node)
            if not node_ident:
                node_ident = '{0:04x}'.format(index)

            self.nodes[node_ident] = node

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

    def node_identifier_from_accounts(self, node):
        """
        Get node identifiers from the first account identifier associated with this node.

        :return: List of identifiers.
        """
        for account in node['accounts']:
            addr = account["_address"]
            if re.search('-0{8}-', addr):
                return addr[:4]

    def node_identifier_from_public_key(self, pub_key):
        for node_id in self.nodes.keys():
            if self.nodes[node_id]['public_key'] == pub_key:
                return node_id

    def save(self, filepath):
        with open(filepath, 'w') as f:
            json.dump(self.genesis, f)


def validate_platform(verbose=False):
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

    if verbose:
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


def get_public_key(private_key):
    proc = subprocess.Popen(['ads', '-s'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    output, errors = proc.communicate(private_key)
    match = re.search('Public key: (.*?)\s', errors)
    return match.group(1)


def prepare_node_configuration(identifiers, genesis_data, private_key=False):

    nodes_to_process = []

    if private_key:
        pub_key = get_public_key(private_key)
        node_ident_genesis = genesis_data.node_identifier_from_public_key(pub_key)

        if node_ident_genesis:
            print("Found matching public key in genesis.")
            # Check if private key matches public key in the chosen identifier
            if identifiers and node_ident_genesis != identifiers[0]:
                print("Invalid private key for node: {0}".format(identifiers[0]))
                sys.exit(1)

            # Node id taken from matching public key
            node = genesis_data.nodes[node_ident_genesis]
            node['_nid'] = node_ident_genesis

            print("Configuring nodes: {0}".format(node_ident_genesis))
        else:
            print("No matching public key found in genesis file.")
            # New node (not in genesis)
            if not identifiers:
                print("No node number provided to set up new node. Did you forget --node?")
                sys.exit(1)

            node = {'_nid': identifiers[0], '_secret': private_key, 'accounts': []}
            print("Configuring new node: {0}".format(identifiers[0]))

        nodes_to_process = [node]

    else:
        for k in sorted(genesis_data.nodes.keys()):
            if identifiers and k not in identifiers:
                continue

            node = genesis_data.nodes[k]
            node['_nid'] = k
            nodes_to_process.append(node)

        if nodes_to_process:
            print("Configuring nodes: {0}".format([node['_nid'] for node in nodes_to_process]))

    return nodes_to_process


def configure(config):
    """
    Configure nodes and their first account

    :return:
    """

    genesis_data = GenesisFile(config.genesis)
    if config.node:
        config.node = config.node.split(',')

    nodes_to_process = prepare_node_configuration(config.node, genesis_data, config.private_key)

    if not nodes_to_process:
        print("No nodes to configure.")
        sys.exit(1)

    local_env = {'data_dir': config.data_dir, 'node_interface': config.interface}
    port_offset = 0
    local_peers = []

    for node in nodes_to_process:

        nconf = NodeConfig(local_env)

        nconf.svid = node['_nid']
        nconf.port = config.port + port_offset      # default port: 8001
        nconf.offi = config.client_port + port_offset  # default port: 9001
        nconf.peers = copy(local_peers)

        # Add yourself as peer
        local_peers.append('{0}:{1}'.format(nconf.addr, nconf.port))

        nconf.private_key_file = node['_secret']
        nconf.save(genesis_data)

        if config.user_dirs:
            for account in node['accounts']:

                a_id = '{0}.{1}'.format(node['_nid'], account['_address'][5:13])

                aconf = AccountConfig(a_id, local_env)

                aconf.port = nconf.offi
                aconf.address = account['_address']
                aconf.public_key = account['public_key']
                aconf.private_key = account['_secret']
                aconf.signature = account['_sign']

                aconf.save()
                break

        port_offset += 1


if __name__ == '__main__':

    validate_platform()

    parser = argparse.ArgumentParser(description='Configure ADS nodes.')

    parser.add_argument('--node', default=None, help='Node number')
    parser.add_argument('--private-key', default=None, help='Private key for the node')
    parser.add_argument('--genesis',
                        default='https://raw.githubusercontent.com/adshares/ads-tests/master/qa/config/genesis/genesis-20x20-rf.json',
                        help='Genesis filepath or url')
    parser.add_argument('--data-dir', default='{0}/.adsd'.format(expanduser('~')), help='Writeable working directory.')
    parser.add_argument('--interface', default=get_my_ip(), help='Interface this node is bound to.')
    parser.add_argument('--user-dirs', action='store_true', help='Create account directories.')
    parser.add_argument('--port', default=8001, type=int, help='Node port')
    parser.add_argument('--client-port', default=9001, type=int, help='Node port')

    args = parser.parse_args()

    configure(args)
