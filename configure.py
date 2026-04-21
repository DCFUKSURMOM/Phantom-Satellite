# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os
import sys
import importlib.machinery
import types


base_dir = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.path.join(base_dir, 'platform', 'python', 'mozbuild'))
from mozbuild.configure import ConfigureSandbox

def load_source(module_name, path):
    loader = importlib.machinery.SourceFileLoader(module_name, path)
    module = types.ModuleType(module_name)
    module.__loader__ = loader
    module.__file__ = path
    module.__package__ = module_name.rpartition('.')[0]

    sys.modules[module_name] = module

    code = loader.get_code(module_name)
    exec(code, module.__dict__)
    return module

config_mod = load_source(
        "configure",
        os.path.join(base_dir, "platform", "configure.py")
)
config_status = config_mod.config_status

def main(argv):
    config = {}
    sandbox = ConfigureSandbox(config, os.environ, argv)
    sandbox.run(os.path.join(os.path.dirname(__file__), 'moz.configure'))

    if sandbox._help:
        return 0

    return config_status(config)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
