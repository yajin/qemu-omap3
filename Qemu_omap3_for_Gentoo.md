# Introduction #

# Details #

1. build qemu-omap3
> a.  download ebuild files
> > http://qemu-omap3.googlecode.com/files/qemu-softmmu-20081127.tar.bz2


> b. put it into your overlay, eg. /usr/local/overlay/app-emulation/qemu-softmmu"

> c. note: you may probably use gcc-4, so before compile qemu,
please downgrade to gcc-3, and gcc-3.4.6 works for me.

#gcc-config -l

/etc/env.d/gcc
> [1](1.md) armv4tl-softfloat-linux-gnueabi-4.2.4 

> [2](2.md) i686-pc-linux-gnu-3.3.6
> [3](3.md) i686-pc-linux-gnu-3.4.6
> [4](4.md) i686-pc-linux-gnu-3.4.6-hardened
> [5](5.md) i686-pc-linux-gnu-3.4.6-hardenednopie
> [6](6.md) i686-pc-linux-gnu-3.4.6-hardenednopiessp
> [7](7.md) i686-pc-linux-gnu-3.4.6-hardenednossp
> [8](8.md) i686-pc-linux-gnu-4.1.2
> [9](9.md) i686-pc-linux-gnu-4.2.3
> [10](10.md) i686-pc-linux-gnu-4.3.2 **#gcc-config
  * Switching native-compiler to i686-pc-linux-gnu-3.4.6 ...               [ok ](.md)**

last shot:
#USE="sdl" emerge -1 qemu-softmmu

this will install this package.

2. generate qemu image file for beagleboad
> use script "beagle-board-nandflash.sh"
which could be downlod from http://qemu-omap3.googlecode.com/svn/trunk

  1. sage: ./beagle-board-nandflash.sh image destimage partition
> so, following steps will generate the running nand image file for beagleboard

./beagle-board-nandflash.sh x-load-bin.ift beagle-nand.bin x-loader

./beagle-board-nandflash.sh u-boot.bin beagle-nand.bin u-boot

3. run qemu
> qemu-system-arm -M beagle -mtdblock beagle-nand.bin
> it will pop out a "Qemu" window, "ctrl + shift + 2" will see u-boot running window