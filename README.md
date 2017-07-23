# :musical_note: zingzong :musical_note:

An simple `Atari ST quartet` music file player.

Original `quartet` music consists of

  1. a `.set` file that contains the instruments.
  2. a `.4v` file  that contains the music score

Alternatively `zingzong` supports a convenient format that bundles
both files into a`.4q` file alongside a small info text.


## Download

:floppy_disk: [GitHub Release page](https://github.com/benjihan/zingzong/releases)


## Usage

    zingzong [OPTIONS] <inst.set> <song.4v> [output.wav]
    zingzong [OPTIONS] <music.q4> [output.wav]

### Options
    -h --help --usage  Print this message and exit.
    -V --version       Print version and copyright and exit.
    -t --tick=HZ       Set player tick rate (default is 200hz).
    -r --rate=[R,]HZ   Set re-sampling method and rate (qerp,48K).
                       Try `-hh' to print the list of [R]esampler.
    -l --length=TIME   Set play time.
    -m --mute=ABCD     Mute selected channels (bit-field or string).
    -c --stdout        Output raw sample to stdout (mono native 16-bit).
    -n --null          Output to the void.
    -w --wav           Generated a .wav file (implicit if output is set).
    -f --force         Clobber output .wav file.

### Time

If time is not set the player tries to auto detect the music duration.
However a number of musics are going into unnecessary loops which
makes it hard to properly detect. Detection threshold is set to 30
minutes.

If time is set to zero `0` or `inf` the player will run forever.

  * pure integer number to represent a number of ticks.
  * comma `,` to separate seconds and milliseconds.
  * `h` to suffix hours and `m` to suffix minutes.

### Output

If output is set it creates a `.wav` file of this name (implies `-w`).

Else with `-w` alone the `.wav` file is the song file stripped of its
path with its extension replaced by `.wav`. In other words the `.wav`
file is created in the current directory.

If output exists the program will refuse to create the file unless the
-f`/`--force` option is used or it is either empty or a RIFF file (4cc
test only).


## Building

### Tools

  * GNU `make` program. Other make programs might work as well but
    it is untested territory.
  * `gcc` the GNU C compiler and linker. Other C compiler and linker
    should work too but might require a bit of tweaking. Aside
    traditional variables have a look to `NODEPS` and `MAKERULES`.
  * `pkg-config` to help configure the dependency libraries.
    Alternatively each dependency can be configured manually by
    setting the corresponding //PACKAGE//_CFLAGS and the
    //PACKAGE//_LIBS variables. See below for the //PACKAGE// prefix.

### Dependencies

  * `liba0` is *highly* recommended unless you only want to build a
    plugin. It provides support for both audio output and RIFF wave
    file generation. It is usually distributed under the `libao-dev`
    package.
  * `soxr` is recommended for high quality resampling at acceptable
    performance for modern CPUs.
  * `libsamplerate` is another alternative for high quality resampling
    with a number quality versus performance trade off methods.


### Build script

There is a build script _build/build.sh. To use it create a directory
named after the host-triplet and call the build.sh script from there.
You can cross-compile with this script provided the environment is
properly setup for it. Make variables and targets can be set on the
command line just like you do with `make`. The script will set the
`MAKERULES` variable automatically if a `makerules` file exists in the
build directory.

    > mkdir _build/i686-w64-mingw32
    > cd _build/i686-w64-mingw32
    > ../build.sh NO_SOXR=0 SOXR_LDLIBS=-lsoxr


### Build directly with make

You can build from any directory including the source code `src` just
by specifying the Makefile location the `-f` option.

    > make -f ${zingzong_topdir}/src/Makefile

OR as the Makefile file should be executable:

    > ${zingzongs_topdir}/src/Makefile

You can set a variety of variables to alter the build. Beside
traditional variables such as `CC` or `CFLAGS`. There is a list of the
variables this Makefile uses.


### Make variables that alter the built

 | Make variable |                        Description                         |
 |---------------|------------------------------------------------------------|
 | `MAKERULES`   | Included if defined (to customize rules or whatever)       |
 | `NODEPS`      | Disable source dependency generation.                      |
 | `NO_AO`       | Set to 1 to disable libao support                          |
 | `NO_SOXR`     | Set to 0 to enable soxr support                            |
 | `NO_SRATE`    | Set to 0 to enable samplerate support                      |
 | `DEBUG`       | Set to defines below for default DEBUG CFLAGS              |
 | `PROFILE`     | Set to 1 for default PROFILING CFLAGS                      |
 | `PKGCONFIG`   | `pkg-config` program to use                                |

For example to build a win64 standalone executable with a cross gcc

      make \
        CC=x86_64-w64-mingw32-gcc \
        PKGCONFIG="x86_64-w64-mingw32-pkg-config --static" \
        CFLAGS="-O3 -static -static-libgcc" \
        NO_SRATE=1


### Preprocessor defines that alter the built

 |     Define    |                        Description                         |
 |---------------|------------------------------------------------------------|
 |`HAVE_CONFIG_H`|To include config.h                                         |
 | `DEBUG`       |Set to 1 for debug messages                                 |
 | `NDEBUG`      |To remove assert; automatically set if DEBUG is not defined |
 | `NO_AO`       |Define to disable libao support                             |
 | `WITH_SOXR`   |Set to 1 to enable soxr support                             |
 | `WITH_SRATE`  |Set to 1 to enable samplerate support                       |
 | `MAX_DETECT`  |Set maximum time detection threshold (in seconds)           |
 | `SPR_MIN`     |Set minimum sampling rate (default: 4 kHz)                  |
 | `SPR_MAX`     |Set maximum sampling rate (default: 96 kHz)                 |
 | `SPR_DEF`     |Set default sampling rate (default: 48 kHz)                 |


## License and copyright

  * Copyright (c) 2017 Benjamin Gerard AKA Ben/OVR
  * Licensed under MIT license


## Bugs

  Report bugs to [GitHub issues page](https://github.com/benjihan/zingzong/issues)
