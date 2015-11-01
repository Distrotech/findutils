# Copyright (C) 2014, 2015 Free Software Foundation, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
"""Check that the list of test files in Makefile.am is complete and not redundant.

Usage:
  checklists file-listing-configured-files test-root subdir-1-containing-tests [subdir-2-containing-tests ...]
"""

import os
import os.path
import re
import sys

def report_unlisted(filename):
    sys.stderr.write(
        'Error: test file %s is not listed in Makefile.am but exists on disk.\n'
        % (filename,))


def report_missing(filename):
    sys.stderr.write(
        'Error: test file %s is listed in Makefile.am but does not exist on disk.\n'
        % (filename,))

def report_dupe(filename):
    sys.stderr.write(
        'Error: test file %s is listed more than once in Makefile.am.\n'
        % (filename,))


def report_problems(problem_filenames, reporting_function):
    for f in problem_filenames:
        reporting_function(f)
    return len(problem_filenames)


def file_names(listfile_name):
    for line in open(listfile_name, 'r').readlines():
        yield line.rstrip('\n')


def configured_file_names(listfile_name):
    dupes = set()
    result = set()
    for filename in file_names(listfile_name):
        if filename in result:
            dupes.add(filename)
        else:
            result.add(filename)
    return dupes, result


def find_test_files(roots):
    testfile_rx = re.compile(r'\.(exp|xo)$')
    for root in roots:
        for parent, dirs, files in os.walk(root):
            for file_basename in files:
                if testfile_rx.search(file_basename):
                    yield os.path.join(parent, file_basename)


class TemporaryWorkingDirectory(object):

    def __init__(self, cwd):
        self.new_cwd = cwd
        self.old_cwd = os.getcwd()

    def __enter__(self):
        os.chdir(self.new_cwd)

    def __exit__(self, *unused_args):
        os.chdir(self.old_cwd)


def main(args):
    if len(args) < 3:
        sys.stderr.write(__doc__)
        return 1
    dupes, configured = configured_file_names(args[1])
    with TemporaryWorkingDirectory(args[2]):
        actual = set(find_test_files(args[3:]))
    sys.stdout.write('%d test files configured for find, %s files on-disk'
                     % (len(configured), len(actual)))
    problem_count = 0
    problem_count += report_problems(dupes, report_dupe)
    problem_count += report_problems(configured - actual, report_missing)
    problem_count += report_problems(actual - configured, report_unlisted)
    if problem_count:
        return 1
    else:
        return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
