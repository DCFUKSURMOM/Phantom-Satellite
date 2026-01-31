# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

installer:
	@$(MAKE) -C phantomsatellite/installer installer

package:
	@$(MAKE) -C phantomsatellite/installer make-archive

l10n-package:
	@$(MAKE) -C phantomsatellite/installer make-langpack

mozpackage:
	@$(MAKE) -C phantomsatellite/installer

package-compare:
	@$(MAKE) -C phantomsatellite/installer package-compare

stage-package:
	@$(MAKE) -C phantomsatellite/installer stage-package make-buildinfo-file

install::
	@$(MAKE) -C phantomsatellite/installer install

clean::
	@$(MAKE) -C phantomsatellite/installer clean

distclean::
	@$(MAKE) -C phantomsatellite/installer distclean

source-package::
	@$(MAKE) -C phantomsatellite/installer source-package

upload::
	@$(MAKE) -C phantomsatellite/installer upload

source-upload::
	@$(MAKE) -C phantomsatellite/installer source-upload

hg-bundle::
	@$(MAKE) -C phantomsatellite/installer hg-bundle

l10n-check::
	@$(MAKE) -C phantomsatellite/locales l10n-check

ifdef ENABLE_TESTS
# Implemented in testing/testsuite-targets.mk

mochitest-browser-chrome:
	$(RUN_MOCHITEST) --browser-chrome
	$(CHECK_TEST_ERROR)

mochitest:: mochitest-browser-chrome

.PHONY: mochitest-browser-chrome

mochitest-metro-chrome:
	$(RUN_MOCHITEST) --metro-immersive --browser-chrome
	$(CHECK_TEST_ERROR)


endif
