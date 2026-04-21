# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at https://mozilla.org/MPL/2.0/.

# Utilities for parsing and comparing version strings.
#
# Two schemes are supported:
#
#   - Version: a constrained, well-defined format (e.g. 1.2.3, 1.5a1)
#   - RichVersion: a permissive tokenizer that accepts essentially anything
#
# Both expose the same comparison semantics via BaseVersion, but differ
# in how (and how much) structure they impose on the input.

from collections import namedtuple
from functools import total_ordering
import itertools

# Python 2 exposed cmp() returning -1/0/1. Having named sentinel values
# still makes the comparison code easier to read and reason about.
#
# Nothing outside this module should rely on these exact values.

class RetCode:
    OK = 0
    LESS_THAN = -1
    GREATER_THAN = 1


# Lightweight read-only views into Level.
#
# These exist so callers can use attribute access (v.MAJOR) instead of
# tuple indexing, without paying for a full object with a __dict__.
# namedtuple + __slots__ keeps them essentially free.

class VersionView(namedtuple("VersionView", ["MAJOR", "MINOR", "PATCH"])):
    __slots__ = ()

class PrereleaseView(namedtuple("PrereleaseView", ["PRERELEASE", "PRERELEASE_NUM"])):
    __slots__ = ()


# Canonical representation for strict versions.
#
# This is where normalization happens. Parsing produces raw components;
# Level turns them into a consistent, comparable form:
#
#   - numeric fields are coerced to int
#   - missing values are filled in
#   - prerelease is kept as a tag + number pair
#
# The rest of the system treats this as the source of truth. Comparison
# and views are both derived from here.

class Level(namedtuple("Level", ["MAJOR", "MINOR", "PATCH", "PRERELEASE", "PRERELEASE_NUM"])):
    __slots__ = ()

    def __new__(cls, major, minor, patch=0, prerelease=None, prerelease_num=0):
        def norm(x, default=0):
            if x is None:
                return default
            if isinstance(x, str):
                if 'esr' in x:
                    return x
            return int(x)

        # A prerelease number without a prerelease tag is meaningless.
        # Normalize it away rather than letting inconsistent states leak.
        if prerelease is None:
            prerelease_num = 0

        return super().__new__(
            cls,
            norm(major),
            norm(minor),
            norm(patch),
            prerelease,
            norm(prerelease_num)
        )

    # Provide structured views without changing the underlying storage.
    # These are computed properties so Level stays a flat, hashable tuple.

    @property
    def version(self):
        return VersionView(self.MAJOR, self.MINOR, self.PATCH)

    @property
    def prerelease(self):
        if self.PRERELEASE is None:
            return None
        if 'esr' in self.PRERELEASE:
            return self.PRERELEASE
        return PrereleaseView(self.PRERELEASE, self.PRERELEASE_NUM)

    def _cmp_key(self):
        # Build a tuple that sorts correctly under native
        # lexicographic comparison.
        #
        # Prerelease handling is the only non-trivial part:
        #
        #   1.0a1 < 1.0
        #
        # We encode this by tagging prereleases lower than final releases:
        #
        #   prerelease -> (OK, tag, num)
        #   final      -> (GREATER_THAN,)
        #
        # Since (1,) > (0, ...), final releases sort after prereleases
        # without any special-case logic in the comparator.

        pre_key = (RetCode.GREATER_THAN,) if self.PRERELEASE is None \
                  else (RetCode.OK, self.PRERELEASE, self.PRERELEASE_NUM)

        return (self.MAJOR, self.MINOR, self.PATCH, pre_key)


# Canonical representation for RichVersion.
#
# Unlike Level, this does not attempt to impose structure. It simply stores
# a sequence of tokens (ints or strings) and defines how they compare.
#
# The only rule enforced here is traditional mixed-type ordering:
#
#   int < str
#
# Everything else is left to the input.

class TokenLevel(tuple):
    __slots__ = ()

    def __new__(cls, components):
        # Components are expected to already be tokenized. We do a small
        # amount of defensive normalization to avoid surprises if callers
        # pass strings that happen to be numeric.
        normalized = []
        for part in components:
            if isinstance(part, int):
                normalized.append(part)
            elif isinstance(part, str) and part.isdigit():
                normalized.append(int(part))
            else:
                normalized.append(part)

        return super().__new__(cls, normalized)


    @property
    def version(self):
        # Take the first three integer components, stopping at the first
        # non-integer and padding the rest with zeroes.
        (MAJOR, MINOR, PATCH) = list(itertools.chain(
        itertools.takewhile(lambda x: isinstance(x, int), self),
        (0, 0, 0)))[:3]

        return VersionView(MAJOR, MINOR, PATCH)


    def _cmp_key(self):
        # Wrap each element so that ints sort before strings, and let
        # tuple comparison handle the rest.
        return tuple(
            (RetCode.OK, part) if isinstance(part, int)
            else (RetCode.GREATER_THAN, part)
            for part in self
        )


# Shared comparison machinery.
#
# Subclasses provide a _cmp_key(); BaseVersion defines how those keys are
# compared and wires them into Python's rich comparison protocol.
#
# This replaces the old __cmp__-based design with the minimal amount of
# boilerplate required by @total_ordering.

