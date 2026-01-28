# Phantom Satellite web browser

This is the source code for the Phantom Satellite web browser, a fork of Pale Moon that aims to support older/niche platforms, without sacrificing support for more common/modern platforms.

The name is a play on Pale Moon, Phantom = Pale (like a ghost) Satellite = Moon (the moon is a natural satellite)

Key differences:
* Custom Branding (Obviously, work in progress)
* [Custom Theme](https://github.com/DCFUKSURMOM/cyanmoon)
* Working PPC64 support (a little rough around the edges but is usable)
* -mcrypto is no longer hard coded on PPC64, and is instead set in the compiler flags in the config file. This allows the browser to work on PPC64 chips older than 2013, while still being able to have -mcrypto reenabled on chips that support (or require) it
* Support for x86 CPUs that do not support SSE via compiler flag shenanigans (only tested on Linux, config is at mozconfigs/Linux/i586-nosse-gtk2.mozconfig, requires at least a Pentium MMX)
* Support for x86 CPUs that do not support SSE2 via compiler flag shenanigans (only tested on Linux, config is at mozconfigs/Linux/i686-nosse2-gtk2.mozconfig, requires a least a Pentium 3)
* The Linux nosse/nosse2 configs should work for stock PaleMoon as well.

I'm not opposed to applying any cool patches that are submitted or that I find myself, preferably patches that can be directly applied to stock Pale Moon so that updating the base for this browser can remain as simple as possible.

Minimum Requirements:
* Windows: Windows 7 or newer running on a Pentium 4 or newer (I have not figured out the compiler flags for older chips on Windows yet)
* Linux: Any reasonably up to date distro, running on a Pentium MMX or newer (depending on the config). The releases are currently built system libs on Artix (x64), ArchPower (PPC, PPC64), and Void Linux (i686) and may not run on distros with older packages, this can be fixed by updating those packages or recompiling the browser.
* Mac OS: Mac OS 10.7 on x86_64 or 11.0 on ARM64. PowerPC Mac OS is currently experimental thanks to [Whitestar](https://github.com/dbsoft/White-Star), but I do not yet have a build environment set up, I do not know the minimum version.

Disclaimer: Arch32, the distro I was testing i586 against, has finally broken after being poorly maintained for years. Until it is either fixed, or I find another option, i586 Linux builds are postponed. i686 and i686-nosse2 Linux builds have been moved to Void Linux for the same reason. x86_64 Linux builds are still on Artix Linux.

Features I would like to have (but arent currently planned because I have no idea where to start with them):
* Support for Windows XP and Windows Vista, I may be able to pull stuff from [here](https://github.com/Eclipse-Community/UXP), but no guarantees.
There are some Pale Moon forks that support Windows XP and Vista, but they are based on older Pale Moon versions.
* Proper support for Android (depends on working ARM support).
Stock Pale Moon had experimental Android support for a while, but it was removed, I thought there was still some traces in the code but I was apparently wrong.
* Proper support for iOS.
Since modern Mac OS support is already here, this probably isnt that far fetched.

Known issues:
* 32 bit ARM does not build at all, this issue is also present in upstream Pale Moon, it's possible im doing something wrong. 64 bit ARM was able to be built by a friend
* Hardware acceleration is broken on PowerPC Linux, causes rendering issues, this issue is also present in upstream Pale Moon
* 32 bit Windows builds fail if optimized for size, something in mozavcodec, builds fine otherwise
* JS heavy sites can be slow or even unusable at times, seems to be hit or miss, this issue is also present in upstream Pale Moon, possible config issue
* JS performance is severely lacking on PowerPC due to the lack of jit support, this issue is also present in upstream Pale Moon
* Support for end-to-end encryption is either missing or incomplete, this issue is also present in upstream Pale Moon and breaks stuff like encrypted messages on Facebook, possible config issue

If you are intested in applying some of the changes from this browser to your own fork, you can find patches in the [patches](https://github.com/DCFUKSURMOM/Phantom-Satellite/tree/patches) branch

The configs I use for the builds in the release tab are in the mozconfigs directory.

You can build Phantom Satellite the same way you build stock Pale Moon using the instructions on the Pale Moon website

* [Build for Windows](https://developer.palemoon.org/build/windows/)
* [Build for Linux](https://developer.palemoon.org/build/linux/)
* [Build for Mac OS](https://www.dbsoft.org/whitestar-build-mac.php)
* [Pale Moon home page](http://www.palemoon.org/)
