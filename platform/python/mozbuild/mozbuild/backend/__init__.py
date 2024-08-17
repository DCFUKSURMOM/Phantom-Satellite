# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

backends = {
    'CompileDB': 'mozbuild.compilation.database',
    'FasterMake': 'mozbuild.backend.fastermake',
    'RecursiveMake': 'mozbuild.backend.recursivemake',
}


def get_backend_class(name):
    class_name = '%sBackend' % name
    module = __import__(backends[name], globals(), locals(), [class_name])
    return getattr(module, class_name)
