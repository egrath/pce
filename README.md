# PCE Emulator

This is a fork of Hampa Hug's PCE, a emulator of various systems, including the Macintosh Plus and IBM PC 5150.

## Changes in this fork

### SDL3 driver

The original version supports X11, SDL1 and SDL2. I wanted to have a SDL3 driver for a few reasons:

* SDL2 is no longer under active development
* SDL3 has a nice integrated upscaling shader called 'Pixelart' which looks absolutely stunning

As SDL3 is quite different from SDL1/2 in some aspects and because I wanted to keep the source tree as clean as possible, I did not integrate the driver in the already existing SDL1/2 driver, but created a new one called 'sdl3'.

To use the new driver, you have to modify your configuration file to use 'sdl3' as your terminal driver and 'sdl3' as your sound driver.

For the sound driver modify your sound section:
```
driver="sdl3:wav=speaker.wav:lowpass=0:wavfilter=0"
```

For the video driver modify your terminal section:
```
driver="sdl3"

There is also a new option in the terminal section, named 'scale_quality' which controls the algorithm used for scaling the framebuffer to the output window. By default, it's set to 'pixelart' for SDL>=3.4 and 'linear' for SDL<3.4. The other option is nearest.
```

## How to build it

My main interest is the Macintosh Plus emulation, so I outline building just this one here:

```
autoreconf -vis
./configure --disable-tun --disable-char-posix --disable-char-ppp --disable-char-pty --disable-char-slip --disable-char-tcp --disable-char-termios --disable-char-wincom --disable-readline --disable-atari-st --disable-cpm80 --disable-ibmpc --disable-ibmpc-rom --disable-rc759 --disable-sim405 --disable-sims32 --disable-simarm --disable-vic20 --disable-spectrum --disable-sound-oss --without-x --with-sdl=3
make -j $(nproc)
```

