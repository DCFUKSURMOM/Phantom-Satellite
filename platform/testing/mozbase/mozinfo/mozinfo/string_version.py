# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from mozbuild.version import RichVersion

class StringVersion(str):
    """
    A string version that can be compared with comparison operators.
    """

    def __init__(self, vstring):
        super().__init__()
        self.version = RichVersion(vstring)

    def __repr__(self):
        return "StringVersion ('%s')" % self

    def __to_version(self, other):
        if not isinstance(other, StringVersion):
            other = RichVersion(other)
        return other.version

    # rich comparison methods

    def __lt__(self, other):
        return self.version < self.__to_version(other)

    def __le__(self, other):
        return self.version <= self.__to_version(other)

    def __eq__(self, other):
        return self.version == self.__to_version(other)

    def __ne__(self, other):
        return self.version != self.__to_version(other)

    def __gt__(self, other):
        return self.version > self.__to_version(other)

    def __ge__(self, other):
        return self.version >= self.__to_version(other)
