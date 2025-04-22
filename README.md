# Phantom Satellite web browser

This is the source code for the Phantom Satellite web browser, it was started as a rebranded fork of Pale Moon to get around their branding drama.

The name is a play one Pale Moon, Phantom = Pale (like a ghost) Satellite = Moon (the moon is a natural satellite)

More information about the backstory can be found on the page I made for it on my website [Phantom Satellite Homepage](http://dcfuksurmom.duckdns.org/phantomsatellite)

The only real difference between this and stock Pale Moon at the moment is branding, at least for now.
There are also some fixes for PowerPC Linux being worked on, with one already in master, as well a fix for building on Linux systems with the musl libc (so far it seems to build but not run)
I would like for it to eventually become something more. I'm not opposed to applying any cool patches that are submitted or that I find myself, preferably patches that can be directly applied to stock Pale Moon so that updating the base for this browser can remain as simple as possible.

Features I would like to have (but arent currently planned because I have no idea where to start with them):

* Support for Windows XP and Windows Vista

There are some Pale Moon forks that support Windows XP and Vista, but they are based on older Pale Moon versions

* Support for x86 CPUs that do not support SSE2 (This would allow the browser to run on a Pentium 3)

* Support for x86 CPUs that do not supprt SSE (This would allow the browser to run on a Pentium 2, and potentially the Pentium Pro and Pentium MMX)

* Proper support for Android

Stock Pale Moon had Android support for a while, but it was removed and is unmaintained (and undocumented), however some traces are still present in the code

* Support for Classic Mac OS X versions, including PowerPC Macs

Stock Pale moon does have Mac OS support (but apparently no build documentation), it also supports the PowerPC architecture for Linux

There are some Pale Moon forks that support old Macs, but they are based on older Pale Moon versions

* Proper support for iOS

Since modern Mac OS support is already here, this probably isnt that far fetched

Due to a personal skill issue when I was half asleep, the contributor information is partially wrong. I accidentally uploaded without a fresh "git init" a while back, so it has the contributor and commit info from the normal Pale Moon repo. I don't know of any way to fix this, if its even possible...

If you are intested in applying some of the changes from this browser to your own fork, you can find patches in the [patches](https://github.com/DCFUKSURMOM/Phantom-Satellite/tree/patches) branch

The configs I use for the builds in the release tab are in the [configs](https://github.com/DCFUKSURMOM/Phantom-Satellite/tree/configs) branch

I will no longer be doing GTK 3 builds, GTK builds do not look or function any differently but they use more ram (I will keep the config file up and it will stay updated with any config tweaks for anyone that wants GTK3)

You can build Phantom Satellite the same way you build stock Pale Moon using the instructions on the Pale Moon website

* [Build for Windows](https://developer.palemoon.org/build/windows/)
* [Build for Linux](https://developer.palemoon.org/build/linux/)
* [Pale Moon home page](http://www.palemoon.org/)
* [Pale Moon Code of Conduct, Contributing, and UXP Coding style](https://repo.palemoon.org/MoonchildProductions/UXP/src/branch/master/docs)
