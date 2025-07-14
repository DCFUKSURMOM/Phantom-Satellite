# Phantom Satellite web browser

This is the source code for the Phantom Satellite web browser, it was originally started as a rebranded fork of Pale Moon to get around their branding drama.

It is slowly becoming something more

The name is a play on Pale Moon, Phantom = Pale (like a ghost) Satellite = Moon (the moon is a natural satellite)

Key differences:
* Custom Branding (Obviously, work in progress)
* Custom theme (MASSIVE WORK IN PROGRESS, it is recommended to use a dark theme on your system or a 3rd party theme on the browser itself for now)
* Working PPC64 support (a little rough around the edges but is usable)
* -mcrypto is no longer hard coded on PPC64, and is instead set in the CFLAGS in the config file. This allows the browser to work on PPC64 chips older than 2013, while still being able to have -mcrypto reenabled on chips that support (or require) it

I'm not opposed to applying any cool patches that are submitted or that I find myself, preferably patches that can be directly applied to stock Pale Moon so that updating the base for this browser can remain as simple as possible.

Features I would like to have (but arent currently planned because I have no idea where to start with them):

* Support for Windows XP and Windows Vista, I may be able to pull stuff from [here](https://github.com/roytam1/UXP), but no guarantees.

There are some Pale Moon forks that support Windows XP and Vista, but they are based on older Pale Moon versions

* Support for x86 CPUs that do not support SSE2 (This would allow the browser to run on a Pentium 3. Someome was able to get this working somewhat, but I don't know what all they did)

* Support for x86 CPUs that do not supprt SSE (This would allow the browser to run on a Pentium 2, and potentially the Pentium Pro and Pentium MMX. Maybe the same steps for non-sse2 cpus could be adapted for this?)

* Proper support for Android (depends on working ARM support)

Stock Pale Moon had Android support for a while, but it was removed, I thought there was still some traces in the code but I was apparently wrong.

* Support for Classic Mac OS X versions, including PowerPC Macs 

[Whitestar](https://github.com/dbsoft/White-Star), a PM fork with some experimental Mac stuff, does seem to be able to be built for Classic Mac OS, but its currently [broken](https://github.com/dbsoft/White-Star/issues/2)

* Proper support for iOS (depends on working ARM support)

Since modern Mac OS support is already here, this probably isnt that far fetched

Known issues:
* 32 bit ARM does not build at all, this issue is also present in upstream Pale Moon, it's possible im doing something wrong. 64 bit ARM was able to be built by a friend
* Builds on systems with musl, but does not run, something is broken in XPCOM, this issue is also present in upstream Pale Moon
* Hardware acceleration is broken on PowerPC, causes rendering issues, this issue is also present in upstream Pale Moon
* 32 bit Windows builds fail if optimized for size, something in mozavcodec, builds fine otherwise
* JS heavy sites can be slow or even unusable at times, seems to be hit or miss, this issue is also present in upstream Pale Moon, possible config issue
* JS performance is severely lacking on PowerPC due to the lack of jit support, this issue is also present in upstream Pale Moon
* Support for end-to-end encryption is either missing or incomplete, this issue is also present in upstream Pale Moon and breaks stuff like encrypted messages on Facebook, possible config issue

If you are intested in applying some of the changes from this browser to your own fork, you can find patches in the [patches](https://github.com/DCFUKSURMOM/Phantom-Satellite/tree/patches) branch

The configs I use for the builds in the release tab are in the mozconfigs directory.

You can build Phantom Satellite the same way you build stock Pale Moon using the instructions on the Pale Moon website

* [Build for Windows](https://developer.palemoon.org/build/windows/)
* [Build for Linux](https://developer.palemoon.org/build/linux/)
* [Pale Moon home page](http://www.palemoon.org/)
