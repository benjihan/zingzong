### Zingzong command-line player

## Usage

    zingzong [OPTIONS] <song.4v> [<inst.set>]
    zingzong [OPTIONS] <music.4q>

### Options

 |Short| Long option    | Description                                        |
 |-----|----------------|----------------------------------------------------|
 |  -h | --help --usage | Print this message and exit.                       |
 |  -V | --version      | Print version and copyright and exit.              |
 |  -t | --tick=HZ      | Set player tick rate (default is 200hz).           |
 |  -r | --rate=[R,]HZ  | Set re-sampling method and rate (qerp,48K).        |
 |  -l | --length=TIME  | Set play time.                                     |
 |  -b | --blend=[X,]Y  | Set channel mapping and blending (see below).      |
 |  -m | --mute=CHANS   | Mute selected channels (bit-field or string).      |
 |  -i | --ignore=CHANS | Ignore selected channels (bit-field or string).    |
 |  -o | --output=URI   | Set output file name (`-w` or `-c`).               |
 |  -c | --stdout       | Output raw PCM to stdout or file (native 16-bit).  |
 |  -n | --null         | Output to the void.                                |
 |  -w | --wav          | Generated a .wav file (implicit if output is set). |


### Time

If time is not set the player tries to auto detect the music duration.
However a number of musics are going into unnecessary loops which
makes it hard to properly detect.

If time is set to zero `0` or `inf` the player will run forever.

 * 0 or `inf` represents an infinite duration
 * comma `,` dot `.` or double-quote `"` separates milliseconds
 * `m` or quote to suffix minutes
 * `h` to suffix hour


### Blending

Blending defines how the 4 channels are mapped to stereo output.
Channels are mapped by pairs. The left pair is channel A plus the
channel specified by `X`. `X` defaults to channel B. The right pair is
composed by the 2 remaining channels.

Both channels pairs are blended together according to `Y`.

 * `0`   is full panning `L=A+X / R=A+B+C+D-L`
 * `128` is centered `L=R=A+B+C+D`
 * `256` is full reversed panning `R=A+X / L=A+B+C+D-R`
 * Any value of `Y` in the range [0-256] is valid resulting in a linear
   blending such as:
	* `L = ( (A+X)*(256-Y) + (A+B+C+D-A-X)*Y )     ) / 256`
	* `R = ( (A+X)*Y       + (A+B+C+D-A-X)*(256-Y) ) / 256`


### Output

Options `-n/--null`,`-c/--stdout` and `-w/--wav` are used to set the
output type. The last one is used. Without it the default output type
is used which should be playing sound via the default or configured
libao driver.

The `-o/--output` option specify the output depending on the output
type.

 * `-n/--null` output is ignored.
 * `-c/--stdout` output to the specified file instead of `stdout`.
 * `-w/--wav` unless set output is a file based on song filename.


### Channel selection

 Select channels to be either muted or ignored. It can be either:
 
 * an integer representing a mask of selected channels (C-style prefix).
 * a string containing the letter A to D (case insensitive) in any order.
