# Phantom Satellite web browser

This is the source code for the Phantom Satellite web browser, a fork of Pale Moon that aims to support older/niche platforms, without sacrificing support for more common/modern platforms.

The name is a play on Pale Moon, Phantom = Pale (like a ghost) Satellite = Moon (the moon is a natural satellite)

This browser has nothing to do with the [fandom of the same name](https://book-of-heroes-and-villains.fandom.com/wiki/Phantom_Satellite), unfortunately that was discovered after I'd already named the browser.

Key differences:
* Custom Branding (Obviously, work in progress)
* [Custom Theme](https://github.com/DCFUKSURMOM/cyanmoon)
* Working PPC64 support (a little rough around the edges but is usable)
* -mcrypto is no longer hard coded on PPC64, and is instead set in the compiler flags in the config file. This allows the browser to work on PPC64 chips older than 2013, while still being able to have -mcrypto reenabled on chips that support (or require) it
* Support for x86 CPUs that do not support SSE via compiler flag shenanigans (only tested on Linux, config is at mozconfigs/Linux/i586-nosse-gtk2.mozconfig, requires at least a Pentium MMX)
* Support for x86 CPUs that do not support SSE2 via compiler flag shenanigans (only tested on Linux, config is at mozconfigs/Linux/i686-nosse2-gtk2.mozconfig, requires a least a Pentium 3)
* The Linux nosse/nosse2 configs should work for stock UXP as well.

I'm not opposed to applying any cool patches that are submitted or that I find myself, preferably patches that can be directly applied to stock UXP so that updating the base can remain as simple as possible.

Minimum Requirements:
* Windows: Windows Vista or newer running on a Pentium 4 or newer (I have not figured out the compiler flags for older chips on Windows yet)
* Linux: Any reasonably up to date distro, as long as it doesnt use musl (other libcs are untested), running on a Pentium MMX or newer (depending on the config).
* Mac OS: Mac OS 10.7 on x86_64 or 11.0 on ARM64. PowerPC Mac OS is supported upstream, but I do not yet have a build environment set up, I do not know the minimum version.

Features I would like to have (but arent currently planned because I have no idea where to start with them):
* Support for Windows XP
* Proper support for Android (depends on working ARM32 Linux support, ARM64 Linux is working).
Stock Pale Moon had experimental Android support for a while, but it was removed, I thought there was still some traces in the code but I was apparently wrong.
* Proper support for iOS.
Since modern Mac OS support is already here, this probably isnt that far fetched.

Known issues:
* Musl builds are unreliable, they either fail to build at all or have weird issues
* 32 bit ARM does not build at all for me, I know I'm doing something wrong because other UXP based browsers like Basilisk have armv7 builds.
* Hardware acceleration is broken on PowerPC Linux, causes rendering issues, this issue is also present in upstream UXP
* JS heavy sites can be slow or even unusable at times, seems to be hit or miss, this issue is also present in upstream UXP
* JS performance is severely lacking on PowerPC due to the lack of jit support, this issue is also present in upstream UXP
* Support for end-to-end encryption is either missing or incomplete, this issue is also present in upstream UXP and breaks stuff like encrypted messages on Facebook

The configs I use for the builds in the release tab are in the mozconfigs directory.

You can build Phantom Satellite the same way you build stock Pale Moon using the instructions on the Pale Moon website

* [Build for Windows](https://developer.palemoon.org/build/windows/)
* [Build for Linux](https://developer.palemoon.org/build/linux/)
* [Build for Mac OS](https://www.dbsoft.org/whitestar-build-mac.php)
* [Pale Moon home page](http://www.palemoon.org/)
