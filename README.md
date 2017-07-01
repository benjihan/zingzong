# :musical_note: zingzong :musical_note:

An simple `Atari ST quartet` music file player.

Original `quartet` music consists of

  1. a `.set` file that contains the instruments.
  2. a `.4v` file  that contains the music score

Alternatively `zingzong` supports an convenient format that bundles both files into a `.4q` file alongside a small info text.

## Download

:floppy_disk: [GitHub Release page](https://github.com/benjihan/zingzong/releases)

## Usage

    zingzong [OPTIONS] <inst.set> <song.4v> [output.wav]
    zingzong [OPTIONS] <music.q4> [output.wav]

### Options
    -h --help --usage  Print this message and exit.
    -V --version       Print version and copyright and exit.
    -t --tick=HZ       Set player tick rate (default is 200hz).
    -r --rate=[M,]HZ   Set re-sampling method and rate (Best,48K).
     M := `qerp` ..... Lagrange quadratic interp
          `best` ..... Band limited sinc interp (145dB SNR, 96% BW)
          `medium` ... Band limited sinc interp (121dB SNR, 90% BW)
          `fast` ..... Band limited sinc interp ( 97dB SNR, 80% BW)
          `zoh` ...... Zero order hold interp (very fast, poor quality).
          `linear` ... Linear interp (very fast, poor quality).
    -l --length=TIME   Set play time.
    -m --mute=ABCD     Mute selected channels (bit-field or string).
    -w --wav           Generated a .wav file (implicit if output is set).
    -f --force         Clobber output .wav file.
    -c --stdout        Output raw sample to stdout.

### Time

If time is not set the player tries to auto detect the music duration. However a number of musics are going into unnecessary loops which makes it hard to properly detect. Detection threshold is set to 1 hour.

If time is set to zero `0` or `inf` the player will run for ever.

  * pure integer number to represent a number of ticks
  * comma `,` to separate seconds and milliseconds
  * `h` to suffix hours; `m` to suffix minutes

### Output

If output is set it creates a `.wav` file of this name (implies `-w`).

Else with `-w` alone the `.wav` file is the song file stripped of
its path with its extension replaced by `.wav`.

If output exists the program will refuse to create the file unless the -f`/`--force` option is used or it is either empty or a RIFF file (4cc test).

## Building

The provided Makefile should work in most GNU environment. You should have pkg-config. Dependency library/modules are:
  * `ao` module provides support for both audio output and RIFF wave file generation. It is usually distributed as `libao-dev` package. Alternatively you can set `AO_LIBS` and `AO_CFLAGS` directly or disable it by setting `AO_CFLAGS=-DNO_AO`.
  * `samplerate` module provides support for a variety of better resample methods. Without it zingzong still provide a relatively good output specially for the default output sampling rate. It is usually distributed as `libsamplerate-dev` package. Alternatively you can set `SRATE_LIBS` and `SRATE_CFLAGS` directly or disable it by setting `SRATE_CFLAGS=-DNO_SRATE`.


For example to build a win64 standalone executable with a cross gcc

      make \
        CC=x86_64-w64-mingw32-gcc \
        PKGCONFIG="x86_64-w64-mingw32-pkg-config --static" \
        CFLAGS="-O3 -static -static-libgcc" \
	SRATE_CFLAGS="-DNO_SRATE"

### Defines that alter the built

 |     Define    |                        Description                         |
 |---------------|------------------------------------------------------------|
 |`HAVE_CONFIG_H`|To include config.h                                         |
 |   `DEBUG`     |Set to 1 for debug messages                                 |
 |   `NDEBUG`    |To removes assert; automatically set if DEBUG is not defined|
 |   `NO_QERP`   |To disable quadratic interpolation                          |
 |  `NO_LIBAO`   |To ignore libao support                                     |
 |  `NO_SRATE`   |To ignore samplerate library support                        |
 | `MAX_DETECT`  |Set maximum time detection threshold (in seconds)           |
 |   `SPR_MIN`   |Set minimum sampling rate                                   |
 |   `SPR_MAX`   |Set maximum sampling rate                                   |
 |   `SPR_DEF`   |Set default sampling rate                                   |

## License and copyright

  * Copyright (c) 2017 Benjamin Gerard AKA Ben/OVR
  * Licensed under MIT license

## Bugs

  Report bugs to [GitHub issues page](https://github.com/benjihan/zingzong/issues)
