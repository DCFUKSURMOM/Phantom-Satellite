/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pref("security.tls.version.min", 1);
pref("security.tls.version.max", 4);
pref("security.tls.version.fallback-limit", 3);
pref("security.tls.insecure_fallback_hosts", "");
pref("security.tls.unrestricted_rc4_fallback", false);
pref("security.tls.enable_0rtt_data", false);

pref("security.ssl.treat_unsafe_negotiation_as_broken", false);
pref("security.ssl.require_safe_negotiation",  false);
pref("security.ssl.enable_ocsp_stapling", true);
pref("security.ssl.enable_false_start", true);
pref("security.ssl.enable_alpn", true);

// TLS 1.3 cipher suites
pref("security.tls13.aes_128_gcm_sha256", true);
pref("security.tls13.chacha20_poly1305_sha256", true);
pref("security.tls13.aes_256_gcm_sha384", true);

// TLS 1.0-1.2 cipher suites
pref("security.ssl3.ecdhe_rsa_aes_128_gcm_sha256", true);
pref("security.ssl3.ecdhe_ecdsa_aes_128_gcm_sha256", true);
pref("security.ssl3.ecdhe_ecdsa_chacha20_poly1305_sha256", true);
pref("security.ssl3.ecdhe_rsa_chacha20_poly1305_sha256", true);
pref("security.ssl3.ecdhe_ecdsa_aes_256_gcm_sha384", true);
pref("security.ssl3.ecdhe_rsa_aes_256_gcm_sha384", true);
pref("security.ssl3.ecdhe_rsa_aes_128_sha", true);
pref("security.ssl3.ecdhe_ecdsa_aes_128_sha", true);
pref("security.ssl3.ecdhe_rsa_aes_256_sha", true);
pref("security.ssl3.ecdhe_ecdsa_aes_256_sha", true);
pref("security.ssl3.dhe_rsa_camellia_256_sha", true);
pref("security.ssl3.dhe_rsa_camellia_128_sha", true);
pref("security.ssl3.rsa_aes_256_gcm_sha384", true);
pref("security.ssl3.rsa_aes_256_sha256", true);
pref("security.ssl3.rsa_camellia_128_sha", true);
pref("security.ssl3.rsa_camellia_256_sha", true);
pref("security.ssl3.rsa_aes_128_sha", true);
pref("security.ssl3.rsa_aes_256_sha", true);

// Deprecated
pref("security.ssl3.dhe_rsa_aes_256_sha", false);
pref("security.ssl3.dhe_rsa_aes_128_sha", false);
pref("security.ssl3.rsa_aes_128_gcm_sha256", false);
pref("security.ssl3.rsa_aes_128_sha256", false);

// Weak/broken (requires fallback_hosts)
pref("security.ssl3.rsa_des_ede3_sha", false);
pref("security.ssl3.rsa_rc4_128_sha", false);
pref("security.ssl3.rsa_rc4_128_md5", false);

pref("security.content.signature.root_hash",
     "97:E8:BA:9C:F1:2F:B3:DE:53:CC:42:A4:E6:57:7E:D6:4D:F4:93:C2:47:B4:14:FE:A0:36:81:8D:38:23:56:0E");

pref("security.default_personal_cert",   "Ask Every Time");
pref("security.remember_cert_checkbox_default_setting", true);
pref("security.ask_for_password",        0);
pref("security.password_lifetime",       30);

// The supported values of this pref are:
// 0: disable detecting Family Safety mode and importing the root
// 1: only attempt to detect Family Safety mode (don't import the root)
// 2: detect Family Safety mode and import the root
// (This is only relevant to Windows 8.1)
pref("security.family_safety.mode", 2);

pref("security.enterprise_roots.enabled", false);

pref("security.OCSP.enabled", 1);
pref("security.OCSP.require", false);
pref("security.OCSP.GET.enabled", false);

pref("security.pki.cert_short_lifetime_in_days", 10);
// NB: Changes to this pref affect CERT_CHAIN_SHA1_POLICY_STATUS.
// See the comment in CertVerifier.cpp.
// 3 = only allow SHA-1 for certificates issued by an imported root.
pref("security.pki.sha1_enforcement_level", 3);

// security.pki.name_matching_mode controls how the platform matches hostnames
// to name information in TLS certificates. The possible values are:
// 0: always fall back to the subject common name if necessary (as in, if the
//    subject alternative name extension is either not present or does not
//    contain any DNS names or IP addresses)
// 1: fall back to the subject common name for certificates valid before 23
//    August 2016 if necessary
// 2: fall back to the subject common name for certificates valid before 23
//    August 2015 if necessary
// 3: only use name information from the subject alternative name extension
pref("security.pki.name_matching_mode", 1);

// security.pki.netscape_step_up_policy controls how the platform handles the
// id-Netscape-stepUp OID in extended key usage extensions of CA certificates.
// 0: id-Netscape-stepUp is always considered equivalent to id-kp-serverAuth
// 1: it is considered equivalent when the notBefore is before 23 August 2016
// 2: similarly, but for 23 August 2015
// 3: it is never considered equivalent
pref("security.pki.netscape_step_up_policy", 1);

// Configures Certificate Transparency support mode:
// 0: Fully disabled.
// 1: Only collect telemetry. CT qualification checks are not performed.
pref("security.pki.certificate_transparency.mode", 0);

pref("security.webauth.u2f", false);
pref("security.webauth.u2f_enable_softtoken", false);
pref("security.webauth.u2f_enable_usbtoken", false);

// OCSP must-staple
pref("security.ssl.enable_ocsp_must_staple", true);

// Enable TLS 1.3 compatmode version for bad middleware boxes?
// This is a holdover from the later draft specs and SHOULD NOT be enabled by
// default. ONLY use this when you explicitly need it. You have been warned!
// Restart required.
pref("security.ssl.enable_tls13_compat_mode", false);

// Enable TLS 1.3 hello downgrade sentinel?
// One of the key protections offered by TLS 1.3 is preventing protocol downgrades
// as part of the initial handshake.
// Some domains, middleware and transparent routers may try to downgrade connections
// this way (which is a bad thing!). To allow users to connect anyway this
// check can be disabled here. Default is for the sentinel to be enabled, preventing
// bad downgrades of the protocol version.
pref("security.tls.hello_downgrade_check", true);

// If a request is mixed-content, send an HSTS priming request to attempt to
// see if it is available over HTTPS.
pref("security.mixed_content.send_hsts_priming", true);
// Don't change the order of evaluation of mixed-content and HSTS upgrades
pref("security.mixed_content.use_hsts", false);
