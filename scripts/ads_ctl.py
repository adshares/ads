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

    stdout = open(os.path.join(nconf_path, 'stdout'), 'w')
    stderr = open(os.path.join(nconf_path, 'stderr'), 'w')

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
    os.chdir(nconf_path)

    try:
        with open('{0}.pid'.format(DAEMON_BIN_NAME), 'r') as f:
            pid = int(f.read())

        try:
            os.kill(pid, signal.SIGKILL)
            print("ADS node {0} stopped.".format(nconf_path))

        except OSError:
            print("ADS node not {0} killed (maybe not found).".format(nconf_path))

    except IOError:
        print("Pid file not found")


def stop_all():
    # https://stackoverflow.com/a/2241047

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
    os.chdir(nconf_path)

    m = hashlib.md5()
    with open('genesis.json', 'r') as f:
        m.update(f.read())

    print(" Genesis.json md5: {0}".format(m.hexdigest()))

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


def investigate(uconf_path, silent=False):

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
    if not glob(data_dir + '/node*'):
        print("Cannot find nodes data in {0}".format(data_dir))
        sys.exit(1)


def clean_action(data_dir):

    if os.path.exists(data_dir):
        shutil.rmtree(data_dir)
        os.mkdir(data_dir)
        print("ADS node configuration removed.")
    else:
        print("{0} doesn't exist".format(data_dir))


def start_action(data_dir, debug=False, init=False):

    block_time = 512
    if debug:
        block_time = 32
    genesis_time = (int(time.time() + 8) / block_time) * block_time
    print("Genesis start time: ", time.strftime("%Z - %Y/%m/%d, %H:%M:%S", time.localtime(float(genesis_time))))

    for nconf in sorted(glob(data_dir + '/node*')):
        print(nconf)
        start_node(nconf, genesis_time, init)


def stop_action(data_dir):
    for nconf in sorted(glob(data_dir + '/node*')):
        print(nconf)
        stop_node(nconf)
    stop_all()


def nodes_action(data_dir):
    for nconf in sorted(glob(data_dir + '/node*')):
        print(nconf)
        state(nconf)


def network_action(data_dir):
    for uconf in sorted(glob(data_dir + '/user*.00000000')):
        print(uconf)
        if not investigate(uconf):
            sys.exit(1)


def wait_action(data_dir):

    print("Waiting for ADS")

    t = time.time()
    started = False
    while not started:

        if time.time() - t > 300:
            print("ADS start timeout")
            sys.exit(1)

        for uconf in sorted(glob(data_dir + '/user*.00000000')):
            if investigate(uconf, False):
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
    parser.add_argument('--data-dir', default='{0}/ads_data'.format(expanduser('~')), help='Writeable directory with node and accounts configurations.')
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
        check_data(args.data_dir)
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
