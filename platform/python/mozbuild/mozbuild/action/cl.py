# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import ctypes
import os
import sys

from mozprocess.processhandler import ProcessHandlerMixin
from mozbuild.makeutil import Makefile
from mozbuild.getencoding import getencoding

def getenv_raw(name, default=None):

    # Py3's os.environ.get uses _wgetenv on Windows, but Python is invoked via
    # CreateProcessA by MSYS1, meaning non-ASCII variables are corrupted.
    # Work around this by using ctypes and getting the bytes from Windows.
    GetEnvironmentVariableA = ctypes.windll.kernel32.GetEnvironmentVariableA
    # Tells Python to use uint32_t here (in Windows terms, a DWORD).
    GetEnvironmentVariableA.restype = ctypes.c_uint32

    # Allocate a string buffer.
    buf = ctypes.create_string_buffer(32768)
    # Call into GetEnvironmentVariableA.
    ret = GetEnvironmentVariableA(name.encode('ascii'), buf, ctypes.sizeof(buf))

    if ret == 0:
        return default
    return buf.value  # raw bytes

# Get the UTF-8 bytes from the environment and decode them correctly.
value_env = getenv_raw("CL_INCLUDES_PREFIX")
decoded = value_env.decode('utf-8')
# Re-encode them using the correct console output encoding.
CL_INCLUDES_PREFIX = decoded.encode(getencoding())

GetShortPathName = ctypes.windll.kernel32.GetShortPathNameW
GetLongPathName = ctypes.windll.kernel32.GetLongPathNameW


# cl.exe likes to print inconsistent paths in the showIncludes output
# (some lowercased, some not, with different directions of slashes),
# and we need the original file case for make/pymake to be happy.
# As this is slow and needs to be called a lot of times, use a cache
# to speed things up.
_normcase_cache = {}

def normcase(path):
    # Get*PathName want paths with backslashes
    path = path.replace('/', os.sep)
    dir = os.path.dirname(path)
    # name is fortunately always going to have the right case,
    # so we can use a cache for the directory part only.
    name = os.path.basename(path)
    if dir in _normcase_cache:
        result = _normcase_cache[dir]
    else:
        path = ctypes.create_unicode_buffer(dir)
        length = GetShortPathName(path, None, 0)
        shortpath = ctypes.create_unicode_buffer(length)
        GetShortPathName(path, shortpath, length)
        length = GetLongPathName(shortpath, None, 0)
        if length > len(path):
            path = ctypes.create_unicode_buffer(length)
        GetLongPathName(shortpath, path, length)
        result = _normcase_cache[dir] = path.value
    return os.path.join(result, name)


def InvokeClWithDependencyGeneration(cmdline):
    target = ""
    # Figure out what the target is
    for arg in cmdline:
        if arg.startswith("-Fo"):
            target = arg[3:]
            break

    if target is None:
        print("No target set", file=sys.stderr)
        return 1

    # Assume the source file is the last argument
    source = cmdline[-1]
    assert not source.startswith('-')

    # The deps target lives here
    depstarget = os.path.basename(target) + ".pp"

    cmdline += ['-showIncludes']

    mk = Makefile()
    rule = mk.create_rule([target])
    rule.add_dependencies([normcase(source)])

    def on_line(line):
        # cl -showIncludes prefixes every header with "Note: including file:"
        # and an indentation corresponding to the depth (which we don't need)
        if CL_INCLUDES_PREFIX in line:
            dep = line[len(CL_INCLUDES_PREFIX):].strip()
            # We can't handle paths with spaces properly in mddepend.pl, but
            # we can assume that anything in a path with spaces is a system
            # header and throw it away.
            dep = dep.decode(getencoding())
            dep = normcase(dep)
            if ' ' not in dep:
                rule.add_dependencies([dep])
        else:
            # Make sure we preserve the relevant output from cl. mozprocess
            # swallows the newline delimiter, so we need to re-add it.
            sys.stdout.buffer.write(line)
            sys.stdout.buffer.write(b'\n')

    # We need to ignore children because MSVC can fire up a background process
    # during compilation. This process is cleaned up on its own. If we kill it,
    # we can run into weird compilation issues.
    p = ProcessHandlerMixin(cmdline, processOutputLine=[on_line],
        ignore_children=True, raw=True)
    p.run()
    p.processOutput()
    ret = p.wait()

    if ret != 0 or target == "":
        # p.wait() returns a long. Somehow sys.exit(long(0)) is like
        # sys.exit(1). Don't ask why.
        return int(ret)

    depsdir = os.path.normpath(os.path.join(os.curdir, ".deps"))
    depstarget = os.path.join(depsdir, depstarget)
    if not os.path.isdir(depsdir):
        try:
            os.makedirs(depsdir)
        except OSError:
            pass # This suppresses the error we get when the dir exists, at the
                 # cost of masking failure to create the directory.  We'll just
                 # die on the next line though, so it's not that much of a loss.

    with open(depstarget, "w") as f:
        mk.dump(f)

    return 0

def main(args):
    return InvokeClWithDependencyGeneration(args)

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
