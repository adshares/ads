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
import socket


DAEMON_BIN_NAME = 'escd'
CLIENT_BIN_NAME = 'esc'


def start_node(nconf_path, genesis_time, init=False):

    os.chdir(nconf_path)

    with open('genesis.json', 'r') as f:
        genesis = json.load(f)

    genesis['config'] = dict()
    genesis['config']['start_time'] = genesis_time

    with open('genesis.json', 'w') as f:
        json.dump(genesis, f)

    cmd = ['./{0}'.format(DAEMON_BIN_NAME), '--genesis=genesis.json']

    if init:
        cmd += ['--init=true']

    stdout = open(os.path.join(nconf_path, 'stdout'), 'w')
    stderr = open(os.path.join(nconf_path, 'stderr'), 'w')

    proc = subprocess.Popen(cmd, stdout=stdout, stderr=stderr)

    try:
        os.kill(proc.pid, 0)
        with open('{0}.pid'.format(DAEMON_BIN_NAME), 'w') as f:
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
        with open('{0}.pid'.format(DAEMON_BIN_NAME)) as f:
            pid = int(f.read())
    except IOError:
        print("Pid file not found")
        sys.exit(1)

    try:
        os.kill(pid, 0)
        print("# Node is UP with pid: {0}".format(pid))
    except OSError:
        print("# Node is DOWN! (supposed pid: {0}".format(pid))


def investigate(uconf_path, silent = False):
    os.chdir(uconf_path)

    with open(os.devnull, 'w') as devnull:
        output = subprocess.check_output('echo -n \'{"run":"get_block"}\' | ./' + CLIENT_BIN_NAME, stderr=devnull, shell=True)

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
        if not silent:
            print(" No block data.")
        return False

def check_data(data_dir):
      if len(glob(data_dir + '/node*')) == 0:
            print("Cannot find nodes data in {0}".format(data_dir))
            sys.exit(1)

def clean_action(data_dir):

    if os.path.exists(data_dir):
        shutil.rmtree(data_dir)
        os.mkdir(data_dir)
        print("ADS node configuration removed.")
    else:
        print("{0} doesn't exist".format(data_dir))


def start_action(data_dir, init = False):

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
            if investigate(uconf, True):
                started = True
                break
            else:
                print("Waiting for escd")
                time.sleep(1)

    print("ADS started")

if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='Start ADS nodes.')
    parser.add_argument('action', choices=['start', 'stop', 'clean', 'nodes', 'network', 'wait'])
    parser.add_argument('--init', action='store_true')
    parser.add_argument('--data', default='/ads_data', help='Writeable directory with node and accounts configurations.')
    parser.add_argument('--wait', action='store_true')

    args = parser.parse_args()

    if args.action == 'network':
        check_data(args.data)
        network_action(args.data)
    elif args.action == 'wait':
        check_data(args.data)
        wait_action(args.data)
    elif args.action == 'clean':
        check_data(args.data)
        clean_action(args.data)
    elif args.action == 'start':
        check_data(args.data)
        start_action(args.data, args.init)
    elif args.action == 'stop':
        stop_action(args.data)
    elif args.action == 'nodes':
        check_data(args.data)
        nodes_action(args.data)

    if args.wait:
        wait_action(args.data)
