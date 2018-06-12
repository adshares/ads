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


def start_node(nconf_path, init=False, block_time=32):

    os.chdir(nconf_path)

    with open('genesis.json', 'r') as f:
        genesis = json.load(f)

    genesis['config'] = dict()
    genesis['config']['start_time'] = int(time.time() / block_time) * block_time

    with open('genesis.json', 'w') as f:
        json.dump(genesis, f)

    cmd = ['./escd', '--genesis=genesis.json']

    if init:
        cmd += ['--init=true']

    stdout = open(os.path.join(nconf_path, 'stdout'), 'w')
    stderr = open(os.path.join(nconf_path, 'stderr'), 'w')

    proc = subprocess.Popen(cmd, stdout=stdout, stderr=stderr)

    try:
        os.kill(proc.pid, 0)
        with open('escd.pid', 'w') as f:
            f.write(str(proc.pid))
    except OSError:
        print("Sever not started.")
        sys.exit(1)

    print("ADS node {0} started.".format(nconf_path))


def stop_node(nconf_path):
    os.chdir(nconf_path)

    with open('escd.pid', 'r') as f:
        pid = int(f.read())

    os.kill(pid, signal.SIGKILL)
    print("ADS node {0} stopped.".format(nconf_path))


def clean(nconf_path):

    shutil.rmtree(nconf_path)
    print("ADS node {0} configuration removed.".format(nconf_path))


def state(nconf_path):
    os.chdir(nconf_path)

    m = hashlib.md5()
    with open('genesis.json', 'r') as f:
       m.update(f.read())

    print(" Genesis.json md5: {0}".format(m.hexdigest()))

    with open('escd.pid', 'r') as f:
       pid = int(f.read())

    try:
       os.kill(pid, 0)
       print("# Node is UP.")
    except OSError:
       print("# Node is DOWN!")


def investigate(uconf_path):
    os.chdir(uconf_path)

    with open(os.devnull, 'w') as devnull:
        output = subprocess.check_output('echo \'{"run":"get_block"}\' | ./esc', stderr=devnull, shell=True)

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
    parser.add_argument('action', choices=['start', 'clean', 'stop', 'state'])
    parser.add_argument('--init', action='store_true')
    args = parser.parse_args()

    for nconf in sorted(glob('/ads_data/node*')):
        print(nconf)
        if args.action == 'start':

            if args.init:
                start_node(nconf, True, block_time)
                args.init = False
            else:
                start_node(nconf)

        elif args.action == 'stop':
            stop_node(nconf)
        elif args.action == 'clean':
            clean(nconf)
        elif args.action == 'state':
           state(nconf)

    if args.action == 'state':
        for uconf in sorted(glob('/ads_data/user*.00000000')):
            print(uconf)
            investigate(uconf)
