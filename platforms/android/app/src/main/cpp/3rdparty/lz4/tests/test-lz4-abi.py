#!/usr/bin/env python3
"""Test LZ4 interoperability between versions"""

#
# Copyright (C) 2011-present, Takayuki Matsuoka
# All rights reserved.
# GPL v2 License
#

import argparse
import glob
import subprocess
import filecmp
import os
import shutil
import sys
import hashlib

repo_url = 'https://github.com/lz4/lz4.git'
tmp_dir_name = 'tests/abiTests'
env_flags = ' ' # '-j CFLAGS="-g -O0 -fsanitize=address"'
make_cmd = 'make'
git_cmd = 'git'
test_dat_src = ['README.md']
head = 'v999'

parser = argparse.ArgumentParser()
parser.add_argument("--verbose", action="store_true", help="increase output verbosity")
args = parser.parse_args()

def debug_message(msg):
    if args.verbose:
        print(msg)

def proc(cmd_args, pipe=True, env=False):
    if env == False:
        env = os.environ.copy()
    debug_message("Executing command {} with env {}".format(cmd_args, env))
    if pipe:
        s = subprocess.Popen(cmd_args,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE,
                             env = env)
    else:
        s = subprocess.Popen(cmd_args, env = env)
    stdout_data, stderr_data = s.communicate()
    if s.poll() != 0:
        print('Error Code:', s.poll())
        print('Standard Error:', stderr_data.decode())
        sys.exit(1)
    return stdout_data, stderr_data

def make(args, pipe=True, env=False):
    if env == False:
        env = os.environ.copy()
    # we want the address sanitizer for abi tests
    if 'CFLAGS' in env:
        env["CFLAGS"] += " -fsanitize=address"
    if 'LDFLAGS' in env:
        env["LDFLAGS"] += " -fsanitize=address"
    return proc([make_cmd] + ['-j'] + ['V=1'] + ['DEBUGFLAGS='] + args, pipe, env)

def git(args, pipe=True):
    return proc([git_cmd] + args, pipe)

def get_git_tags():
    # Only start from first v1.7.x format release
    stdout, stderr = git(['tag', '-l', 'v[1-9].[0-9].[0-9]'])
    tags = stdout.decode('utf-8').split()
    return tags

# https://stackoverflow.com/a/19711609/2132223
def sha1_of_file(filepath):
    with open(filepath, 'rb') as f:
        return hashlib.sha1(f.read()).hexdigest()

if __name__ == '__main__':
    if sys.platform == "darwin":
        print("!!! Warning: this test is not validated for macos !!!")

    error_code = 0
    base_dir = os.getcwd() + '/..'           # /path/to/lz4
    tmp_dir = base_dir + '/' + tmp_dir_name  # /path/to/lz4/tests/versionsTest
    clone_dir = tmp_dir + '/' + 'lz4'        # /path/to/lz4/tests/versionsTest/lz4
    lib_dir = base_dir + '/lib'              # /path/to/lz4/lib
    test_dir = base_dir + '/tests'
    os.makedirs(tmp_dir, exist_ok=True)

    # since Travis clones limited depth, we should clone full repository
    if not os.path.isdir(clone_dir):
        git(['clone', repo_url, clone_dir])

    # Retrieve all release tags
    print('Retrieve release tags >= v1.7.5 :')
    os.chdir(clone_dir)
    tags = [head] + get_git_tags()
    tags = [x for x in tags if (x >= 'v1.7.5')]
    print(tags)

    # loop across architectures
    # note : '-mx32' was removed, because some CI environment (GA) do not support x32 well
    for march in ['-m64', '-m32']:
        print(' ')
        print('=====================================')
        print('Testing architecture ' + march);
        print('=====================================')

        # Build all versions of liblz4
        # note : naming scheme only works on Linux
        for tag in tags:
            print('building library ', tag)
            os.chdir(base_dir)
    #        if not os.path.isfile(dst_liblz4) or tag == head:
            if tag != head:
                r_dir = '{}/{}'.format(tmp_dir, tag)  # /path/to/lz4/test/lz4test/<TAG>
                #print('r_dir = ', r_dir)  # for debug
                os.makedirs(r_dir, exist_ok=True)
                os.chdir(clone_dir)
                git(['--work-tree=' + r_dir, 'checkout', tag, '--', '.'])
                os.chdir(r_dir + '/lib')  # /path/to/lz4/lz4test/<TAG>/lib
            else:
                # print('lib_dir = {}', lib_dir)  # for debug
                os.chdir(lib_dir)
            make(['clean'])
            build_env = os.environ.copy()
            build_env["CFLAGS"] = "-O1 " + march
            make(['liblz4'], env=build_env)

        print(' ')
        print('******************************')
        print('Round trip expecting current ABI but linking to older Dynamic Library version')
        print('******************************')
        os.chdir(test_dir)
        # Start with matching version : should be no problem
        build_env = os.environ.copy()
        # we use asan to detect any out-of-bound read or write
        build_env["CFLAGS"] = "-O1 " + march
        build_env["LDFLAGS"] = "-L../lib"
        build_env["LDLIBS"] = "-llz4"
        if os.path.isfile('abiTest'):
            os.remove('abiTest')
        make(['abiTest'], env=build_env, pipe=False)

        for tag in tags:
            print('linking to lib tag = ', tag)
            run_env = os.environ.copy()
            if tag == head:
                run_env["LD_LIBRARY_PATH"] = '../lib'
            else:
                run_env["LD_LIBRARY_PATH"] = 'abiTests/{}/lib'.format(tag)
            # check we are linking to the right library version at run time
            proc(['./check_liblz4_version.sh'] + ['./abiTest'], pipe=False, env=run_env)
            # now run with mismatched library version
            proc(['./abiTest'] + test_dat_src, pipe=False, env=run_env)

        print(' ')
        print('******************************')
        print('Round trip using current Dynamic Library expecting older ABI version')
        print('******************************')

        for tag in tags:
            print(' ')
            print('building using older lib ', tag)
            build_env = os.environ.copy()
            if tag != head:
                build_env["CPPFLAGS"] = '-IabiTests/{}/lib'.format(tag)
                build_env["LDFLAGS"]  = '-LabiTests/{}/lib'.format(tag)
            else:
                build_env["CPPFLAGS"] = '-I../lib'
                build_env["LDFLAGS"]  = '-L../lib'
            build_env["LDLIBS"]  = "-llz4"
            build_env["CFLAGS"]  = "-O1 " + march
            os.remove('abiTest')
            make(['abiTest'], pipe=False, env=build_env)

            print('run with CURRENT library version (head)')
            run_env = os.environ.copy()
            run_env["LD_LIBRARY_PATH"] = '../lib'
            # check we are linking to the right library version at run time
            proc(['./check_liblz4_version.sh'] + ['./abiTest'], pipe=False, env=run_env)
            # now run with mismatched library version
            proc(['./abiTest'] + test_dat_src, pipe=False, env=run_env)


    if error_code != 0:
        print('ERROR')

    sys.exit(error_code)
