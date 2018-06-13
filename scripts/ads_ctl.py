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


DATA_DIR = '/ads_data'
DAEMON_BIN_NAME = 'escd'
CLIENT_BIN_NAME = 'esc'


def start_node(nconf_path, init=False, block_time=32):

    os.chdir(nconf_path)

    with open('genesis.json', 'r') as f:
        genesis = json.load(f)

    genesis['config'] = dict()
    genesis['config']['start_time'] = (int(time.time() / block_time) + 2) * block_time

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
    except OSError:
        print("Server not started.")
        sys.exit(1)

    print("ADS node {0} started.".format(nconf_path))


def stop_node(nconf_path):
    os.chdir(nconf_path)

    with open('{0}.pid'.format(DAEMON_BIN_NAME), 'r') as f:
        pid = int(f.read())

    os.kill(pid, signal.SIGKILL)
    print("ADS node {0} stopped.".format(nconf_path))


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
            except OSError:
                print("Process {0} not killed".format(p.pid))
                sys.exit(1)


def clean():

    if os.path.exists(DATA_DIR):
        shutil.rmtree(DATA_DIR)
        os.mkdir(DATA_DIR)
        print("ADS node configuration removed.")
    else:
        print("{0} doesn't exist".format(DATA_DIR))


def state(nconf_path):
    os.chdir(nconf_path)

    m = hashlib.md5()
    with open('genesis.json', 'r') as f:
       m.update(f.read())

    print(" Genesis.json md5: {0}".format(m.hexdigest()))

    with open('{0}.pid'.format(DAEMON_BIN_NAME)) as f:
       pid = int(f.read())

    try:
       os.kill(pid, 0)
       print("# Node is UP with pid: {0}".format(pid))
    except OSError:
       print("# Node is DOWN! (supposed pid: {0}".format(pid))


def investigate(uconf_path):
    os.chdir(uconf_path)

    with open(os.devnull, 'w') as devnull:
        output = subprocess.check_output('echo \'{"run":"get_block"}\' | ./{0}'.format(CLIENT_BIN_NAME), stderr=devnull, shell=True)

    json_out = json.loads(output)
    try:
        last_block_id, last_block_hash = json_out['block']['id'], json_out['block']['nowhash']
        mtimes = sorted([int(n['mtim']) for n in json_out['block']['nodes']])

        print(" Last block ID: {0}".format(last_block_id))
        print(" Last block hash: {0}".format(last_block_hash))
        print(" Last message: {0:.3f} seconds ago.".format(time.time() - mtimes[-1]))
    except KeyError:
        print(" No block data.")


if __name__ == '__main__':

    block_time = 32

    parser = argparse.ArgumentParser(description='Start ADS nodes.')
    parser.add_argument('action', choices=['start', 'clean', 'stop', 'nodes', 'network'])
    parser.add_argument('--init', action='store_true')
    args = parser.parse_args()

    if args.action == 'network':
        for uconf in sorted(glob('/ads_data/user*.00000000')):
            print(uconf)
            investigate(uconf)

    elif args.action == 'clean':
        clean()

    for nconf in sorted(glob('/ads_data/node*')):

        if args.action == 'start':
            print(nconf)
            if args.init:
                start_node(nconf, True, block_time)
                args.init = False
            else:
                start_node(nconf)

        elif args.action == 'stop':
            print(nconf)
            stop_node(nconf)
            stop_all()

        elif args.action == 'nodes':
            print(nconf)
            state(nconf)