@total_ordering
class BaseVersion:

    def __init__(self, version_string=None):
        if version_string:
            self.parse(version_string)

    def __repr__(self):
        return "%s ('%s')" % (self.__class__.__name__, str(self))

    def _coerce_other(self, other):
        # Allow comparisons against raw strings by coercing them into
        # the same class. Cross-type comparisons are intentionally not
        # supported.
        if isinstance(other, str):
            return self.__class__(other)
        if not isinstance(other, self.__class__):
            return NotImplemented
        return other

    def _cmp(self, other):
        other = self._coerce_other(other)
        if other is NotImplemented:
            return NotImplemented

        a = self._cmp_key()
        b = other._cmp_key()

        if a == b:
            return RetCode.OK
        return RetCode.LESS_THAN if a < b else RetCode.GREATER_THAN

    # __eq__ and __lt__ are sufficient for @total_ordering.

    def __eq__(self, other):
        result = self._cmp(other)
        if result is NotImplemented:
            return result
        return result == RetCode.OK

    def __lt__(self, other):
        result = self._cmp(other)
        if result is NotImplemented:
            return result
        return result < RetCode.OK


# Expressive version scheme.
#
# The input is tokenized into runs of digits and non-digits. Separators
# (such as '.') are ignored. No validation is performed; any string is
# accepted.
#
# This makes the behavior predictable but not necessarily intuitive for
# inconsistent versioning schemes. That tradeoff is intentional.

class RichVersion(BaseVersion):

    def __init__(self, version_string=None):
        if version_string:
            self.parse(version_string)

    def parse(self, version_string):
        # Preserve the original string for display. Unlike Version, we
        # cannot reconstruct a canonical form from the parsed structure.
        self.version_string = version_string

        components = []
        current = []
        mode = None  # 'digit', 'alpha', or 'other'

        def flush():
            if not current:
                return
            token = ''.join(current)
            if mode == 'digit':
                components.append(int(token))
            else:
                components.append(token)
            current.clear()

        for ch in version_string:
            if ch.isdigit():
                new_mode = 'digit'
            elif ch.isalpha():
                new_mode = 'alpha'
            elif "." in ch:
                # Periods are separators only.
                new_mode = None
            else:
                new_mode = 'other'

            if new_mode != mode:
                flush()
                mode = new_mode

            if new_mode:
                current.append(ch)
        else:
            flush()
            mode = None

        flush()

        self._level = TokenLevel(components)

    @property
    def version(self):
        # Expose a list for compatibility with existing expectations.
        return list(self._level)

    @property
    def level(self):
        # Expose semi-strict VersionView for some callers.
        return self._level.version

    def _cmp_key(self):
        return self._level._cmp_key()

    def __str__(self):
        return self.version_string

    def __repr__(self):
        return "RichVersion ('%s')" % str(self)


# Normal version scheme.
#
# Accepts:
#   MAJOR.MINOR[.PATCH][aN|bN]
#
# The structure is enforced and normalized via Level. Invalid inputs are
# rejected early.

class Version(BaseVersion):

    def parse(self, version_string):
        if not version_string:
            raise ValueError

        # Require exactly two or three numeric components.
        if version_string.count('.') not in (1, 2):
            raise ValueError("invalid version number '%s'" % version_string)

        prerelease = None
        prerelease_num = None
        major = None
        minor = None
        patch = None

        # Extract prerelease if present. We only support 'a' and 'b'.
        # This is intentionally simple and assumes well-formed input.
        for pre_type in ('a', 'b', 'esr'):
            if pre_type in version_string:
                version_string, pre_part = version_string.split(pre_type)
                if pre_type != 'esr':
                    prerelease = pre_type + pre_part
                    prerelease_num = pre_part
                else:
                    prerelease = None
                    prerelease_num = None
                break
            else:
                prerelease = None
                prerelease_num = None
                pre_type = None

        try:
            major, minor, patch = version_string.split('.')
        except ValueError:
            try:
                major, minor = version_string.split('.')
            except ValueError:
                raise ValueError("invalid version number '%s'" % version_string)

        # For ESR, do this last-minute:
        if pre_type == 'esr':
            if patch:
                prerelease = patch + pre_type
            else:
                prerelease = minor + pre_type

        try:
            level = Level(major, minor, patch, pre_type, prerelease_num)
        except (ValueError, IndexError):
            raise ValueError("invalid version number '%s'" % version_string)

        self.level = level

    @property
    def version(self):
        return self.level.version

    @property
    def prerelease(self):
        return self.level.prerelease

    def __str__(self):
        # Suppress trailing '.0' to match historical behavior.
        if self.version.PATCH == 0:
            version_string = "%d.%d" % (self.version.MAJOR, self.version.MINOR)
        else:
            version_string = "%d.%d.%d" % (
                self.version.MAJOR,
                self.version.MINOR,
                self.version.PATCH
            )

        if self.prerelease:
            if 'esr' not in self.level.PRERELEASE:
                version_string += (
                    self.prerelease.PRERELEASE +
                    str(self.prerelease.PRERELEASE_NUM)
                )
            else:
                version_string += (
                   self.level.PRERELEASE
                )

        return version_string

    def _cmp_key(self):
        return self.level._cmp_key()
