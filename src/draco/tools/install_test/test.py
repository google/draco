#!/usr/bin/python3
#
# Copyright 2022 The Draco Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Tests installations of the Draco library.

Builds the library in shared and static configurations on the current host
system, and then confirms that a simple test application can link in both
configurations.
"""

import argparse
import multiprocessing
import os
import pathlib
import shlex
import shutil
import subprocess

# The Draco tree that this script uses.
DRACO_SOURCES_PATH = os.path.abspath('../../../..')

# Path to this script and the rest of the test project files.
TEST_SOURCES_PATH = os.path.split(os.path.abspath(__file__))[0]

# The Draco build directories.
DRACO_SHARED_BUILD_PATH = os.path.join(TEST_SOURCES_PATH, '_draco_build_shared')
DRACO_STATIC_BUILD_PATH = os.path.join(TEST_SOURCES_PATH, '_draco_build_static')

# The Draco install roots.
DRACO_SHARED_INSTALL_PATH = os.path.join(TEST_SOURCES_PATH,
                                         '_install_test_root_shared')
DRACO_STATIC_INSTALL_PATH = os.path.join(TEST_SOURCES_PATH,
                                         '_install_test_root_static')

# Argument for -j when using make, or -m when using Visual Studio. Number of
# build jobs.
NUM_PROCESSES = multiprocessing.cpu_count() - 1

# The test project build directories.
TEST_SHARED_BUILD_PATH = os.path.join(TEST_SOURCES_PATH, '_test_build_shared')
TEST_STATIC_BUILD_PATH = os.path.join(TEST_SOURCES_PATH, '_test_build_static')

# Show configuration and build output.
VERBOSE = False


def get_cmake_generator():
  """Returns the CMake generator from CMakeCache.txt in the current dir."""
  cmake_cache_file_path = os.path.join(os.getcwd(), 'CMakeCache.txt')
  cmake_cache_text = ''
  with open(cmake_cache_file_path, 'r') as cmake_cache_file:
    cmake_cache_text = cmake_cache_file.read()

  if not cmake_cache_text:
    raise FileNotFoundError(f'{cmake_cache_file_path} missing or empty.')

  generator = ''
  for line in cmake_cache_text.splitlines():
    if line.startswith('CMAKE_GENERATOR:INTERNAL='):
      generator = line.split('=')[1]

  return generator


def run_process_and_capture_output(cmd, env=None):
  """Runs |cmd| as a child process.

  Returns process exit code and output.

  Args:
    cmd: String containing the command to execute.
    env: Optional dict of environment variables.

  Returns:
    Tuple of exit code and output.
  """
  if not cmd:
    raise ValueError('run_process_and_capture_output requires cmd argument.')
  proc = subprocess.Popen(
      shlex.split(cmd),
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      env=env)
  stdout = proc.communicate()
  return [proc.returncode, stdout[0].decode('utf-8')]


def create_output_directories():
  """Creates the build output directores for the test."""
  pathlib.Path(DRACO_SHARED_BUILD_PATH).mkdir(parents=True, exist_ok=True)
  pathlib.Path(DRACO_STATIC_BUILD_PATH).mkdir(parents=True, exist_ok=True)
  pathlib.Path(TEST_SHARED_BUILD_PATH).mkdir(parents=True, exist_ok=True)
  pathlib.Path(TEST_STATIC_BUILD_PATH).mkdir(parents=True, exist_ok=True)


def cleanup():
  """Removes the build output directories from the test."""
  shutil.rmtree(DRACO_SHARED_BUILD_PATH)
  shutil.rmtree(DRACO_STATIC_BUILD_PATH)
  shutil.rmtree(TEST_SHARED_BUILD_PATH)
  shutil.rmtree(TEST_STATIC_BUILD_PATH)


def cmake_configure(source_path, cmake_args=None):
  """Configures a CMake build."""
  command = f'cmake {source_path}'

  if cmake_args:
    for arg in cmake_args:
      command += f' {arg}'

  if VERBOSE:
    print(f'CONFIGURE command:\n{command}')

  result = run_process_and_capture_output(command)

  if VERBOSE:
    print(f'CONFIGURE result:\nexit_code: {result[0]}\n{result[1]}')


def cmake_build(cmake_args=None, build_args=None):
  """Runs a CMake build."""
  command = 'cmake --build .'

  if cmake_args:
    for arg in cmake_args:
      command += f' {arg}'

  if not build_args:
    build_args = []

  generator = get_cmake_generator()
  if generator.endswith('Makefiles'):
    build_args.append(f' -j {NUM_PROCESSES}')
  elif generator.startswith('Visual'):
    build_args.append(f' -m {NUM_PROCESSES}')

  if build_args:
    command += ' --'
    for arg in build_args:
      command += f' {arg}'

  if VERBOSE:
    print(f'BUILD command:\n{command}')

  result = run_process_and_capture_output(f'{command}')

  if VERBOSE:
    print(f'BUILD result:\nexit_code: {result[0]}\n{result[1]}')


def build_and_install_draco():
  """Builds Draco in shared and static configurations."""
  # Build and install Draco in shared library config for the current host
  # machine.
  os.chdir(DRACO_SHARED_BUILD_PATH)
  cmake_args = []
  cmake_args.append(f'-DCMAKE_INSTALL_PREFIX={DRACO_SHARED_INSTALL_PATH}')
  cmake_args.append('-DBUILD_SHARED_LIBS=ON')
  cmake_configure(source_path=DRACO_SOURCES_PATH, cmake_args=cmake_args)
  cmake_build(cmake_args=['--target install'])

  # Build and install Draco in the static config for the current host machine.
  os.chdir(DRACO_STATIC_BUILD_PATH)
  cmake_args = []
  cmake_args.append(f'-DCMAKE_INSTALL_PREFIX={DRACO_STATIC_INSTALL_PATH}')
  cmake_args.append('-DBUILD_SHARED_LIBS=OFF')
  cmake_configure(source_path=DRACO_SOURCES_PATH, cmake_args=cmake_args)
  cmake_build(cmake_args=['--target install'])


def build_test_project():
  """Builds the test application in shared and static configurations."""
  os.chdir(TEST_SHARED_BUILD_PATH)

  # Configure the test project in shared mode and build it.
  cmake_configure(
      source_path=f'{TEST_SOURCES_PATH}', cmake_args=['-DBUILD_SHARED_LIBS=ON'])
  cmake_build()

  # Configure in static mode and build it.
  os.chdir(TEST_STATIC_BUILD_PATH)
  cmake_configure(
      source_path=f'{TEST_SOURCES_PATH}',
      cmake_args=['-DBUILD_SHARED_LIBS=OFF'])
  cmake_build()


def test_draco_install():
  create_output_directories()
  build_and_install_draco()
  build_test_project()
  cleanup()


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-v',
      '--verbose',
      action='store_true',
      help='Show configuration and build output.')
  args = parser.parse_args()
  if args.verbose:
    VERBOSE = True

  if VERBOSE:
    print(f'DRACO_SOURCES_PATH={DRACO_SOURCES_PATH}')
    print(f'DRACO_SHARED_BUILD_PATH={DRACO_SHARED_BUILD_PATH}')
    print(f'DRACO_STATIC_BUILD_PATH={DRACO_STATIC_BUILD_PATH}')
    print(f'DRACO_SHARED_INSTALL_PATH={DRACO_SHARED_INSTALL_PATH}')
    print(f'DRACO_STATIC_INSTALL_PATH={DRACO_STATIC_INSTALL_PATH}')
    print(f'NUM_PROCESSES={NUM_PROCESSES}')
    print(f'TEST_SHARED_BUILD_PATH={TEST_SHARED_BUILD_PATH}')
    print(f'TEST_STATIC_BUILD_PATH={TEST_STATIC_BUILD_PATH}')
    print(f'TEST_SOURCES_PATH={TEST_SOURCES_PATH}')
    print(f'VERBOSE={VERBOSE}')

  test_draco_install()
