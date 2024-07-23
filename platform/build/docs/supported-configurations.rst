.. _build_supported_configurations:

========================
Supported Configurations
========================

This page attempts to document supported build configurations.
For more up-to-date information please go to http://developer.palemoon.org/

Windows
=======

We support building on Windows 7 and newer operating systems using
Visual Studio 2015 U3.

The following are not fully supported (but may work):

* Building with a *MozillaBuild* Windows development
  environment not mentioned on the developer documentation site.
* Building with Mingw or any other non-Visual Studio toolchain.

OS X
====
(This section needs updating)
We support building on OS X 10.8 and newer with the OS X 10.8 SDK.

The tree should build with the following OS X releases and SDK versions:

* 10.8 Mountain Lion
* 10.9 Mavericks
* 10.10 Yosemite
* 10.11 El Capitan
* 10.12 Sierra
* 10.13 High Sierra
* 10.14 Mojave
* 10.15 Catalina
* 11 Big Sur (Including Apple ARM SoC)

The tree requires building with Apple's Clang 4.2 that ships with Xcode.
This corresponds to Xcode 4.6 and newer. Xcode 4.6 only runs on OS X 10.7.4
and newer.

The tree should build with GCC 7.1 and newer on OS X. However, this
build configuration isn't as widely used (and differs from what Mozilla
uses to produce OS X builds).

Linux
=====

Linux 2.6 and later kernels are supported.

Most distributions are supported as long as the proper package
dependencies are in place. ``configure`` will typically
detect missing dependencies and inform you how to disable features to
work around unsatisfied dependencies.
