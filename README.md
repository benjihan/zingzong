# zingzong

An simple __Atari ST quartet__ music file player.

Original __quartet__ music consists of

  1. a __.set__ file that contains the instruments.
  2. a __.4v__ file  that contains the music score

Alternatively __zingzong__ supports an convenient format that bundles both
files into a __.4q__ file alongside a small info text.

## Usage:

    zingzong [OPTIONS] <inst.set> <song.4v> [output.wav]
    zingzong [OPTIONS] <music.q4> [output.wav]

### Options:
    -h --help --usage  Print this message and exit.
    -V --version       Print version and copyright and exit.
    -t --tick=HZ       Set player tick rate (default is 200hz)
    -r --rate=HZ       Set sampling rate (default is 48kHz)
    -w --wav           Generated a .wav file (implicit if output is set)
    
### Output:

If output is set it creates a __.wav__ file of this name (implies __-w__).

Else with __-w__ alone the __.wav__ file is the song file stripped of
its path with its extension replaced by __.wav__.  
 
If output exists the program will refuse to create the file unless it
is already a RIFF file (just a "RIFF" 4cc test)

## Building:

The provided Makefile should work in most GNU environment. You must
have pkg-config and its libao module usually provided by libao-dev
package. Alternatively you can define AO_LIBS and AO_CFLAGS.
  
For example to build a win64 standalone executable with a cross gcc
  
      make \
        CC=x86_64-w64-mingw32-gcc \
        PKGCONFIG="x86_64-w64-mingw32-pkg-config --static" \
        CFLAGS="-O3 -static -static-libgcc"


## License and copyright

  * Copyright (c) 2017 Benjamin Gerard AKA Ben/OVR
  * Licensed under MIT license

## Bugs

  Report bugs to <https://github.com/benjihan/zingzong/issues>
