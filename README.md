# zingzong
An simple Atari ST quartet music file player

## Usage:

    zingzong [OPTIONS] <inst.set> <song.4v> [output.wav]

    A simple /|\ Atari ST /|\ quartet music file player

### Options:
    -h --help --usage  Print this message and exit (`-v' for more details).
    -V --version       Print version and copyright and exit.
    -t --tick=HZ       Set player tick rate (default is 200hz)
    -r --rate=HZ       Set sampling rate (default is 48kHz)
    -w --wav           Generated a .wav file (implicit if output is set)
    
### Output:
    If output is set it creates a .wav file of this name (implies `-w').
    Else with `-w' the .wav filename is the song filename stripped
    of its path and its .4v extension replaced by .wav.

## Building:

  The provided Makefile shoud work in most GNU environment. You must have pkg-config and its libao module usually provided by libao-dev package. Alternatively you can define AO_LIBS and AO_CFLAGS.
  
  For exemple to build a win64 standalone executable with a cross gcc
  
      make \
        CC=x86_64-w64-mingw32-gcc \
        PKGCONFIG="x86_64-w64-mingw32-pkg-config --static" \
        CFLAGS="-O3 -static -static-libgcc"
