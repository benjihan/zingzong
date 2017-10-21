### Zingzong command-line player

## Usage

    zingzong [OPTIONS] <song.4v> [<inst.set>]
    zingzong [OPTIONS] <music.4q>

### Options
    -h --help --usage  Print this message and exit.
    -V --version       Print version and copyright and exit.
    -t --tick=HZ       Set player tick rate (default is 200hz).
    -r --rate=[R,]HZ   Set re-sampling method and rate (qerp,48K).
		               Try `-hh' to print the list of [R]esampler.
    -l --length=TIME   Set play time.
    -m --mute=ABCD     Mute selected channels (bit-field or string).
    -i --ignore=ABCD   Ignore selected channels (bit-field or string).
    -o --output=URI    Set output file name (`-w` or `-c`).
    -c --stdout        Output raw PCM to stdout or file (native 16-bit).
    -n --null          Output to the void.
    -w --wav           Generated a .wav file (implicit if output is set).

### Time

If time is not set the player tries to auto detect the music duration.
However a number of musics are going into unnecessary loops which
makes it hard to properly detect. Detection threshold is set to 30
minutes.

If time is set to zero `0` or `inf` the player will run forever.

 * 0 or `inf' represents an infinite duration
 * comma `,' dot `.' or double-quote `\"' separates milliseconds
 * `m' or quote to suffix minutes
 * `h' to suffix hour

### Output

Options `-n/--null`,'-c/--stdout' and `-w/--wav` are used to set the
output type. The last one is used. Without it the default output type
is used which should be playing sound via the default or configured
libao driver.

The `-o/--output` option specify the output depending on the output
type.

 * `-n/--null` output is ignored.
 * `-c/--stdout` output to the specified file instead of `stdout`.
 * `-w/--wav` unless set output is a file based on song filename.
