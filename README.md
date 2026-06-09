# PCE Emulator

This is a fork of Hampa Hug's PCE, a emulator of various systems, including the Macintosh Plus and IBM PC 5150.

## Changes in this fork

### SDL3 driver

The original version supports X11, SDL1 and SDL2. I wanted to have a SDL3 driver for a few reasons:

* SDL2 is no longer under active development
* SDL3 has a nice integrated upscaling shader called 'Pixelart' which looks absolutely stunning

As SDL3 is quite different from SDL1/2 in some aspects and because I wanted to keep the source tree as clean as possible, I did not integrate the driver in the already existing SDL1/2 driver, but created a new one called 'sdl3'. To use this driver, set the driver to 'sdl3' in your configuration file.

## How to build it

My main interest is the Macintosh Plus emulation, so I outline building just this one here:

´´´
autoreconf -vis
./configure --disable-tun --disable-char-posix --disable-char-ppp --disable-char-pty --disable-char-slip --disable-char-tcp --disable-char-termios --disable-char-wincom --disable-readline --disable-atari-st --disable-cpm80 --disable-ibmpc --disable-ibmpc-rom --disable-rc759 --disable-sim405 --disable-sims32 --disable-simarm --disable-vic20 --disable-spectrum --disable-sound-oss --without-x --with-sdl=3
make -j $(nproc)
´´´

