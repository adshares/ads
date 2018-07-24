#!/usr/bin/env python
from __future__ import print_function
import os
import subprocess
from glob import glob
import argparse
import shutil
import json
import time
import signal
import sys
import hashlib
import psutil
from os.path import expanduser


DAEMON_BIN_NAME = 'adsd'
CLIENT_BIN_NAME = 'ads'


def start_node(nconf_path, genesis_time, init=False):
    """
    Start a background node process. Modifies the local genesis file by setting the `genesis_time`.
    On success, creates a pid file with the process number.

    :param nconf_path: Path to node configuration directory.
    :param genesis_time: Genesis time of the network.
    :param init: Set it to true, if this is the first node of the network.
    :return:
    """

    genesis_path = os.path.join(nconf_path, 'genesis.json')

    with open(genesis_path, 'r') as f:
        genesis = json.load(f)

    genesis['config'] = dict()
    genesis['config']['start_time'] = genesis_time

    with open(genesis_path, 'w') as f:
        json.dump(genesis, f)

    cmd = [
        DAEMON_BIN_NAME,
        '--genesis={0}'.format(genesis_path),
        '--work-dir={0}'.format(nconf_path)
    ]

    if init:
        cmd += ['--init=true']

    node_name = os.path.basename(nconf_path)

    stdout = open(os.path.join(nconf_path, '{0}.log'.format(node_name)), 'w')
    stderr = open(os.path.join(nconf_path, '{0}.error.log'.format(node_name)), 'w')

    proc = subprocess.Popen(cmd, stdout=stdout, stderr=stderr)

    try:
        os.kill(proc.pid, 0)

        pidfile = os.path.join(nconf_path, '{0}.pid'.format(DAEMON_BIN_NAME))
        with open(pidfile, 'w') as f:
            f.write(str(proc.pid))

        print("Process started: ", time.strftime("%Z - %Y/%m/%d, %H:%M:%S", time.localtime(time.time())))
    except OSError:
        print("Server not started.")
        sys.exit(1)

    print("ADS node {0} started.".format(nconf_path))


def stop_node(nconf_path):
    """
    Stop the node by sending a SIGKILL to the process. Process ID is read from the pid file.

    :param nconf_path: Path to directory holding the pid file.
    :return:
    """

    pid_filepath = os.path.join(nconf_path, '{0}.pid'.format(DAEMON_BIN_NAME))
    try:
        with open(pid_filepath, 'r') as f:
            pid = int(f.read())

        try:
            os.kill(pid, signal.SIGKILL)
            os.remove(pid_filepath)
            print("ADS node {0} stopped.".format(nconf_path))

        except OSError:
            print("ADS node {0} not killed for node config.".format(nconf_path))

    except IOError:
        print("Pid file not found")


def stop_all():
    """
    Kill all process matching the daemon binary name.
    From: https://stackoverflow.com/a/2241047

    :return:
    """

    name = DAEMON_BIN_NAME

    for p in psutil.process_iter():
        name_, exe, cmdline = "", "", []
        try:
            name_ = p.name()
            cmdline = p.cmdline()
            exe = p.exe()
        except (psutil.AccessDenied, psutil.ZombieProcess):
            pass
        except psutil.NoSuchProcess:
            continue
        if name == name_ or (cmdline and './{0}'.format(name) in cmdline) or os.path.basename(exe) == name:
            print("Found process {0}: ".format(p.pid))
            print(name_, cmdline, exe)

            try:
                os.kill(p.pid, signal.SIGKILL)
                print("Killed process {0}".format(p.pid))
            except OSError:
                print("Process {0} not killed".format(p.pid))
                sys.exit(1)


def state(nconf_path):
    """
    Get node state with the following information:
    * genesis.json hash
    * check if process is alive by trying to ping it, using the pid file number.

    :param nconf_path: Path to directory with the node's genesis.json and pid file.
    :return: Exists system with status code 1 if pid is not found.
    """

    m = hashlib.md5()
    with open(os.path.join(nconf_path, 'genesis.json'), 'r') as f:
        m.update(f.read())

    print(" Genesis.json md5: {0}".format(m.hexdigest()))

    with open(os.path.join(nconf_path, 'genesis.json'), 'r') as f:
        genesis = json.load(f)

    print("Genesis start time: ", time.strftime("%Z - %Y/%m/%d, %H:%M:%S", time.localtime(float(genesis['config']['start_time']))))

    try:
        pidfile = os.path.join(nconf_path, '{0}.pid'.format(DAEMON_BIN_NAME))
        with open(pidfile, 'r') as f:
            pid = int(f.read())
    except IOError:
        print("Pid file not found")
        sys.exit(1)

    try:
        os.kill(pid, 0)
        print("# Node is UP with pid: {0}".format(pid))
    except OSError:
        print("# Node is DOWN! (supposed pid: {0}".format(pid))


def read_nconf(options_filepath):
    with open(options_filepath, 'r') as f:
        options = f.readlines()

    for opt in options:
        key, value = opt.split('=')
        if key == 'offi':
            port = value.trim()
        elif key == 'addr':
            host = value.trim()

    if port and host:
        return host, port


def investigate_node(nconf):

    options = read_nconf(nconf)
    if not options:
        print("Invalid options")
        sys.exit(1)

    try:
        cmd = 'echo \'{"run":"get_me"}\''
        cmd += ' | {0} --work-dir=. --secret=123 -P{1} -H{2}'.format(CLIENT_BIN_NAME, options[0], options[1])

        with open(os.devnull, 'w') as devnull:
            output = subprocess.check_output(cmd, stderr=devnull, shell=True)

    except subprocess.CalledProcessError as e:
        print(e)
        return False

    json_out = json.loads(output)

    if 'current_block_time' and 'previous_block_time' in json_out.keys():
        return True

    return False


