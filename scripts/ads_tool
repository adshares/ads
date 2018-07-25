#!/usr/bin/env python
import argparse
import getpass
import json
import os
import re
import shutil
import signal
import socket
import subprocess
import sys
import time

try:
    from urllib2 import urlopen
except ImportError:
    from urllib.request import urlopen

DAEMON_BIN_NAME = 'adsd'


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
            node_ident = '{0:04x}'.format(index)
            self.nodes[node_ident] = node

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


def prepare_node_configuration(node_id, genesis_data, private_key, node_ident_genesis=None):

    if node_ident_genesis:
        print("Found matching public key in genesis.")
        # Check if private key matches public key in the chosen identifier
        if node_id and node_ident_genesis != node_id:
            print("Invalid private key for node: {0}".format(node_id))
            sys.exit(1)

        # Node id taken from matching public key
        node = genesis_data.nodes[node_ident_genesis]
        node['_nid'] = node_ident_genesis

        print("Configuring nodes: {0}".format(node_ident_genesis))
    else:
        print("No matching public key found in genesis file.")
        # New node (not in genesis)
        if not node_id:
            print("No node number provided to set up new node. Did you forget --node?")
            sys.exit(1)

        node = {'_nid': node_id, '_secret': private_key, 'accounts': []}
        print("Configuring new node: {0}".format(node_id))

    return node


def configure(config, genesis_data, node_ident_genesis=None):
    """
    Configure nodes and their first account

    :return:
    """
    node = prepare_node_configuration(config.node, genesis_data, config.private_key, node_ident_genesis)

    if not node:
        print("No nodes to configure.")
        sys.exit(1)

    if not os.path.exists(config.data_dir):
        os.makedirs(config.data_dir)

    genesis_data.save(os.path.join(config.data_dir, 'genesis.json'))

    private_key_dir = os.path.join(config.data_dir, 'key')

    if not os.path.exists(private_key_dir):
        os.makedirs(private_key_dir)

    os.chmod(private_key_dir, 0700)

    with open(os.path.join(private_key_dir, 'key.txt'), 'w') as f:
        f.write(node['_secret'])

    os.chmod(os.path.join(private_key_dir, 'key.txt'), 0600)

    nconf = {'svid': node['_nid'],
             'port': 6510,
             'offi': 6511,
             'addr': config.interface}

    filepath = os.path.join(config.data_dir, 'options.cfg')

    save_config(filepath, nconf)
    print("{0} Saved options to: {1}".format(node['_nid'], filepath))


def get_public_key(private_key):
    proc = subprocess.Popen(['ads', '-s'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    output, errors = proc.communicate(private_key)
    match = re.search('Public key: (.*?)\s', errors)
    if match:
        return match.group(1)


def get_nodeid_from_genesis(genesis_data, private_key):
    pub_key = get_public_key(private_key)
    return genesis_data.node_identifier_from_public_key(pub_key)


def action_signal(data_dir, chosen_signal, name):
    """
    Stop the node by sending a SIGKILL to the process. Process ID is read from the pid file.

    :param data_dir: Path to directory holding the pid file.
    :return:
    """
    pid = get_daemon_pid(data_dir)
    if not pid:
        print("Daemon not running.")
    else:
        try:
            os.kill(pid, chosen_signal)
            print("ADS node {0} {1} was successful.".format(data_dir, name))
        except OSError:
            print("ADS node {0} {1} has failed.".format(data_dir, name))


def action_configure(args):
    
    args.private_key = getpass.getpass("Node's private key: ")
    print "Default values are in []. Press enter to choose them."

    node_ident_genesis = get_nodeid_from_genesis(genesis_data, args.private_key)
    if node_ident_genesis:
        print "Public key for your private key found in genesis file. Associated node identifier is {0}.".format(node_ident_genesis)
        args.node = raw_input("Node identifier [{0}]: ".format(node_ident_genesis))
        if not args.node:
            args.node = node_ident_genesis
    else:
        args.node = raw_input("Node identifier: ")

    args.interface = raw_input("Node interface [{0}]): ".format(get_my_ip()))
    if not args.interface:
        args.interface = get_my_ip()

    configure(args, genesis_data, node_ident_genesis)


def get_daemon_pid(data_dir):
    try:
        pid = subprocess.check_output(['pgrep', '-f', '{0}.*--work-dir={1}'.format(DAEMON_BIN_NAME, data_dir)])
        return int(pid)
    except subprocess.CalledProcessError:
        return None


def action_start(data_dir):
    """
    Start a background node process. Modifies the local genesis file by setting the `genesis_time`.
    On success, creates a pid file with the process number.

    :param args.data_dir: Path to node configuration directory.
    :param genesis_time: Genesis time of the network.
    :param init: Set it to true, if this is the first node of the network.
    :return:
    """

    genesis_path = os.path.join(data_dir, 'genesis.json')

    cmd = [
        DAEMON_BIN_NAME,
        '--genesis={0}'.format(genesis_path),
        '--work-dir={0}'.format(data_dir)
    ]

    stdout = open(os.path.join(data_dir, '{0}.log'.format(DAEMON_BIN_NAME)), 'w')
    stderr = open(os.path.join(data_dir, '{0}.error.log'.format(DAEMON_BIN_NAME)), 'w')

    proc = subprocess.Popen(cmd, stdout=stdout, stderr=stderr)

    try:
        os.kill(proc.pid, 0)
        print("Process started: ", time.strftime("%Z - %Y/%m/%d, %H:%M:%S", time.localtime(time.time())))
    except OSError:
        print("Server not started.")
        sys.exit(1)

    print("ADS node {0} started.".format(data_dir))


def action_clean(data_dir):
    """
    Remove and recreate the whole ADS data directory.

    :param data_dir: Directory to clean up.
    :return:
    """
    if os.path.exists(data_dir):
        shutil.rmtree(data_dir)
        os.mkdir(data_dir)
        print("ADS node configuration removed.")
    else:
        print("{0} doesn't exist".format(data_dir))


if __name__ == '__main__':

    validate_platform()
    
    genesis_data = GenesisFile(
        'https://raw.githubusercontent.com/adshares/ads-tests/master/qa/config/genesis/genesis-20x20-rf.json')

    # Support for sudo
    default_datapath = os.path.join(os.path.expanduser("~" + os.environ["USER"]), ".adsd")

    parser = argparse.ArgumentParser(description='Tools for ADS nodes.')
    parser.add_argument('action', choices=['start', 'stop', 'configure', 'clean', 'restart'])
    parser.add_argument('-f', '--force-stop', action='store_true')
    parser.add_argument('--data-dir', default=default_datapath, help='Writeable working directory.')

    args = parser.parse_args()

    if args.action == 'start':
        action_start(args.data_dir)
    elif args.action == 'stop':
        if args.force_stop:
            action_signal(args.data_dir, signal.SIGKILL, 'forced stop')
        else:
            action_signal(args.data_dir, signal.SIGTERM, 'stop')
    elif args.action == 'restart':
        action_signal(args.data_dir, signal.SIGUSR1, 'restart')
    elif args.action == 'clean':
        action_signal(args.data_dir, signal.SIGKILL, 'forced stop for cleaning')
        action_clean(args.data_dir)
    elif args.action == 'configure':
        action_configure(args)
