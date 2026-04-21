# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import codecs
import os
import subprocess
import sys
import textwrap

base_dir = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, os.path.join(base_dir, 'python', 'mozbuild'))
from mozbuild.configure import ConfigureSandbox
from mozbuild.util import (
    indented_repr,
)


def main(argv):
    config = {}
    sandbox = ConfigureSandbox(config, os.environ, argv)
    sandbox.run(os.path.join(os.path.dirname(__file__), 'moz.configure'))

    if sandbox._help:
        return 0

    return config_status(config)


def config_status(config):
    # Sanitize config data to feed config.status
    # Ideally, all the backend and frontend code would handle the booleans, but
    # there are so many things involved, that it's easier to keep config.status
    # untouched for now.
    def sanitized_values(v):
        if v is True:
            return '1'
        if v is False:
            return ''
        if isinstance(v, dict):
            return {key: sanitized_values(value) for key, value in v.items()}
        if isinstance(v, (list, tuple)):
            return [sanitized_values(value) for value in v]
        return v

    sanitized_config = {}
    sanitized_config['substs'] = {
        key: sanitized_values(value) for key, value in config.items()
        if key not in ('DEFINES', 'non_global_defines', 'TOPSRCDIR', 'TOPOBJDIR')
    }
    sanitized_config['defines'] = {
        key: sanitized_values(value) for key, value in config['DEFINES'].items()
    }
    sanitized_config['non_global_defines'] = config['non_global_defines']
    sanitized_config['topsrcdir'] = config['TOPSRCDIR']
    sanitized_config['topobjdir'] = config['TOPOBJDIR']
    sanitized_config['mozconfig'] = config.get('MOZCONFIG')

    # Create config.status. Eventually, we'll want to just do the work it does
    # here, when we're able to skip configure tests/use cached results/not rely
    # on autoconf.
    print("Creating config.status", file=sys.stderr)
    with codecs.open('config.status', 'w', encoding='utf-8', errors='replace') as fh:
        fh.write(textwrap.dedent('''\
            #!%(python)s
            # coding=%(encoding)s
            from __future__ import unicode_literals
        ''') % {'python': config['PYTHON'], 'encoding': 'utf-8'})
        # A lot of the build backend code is currently expecting byte
        # strings and breaks in subtle ways with unicode strings. (bug 1296508)
        for k, v in sanitized_config.items():
            fh.write('%s = %s\n' % (k, indented_repr(v)))
        fh.write("__all__ = ['topobjdir', 'topsrcdir', 'defines', "
                 "'non_global_defines', 'substs', 'mozconfig']")

        if config.get('MOZ_BUILD_APP') != 'js' or config.get('JS_STANDALONE'):
            fh.write(textwrap.dedent('''
                if __name__ == '__main__':
                    from mozbuild.config_status import config_status
                    args = dict([(name, globals()[name]) for name in __all__])
                    config_status(**args)
            '''))

    # Other things than us are going to run this file, so we need to give it
    # executable permissions.
    os.chmod('config.status', 0o755)
    if config.get('MOZ_BUILD_APP') != 'js' or config.get('JS_STANDALONE'):
        os.environ['WRITE_MOZINFO'] = '1'
        from mozbuild.config_status import config_status

        # Some values in sanitized_config also have more complex types, such as
        # EnumString and tuples. Converting string subclasses to plain strings
        # and tuples to lists keeps this code path consistent with the
        # generated config.status file, which serializes iterables as lists.
        return config_status(args=[], **sanitized_config)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