def investigate(uconf_path, silent=False):
    """
    Check the node status by sending it a message from a local account.
    Prints out:
    * ID of last block
    * Hash of last block
    * Time since last message.

    :param uconf_path: Path to directory with the account config.
    :param silent: Set to True to supress some outputs.
    :return: True on receiving a response, False on failure.
    """
    try:
        cmd = 'echo \'{"run":"get_block"}\''
        cmd += ' | {0} --work-dir={1}'.format(CLIENT_BIN_NAME, uconf_path)

        #if silent:
        with open(os.devnull, 'w') as devnull:
            output = subprocess.check_output(cmd, stderr=devnull, shell=True)
        #else:
        #    output = subprocess.check_output(cmd, shell=True)
    except subprocess.CalledProcessError as e:
        print(e)
        return False

    json_out = json.loads(output)

    try:
        last_block_id, last_block_hash = json_out['block']['id'], json_out['block']['nowhash']
        mtimes = sorted([int(n['mtim']) for n in json_out['block']['nodes']])
        if not silent:
            print(" Last block ID: {0}".format(last_block_id))
            print(" Last block hash: {0}".format(last_block_hash))
            print(" Last message: {0:.3f} seconds ago.".format(time.time() - mtimes[-1]))
        return True
    except KeyError:
        print(" No block data.")


def check_data(data_dir):
    """
    Check for node data in `data_dir`.

    :param data_dir: Directory with the node configuration.
    :return: Exists system with 1 if config not found.
    """
    if not glob(data_dir + '/node*'):
        print("Cannot find nodes data in {0}".format(data_dir))
        sys.exit(1)


def clean_action(data_dir):
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


def start_action(data_dir, debug=False, init=False):
    """
    Create the genesis time and start the node in background.

    :param data_dir: Directory with the node configuration.
    :param debug: Set this to True for shorter block time. This will result in faster start time.
    :param init: Set this to True if node is to be initialized.
    :return:
    """
    block_time = 512
    if debug:
        block_time = 32
    genesis_time = (int(time.time() + 8) / block_time) * block_time
    print("Genesis start time: ", time.strftime("%Z - %Y/%m/%d, %H:%M:%S", time.localtime(float(genesis_time))))

    for nconf in sorted(glob(data_dir + '/node*')):
        print(nconf)
        start_node(nconf, genesis_time, init)


def stop_action(data_dir):
    """
    Try to stop nodes gently. Then try to kill all still living.

    :param data_dir: Data directory with node configurations.
    :return:
    """
    for nconf in sorted(glob(data_dir + '/node*')):
        print(nconf)
        stop_node(nconf)
    stop_all()


def nodes_action(data_dir):
    """
    Get the state of the nodes from static resources.

    :param data_dir: Data directory with node configurations.
    :return:
    """
    for nconf in sorted(glob(data_dir + '/node*')):
        print(nconf)
        state(nconf)


def network_action(data_dir):
    """
    Get the state of the nodes from asking the node for information.

    :param data_dir: Data directory with node configurations.
    :return:
    """
    for uconf in sorted(glob(data_dir + '/user*.00000000')):
        print(uconf)
        if not investigate(uconf):
            sys.exit(1)


def wait_action(data_dir, timeout=300):
    """
    Wait till the node is alive by waiting for a response.

    Timeout can be set.

    :param data_dir: Directory with account configuration used to connect to the nodes.
    :param timeout: Timeout, in seconds.
    :return:
    """
    print("Waiting for ADS")

    t = time.time()
    started = False
    while not started:

        if time.time() - t > timeout:
            print("ADS start timeout")
            sys.exit(1)

        user_dirs = glob(data_dir + '/user*.00000000')

        if user_dirs:
            for uconf in sorted(user_dirs):
                if investigate(uconf, False):
                    started = True
                    break
                else:
                    print("Waiting for adsd")
                    time.sleep(1)
        else:
            node_options = glob(data_dir + '/node*/options.cfg')
            if not node_options:
                node_options = [os.path.join(data_dir, 'options.cfg')]
            for nconf in sorted(node_options):
                if investigate_node(nconf):
                    started = True
                    break
                else:
                    print("Waiting for adsd")
                    time.sleep(1)

    print("ADS started")


if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='Start ADS nodes.')
    parser.add_argument('action', choices=['start', 'stop', 'clean', 'nodes', 'network', 'wait'])
    parser.add_argument('-i', '--init', action='store_true', help='Initialize the first network node.')
    parser.add_argument('--data-dir', default=expanduser('~/.adsd'), help='Writeable working directory.')
    parser.add_argument('-w', '--wait', action='store_true', help='Wait and make sure the daemon is working.')
    parser.add_argument('-d', '--debug', action='store_true', help='Enable debug mode')

    args = parser.parse_args()

    if args.action == 'network':
        check_data(args.data_dir)
        network_action(args.data_dir)
    elif args.action == 'wait':
        check_data(args.data_dir)
        wait_action(args.data_dir)
    elif args.action == 'clean':
        clean_action(args.data_dir)
    elif args.action == 'start':
        check_data(args.data_dir)
        start_action(args.data_dir, args.debug, args.init)
    elif args.action == 'stop':
        stop_action(args.data_dir)
    elif args.action == 'nodes':
        check_data(args.data_dir)
        nodes_action(args.data_dir)

    if args.wait:
        wait_action(args.data_dir)
