Linux config files used for both my personal builds and the ones in the releases:

i686-gtk2.mozconfig is the x86 (32 bit) GTK 2 config, if building on x86_64, you need to run "setarch i686" before building

x86_64-gtk2.mozconfig is the x86_64 (64 bit) GTK 2 config

x86_64-gtk3.mozconfig is the x86_64 (64 bit) GTK 3 config 

powerpc-gtk2.mozconfig is the PowerPC (32 bit) GTK2 config, if building on PPC64 you need to build in a PPC32 chroot and run "setarch ppc" before building

powerpc64-gtk2.mozconfig is the PowerPC (64 bit) GTK2 config

Special x86 config files:

i686-nosse2-gtk2.mozconfig is mostly identical to i686-gtk2.mozconfig, except the compiler have been modified to allow the browser to run without SSE2

i586-nosse-gtk2.mozconfig is mostly identical to i686-gtk2.mozconfig, except the compiler flags have been modified to allow the browser to run without SSE

i686-nosse2-gtk2.mozconfig requires at least a Pentium 3. i586-nosse-gtk2.mozconfig requires at least a Pentium MMX.

I did not disable any features in these configs, I only tweaked compiler flags, they can likely be optimized further.

These configs do not seem to work reliably when cross compiled the same way I cross compile the normal 32 bit x86 configs.

The most reliable way I have found to build them is to chroot into the root of the target machine, run "setarch=i686" (or "setarch=i586") and build it there

These configs were tested in QEMU using the i486 version of Arch32 (The i686 version has broken build tools, and I needed something that would boot on i586 anyways)

Different Linux distros and real hardware may behave differently.