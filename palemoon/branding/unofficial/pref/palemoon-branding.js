/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pref("app.vendorURL", "http://dcfuksurmom.duckdns.org/phantomsatellite");
pref("browser.identity.ssl_domain_display", 1);
pref("browser.newtab.url","about:newtab");
pref("extensions.getMoreThemesURL", "https://addons.palemoon.org/themes/");
pref("extensions.update.autoUpdateDefault", true);
pref("extensions.getAddons.maxResults", 10);
pref("extensions.getAddons.cache.enabled", false);
pref("dom.max_chrome_script_run_time", 90);
pref("dom.max_script_run_time", 20);
pref("plugin.default.state", 2);
pref("plugin.expose_full_path", true);
pref("dom.ipc.plugins.timeoutSecs", 20);
pref("nglayout.initialpaint.delay", 300);
pref("image.mem.max_ms_before_yield", 50);
pref("image.mem.decode_bytes_at_a_time", 65536);

pref("services.sync.serverURL","https://pmsync.palemoon.org/sync/index.php/");
pref("services.sync.jpake.serverURL","https://keyserver.palemoon.org/");
pref("services.sync.termsURL", "http://www.palemoon.org/sync/terms.shtml");
pref("services.sync.privacyURL", "http://www.palemoon.org/sync/privacy.shtml");
pref("services.sync.statusURL", "https://pmsync.palemoon.org/status/");
pref("services.sync.syncKeyHelpURL", "http://www.palemoon.org/sync/help/recoverykey.shtml");
pref("services.sync.APILevel", 1);

pref("accessibility.force_disabled", 1);
pref("devtools.selfxss.count", 5);
pref("startup.homepage_welcome_url","http://dcfuksurmom.duckdns.org/phantomsatellite");
pref("startup.homepage_override_url","http://dcfuksurmom.duckdns.org/phantomsatellite");
pref("app.releaseNotesURL", "https://github.com/DCFUKSURMOM/Phantom-Satellite/releases/latest");
pref("app.update.enabled", false);
pref("app.update.url", "");
pref("general.useragent.compatMode", 2);
pref("general.useragent.compatMode.firefox", true); //firefox compatibility mode by default (improves website compatibility)
pref("general.useragent.compatMode.gecko", true);
pref("layers.acceleration.enabled", false); //no longer enabled by default, does not work properly on PowerPC
pref("network.captive-portal-service.enabled", true); //captive portal (browser based wifi login) support by default
pref("privacy.GPCheader.enabled", true); //tell sites to not share or sell data by default
pref("canvas.poisondata", true); //poison canvas data by default
pref("general.warnOnAboutConfig", false); //nuke about:config warning
pref("xpinstall.whitelist.required", true); //warn when sites try to install extentions