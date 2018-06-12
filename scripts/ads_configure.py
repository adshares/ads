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


class AccountAddressError(Exception):
    pass


def save_config(filepath, settings):
    with open(filepath, 'w') as f:
        for k, v in sorted(settings.items()):
            if isinstance(v, list):
                for seq_el in v:
                    f.write("{0}={1}\n".format(k, seq_el))
            else:
                f.write("{0}={1}\n".format(k, v))


class AccountConfig(object):

    def __init__(self, address, node_env, identifier):
        self.port = 9001
        self.node_addr = get_my_ip()
        self.address = address
        self.private_key = None
        self.node_env = node_env
        self.id = identifier

    def validate_address(self):
        return re.match('[0-9a-fA-F]{4}-[0-9a-fA-F]{8}-[0-9a-fA-FX]{4}', self.address)

    def save(self):

        if not self.validate_address():
            raise AccountAddressError("Account address is not correct.")

        options = {'port': self.port,
                   'host': self.node_addr,
                   'address': self.address,
                   'secret': self.private_key}

        directory = os.path.join(self.node_env['data_dir'], 'user{0}'.format(self.id))

        if not os.path.exists(directory):
            os.makedirs(directory)

        if not os.path.exists(os.path.join(directory, "esc")):
            os.symlink(self.node_env['esc_bin'], os.path.join(directory, "esc"))

        filepath = os.path.join(directory, 'settings.cfg')

        save_config(filepath, options)
        print("Saved account settings file to: {0}".format(filepath))


class NodeConfig(object):

    def __init__(self, node_env):

        self.svid = 0
        self.port = 8001
        self.offi = 9001
        self.addr = get_my_ip()

        self.accounts = []

        self.peers = []

        self.node_env = node_env

        self.public_key_file = None
        self.private_key_file = None

        self.signature = None

    def add_account(self, account_dict):
        # TODO: validation? logging?
        self.accounts.append(account_dict)

    def save(self, genesis_filepath):

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

        if not os.path.exists(os.path.join(directory, "escd")):
            os.symlink(self.node_env['escd_bin'], os.path.join(directory, "escd"))

        copyfile(genesis_filepath, os.path.join(directory, 'genesis.json'))

        filepath = os.path.join(directory, 'options.cfg')

        save_config(filepath, options)
        print("Saved node file to: {0}".format(filepath))


class GenesisFile(object):

    def __init__(self, genesis_file):

        with open(genesis_file, 'r') as f:
            self.genesis = json.load(f)

    def node_count(self):
        return len(self.genesis['nodes'])

    def node_data(self, node_number):
        node_config = self.genesis['nodes'][node_number]
        return node_config['_secret'], node_config['public_key'], node_config['_sign']

    def node_identifiers(self):
        ids = []
        for node in self.genesis['nodes']:
            for account in node['accounts']:
                addr = account["_address"]
                if re.search('-0{8}-', addr):
                    ids.append(addr[:4])

        return ids


def get_user_input(ask=True):

    node_env = {'data_dir': '/ads_data',
                'esc_bin': '/ads/esc/esc',
                'escd_bin': '/ads/escd/escd',
                'node_ip': get_my_ip()}

    description = {'data_dir': 'Writeable directory with node and accounts configurations.',
                   'esc_bin': 'Filepath to executable for esc client',
                   'escd_bin': 'Filepath to executable for esc daemon',
                   'node_ip': 'Ip of this node'}

    if not ask:
        return node_env

    print("Please provide the manual configuration.")
    print("Default values are provided in brackets, you can accept them, by pressing Enter.")

    user_node_env = {}
    for k, v in sorted(node_env.items()):

        print('#', description[k])

        val = raw_input("[{0}]:".format(v))
        if not val:
            val = v

        user_node_env[k] = val

    for k, v in sorted(user_node_env.items()):
        print('##', description[k])
        print('#', v)

    return user_node_env


def validate_platform():

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


def get_my_ip():

    # https://stackoverflow.com/a/166589

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # Try to connect to Google DNS, to get the local address
    s.connect(("8.8.8.8", 80))
    my_ip = s.getsockname()[0]
    s.close()

    return my_ip


if __name__ == '__main__':

    validate_platform()

    parser = argparse.ArgumentParser(description='Configure HPX.')

    parser.add_argument('genesis', default=None, help='Genesis file')
    parser.add_argument('--ask', default=False, action='store_true', help='Ask for values, don\'t use defaults.')
    parser.add_argument('--identifiers', help='Configure only these specific node identifiers.')

    args = parser.parse_args()

    genesis_data = GenesisFile(args.genesis)

    local_env = get_user_input(args.ask)

    node_numerical_identifier = 0
    node_identifiers = genesis_data.node_identifiers()
    local_peers = []

    if args.identifiers:
        chosen_identifiers = set(args.identifiers.split(',')).intersection(set(genesis_data.node_identifiers()))
        print("Configuring nodes: {0}".format(args.identifiers))

    for index, node_identifier in enumerate(genesis_data.node_identifiers()):

        node_numerical_identifier += 1

        if args.identifiers and node_identifier not in chosen_identifiers:
            continue

        nconf = NodeConfig(local_env)

        nconf.svid = node_identifier
        nconf.port = 8000 + node_numerical_identifier
        nconf.offi = 9000 + node_numerical_identifier
        nconf.addr = get_my_ip()
        nconf.peers = copy(local_peers)

        # Add yourself as peer
        local_peers.append('{0}:{1}'.format(nconf.addr, nconf.port))

        node = genesis_data.genesis['nodes'][index]

        nconf.public_key_file = node['public_key']
        nconf.private_key_file = node['_secret']

        nconf.signature = node['_sign']

        nconf.save(args.genesis)

        for account in node['accounts']:

            a_id = '{0}.{1}'.format(node_identifier, account['_address'][5:13])

            aconf = AccountConfig(nconf.addr, local_env, a_id)

            aconf.port = nconf.offi
            aconf.node_addr = nconf.addr
            aconf.address = account['_address']

            aconf.public_key = account['public_key']
            aconf.private_key = account['_secret']
            aconf.signature = account['_sign']

            aconf.save()
