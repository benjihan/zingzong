# Zingzong - Building help

## Requirements

  * `make`
  
  GNU make program. Other make programs might work as well but it is
  untested territory.

  * `gcc`
  
  GNU toolchain with C compiler and buddies. Other C compilers should
  work too but it might require a bit of tweaking.
	
  * `pkg-config`
  
  Get information on installed library. Alternatively each dependency
  can be configured manually by setting the corresponding
  *PACKAGE*_CFLAGS and the *PACKAGE*_LIBS variables. See below for the
  *PACKAGE* prefix.
  
  * `bash`
  
  GNU Bourne-Again SHell. Any decent Bourne shell should do. This is
  specially important when running the build helper script. Compiling
  directly with the Makefile should work with any shell as long as
  the `$(shell)` function works.


## Dependency libraries

  * `liba0` [The cross platform audio library](https://www.xiph.org/ao/) 
  
  *highly* recommended unless you only want to build a plugin. It
  provides support for live audio output and RIFF wave file
  generation. It is usually distributed under the `libao-dev` package.
	
	
  * `soxr` [The SoX Resampler library](https://sourceforge.net/p/soxr/wiki/Home)
  
  recommended for high quality resampling at acceptable performance for
  modern CPUs.
  
  
  * `libsamplerate` [Secret Rabbit Code](http://www.mega-nerd.com/SRC)
  
   a Sample Rate Converter for audio. That library provides a variety
   of resampling methods allowing quality to be traded off against
   computation cost. Some methods requires a heavy computation even
   for modern CPUS. Still run real-time but it might heavily impact
   your system performance).


## Building Zingzong

### Build script

There is a build script `_build/zz_build`. To use it create a directory
named after the host-triplet and call the zz_build script from there.
You can cross-compile with this script provided the environment is
properly setup for it. Make variables and targets can be set on the
command line just like you do with `make`. The script will set the
`MAKERULES` variable automatically if a `makerules` file exists in the
build directory.

    > mkdir _build/i686-w64-mingw32
    > cd _build/i686-w64-mingw32
    > ../zz_build NO_SOXR=0 SOXR_LDLIBS=-lsoxr


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
 | `gb_CFLAGS`   | Append to CFLAGS                                           |
 | `gb_LDLIBS`   | Append to LDLIBS                                           |
 | `gb_LDFLAGS`  | Append to LDFLAGS                                          |
 | `MAKERULES`   | Included if defined (to customize rules or whatever)       |
 | `NODEPS`      | Disable source dependency generation.                      |
 | `NO_AO`       | Set to 1 to disable libao support                          |
 | `NO_SOXR`     | Set to 0 to enable soxr support                            |
 | `NO_SRATE`    | Set to 0 to enable samplerate support                      |
 | `DEBUG`       | Set to defines below for default DEBUG CFLAGS              |
 | `PROFILE`     | Set to 1 for default PROFILING CFLAGS                      |
 | `PKGCONFIG`   | `pkg-config` program to use                                |
 

### Make variables for installing

The Makefile uses a subset of variables very similar to GNU packages.

 | Make variable |                        Description                         |
 |---------------|------------------------------------------------------------|
 | `DESTDIR`     | Support for staged installs                                |
 | `prefix`      | Prefix the default values of the variables listed below    |
 | `exec_dir`    | Prefix for executable and library files                    |
 | `bindir`      | Executable files                                           |
 | `libdir`      | Library and object files                                   |
 | `datadir`     | Prefix for architecture-independent data files             |
 | `includedir`  | Header files                                               |
 | `docdir`      | Documentation files                                        |
 | `man1dir`     | Section 1 man pages                                        |
 | `man1ext`     | The file name extension for installed section 1 man pages  |


      make \
	CC=x86_64-w64-mingw32-gcc \
	PKGCONFIG="x86_64-w64-mingw32-pkg-config --static" \
	gb_CFLAGS="-static" \
	gb_LDFLAGS="-static -static-libgcc" \
	NO_SRATE=0


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
