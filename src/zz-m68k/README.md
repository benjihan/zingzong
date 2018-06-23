# Zingzong Microdeal Quartet player for ATARI ST and AMIGA

## zingzong binary player

 * The player is compiled using gcc m68k C ABI in PC relative mode:
   * The parameters have to be pushed in the stack in backward order.
   * Return value is stored in register d0.
   * Registers d0/d1/a0/a1 are not saved.
 * The player expects to have sufficient privilege to access the hardware.
 * The stack usage is about 400 bytes.


### zingzong.bin stub

	+$00|  bra.w  zingzong_init  ;; Initialize player and song
	+$04|  bra.w  zingzong_kill  ;; Stop player
	+$08|  bra.w  zingzong_play  ;; Run at tick rate (usually 200hz)
	+$0c|  bra.w  zingzong_mute  ;; Get/Set muted/ignored channels
	+$10|  bra.w  zingzong_cmap  ;; Get/Set channel mapping and blending
	+$14|  bra.w  zingzong_vers  ;; Get version string
	+$18|  bra.w  zingzong_stat  ;; Get current status (0=OK)
	+$1c|  bra.w  zingzong_samp  ;; Get effective sampling rate
	+$20|  bra.w  zingzong_driv  ;; Get internal dri_t struct
	+$24|  bra.w  zingzong_rate  ;; Get/Set player tick rate
	+$28|  bra.w  zingzong_core  ;; Get internal core_t struct


### Prototypes

	typedef struct {
		byte * ptr;   /* pointer to file data (if 0 use bin_t::dat[]) */
		long   max;   /* maximum size available in the buffer */
		long   len;   /* actual file size */
		byte   dat[]; /* optional data buffer */
	} bin_t;

	long  zingzong_init(bin_t * song, bin_t * vset, long dri, long spr);
	void  zingzong_kill(void);
	void  zingzong_play(void);
	byte  zingzong_mute(byte clr, byte set);
	long  zingzong_cmap(byte map, word lr8);
	char* zingzong_vers(void);
	byte  zingzong_stat(void);
	long  zingzong_samp(void);
	void* zingzong_driv(void);
	long  zingzong_rate(word rate);
	void* zingzong_core(void);


### Detailed information

	long  zingzong_init(bin_t * song, bin_t * vset, long dri, long spr);

#### Return value

`zingzong_init()` returns an error code (`0` on success).

#### Driver identifiers (long dri)

 If `dri.l` is greater than 255 it is interpreted as a pointer to an
 external driver provided by the caller. Otherwise it is an identifier
 for one of zingzong-m68k internal driver as described in the table
 below.

 |  Value |           Driver         |             Remarks            |
 |--------|--------------------------|--------------------------------|
 |   `0`  |  Auto detect             | Use "_SND" cookie (Atari only) |
 |   `1`  |  Amiga/Paula             | Must be loaded in chipmem.     |
 |   `2`  |  STf (PSG+Timer-A)       | System friendly (hopefully).   |
 |   `3`  |  STe (8-bit DMA)         | Support mono and stereo        |
 |   `4`  |  Falcon (2x16-bit-DMA)   | CPU only. Blended stereo.      |

#### Sampling rate (long spr)

 | Symbol  | Value |       Description           |
 |---------|-------|-----------------------------|
 |         |   0   |  Use song default sampling  |
 | `ZZ_FQ` |   1   |  Driver fast quality        |
 | `ZZ_LQ` |   2   |  Driver lowest quality      |
 | `ZZ_MQ` |   3   |  Driver medium quality      |
 | `ZZ_HQ` |   4   |  Driver highest quality     |
 |         | 4000+ |  Direct sampling rate       |

 Each drivers is responsible for handling the sampling rate. For
 example the Amiga driver simply ignore the user request as Paula has
 a set sampling rate of ~31khz. STe/Falcon DMA sampling rate is
 limited to a set of values. In that case the driver will try to match
 something reasonably close.

 * STe / DMA-8bit driver

 | Symbol  | Sampling | Direct sampling range |
 |---------|----------|-----------------------|
 | `ZZ_FQ` | 12517 hz | 7000-13999            |
 | `ZZ_LQ` |  6258 hz | < 7000                |
 | `ZZ_MQ` | 25033 hz | 14000-34999           |
 | `ZZ_HQ` | 25033 hz | 14000-34999           |
 |         | 50066 hz | >=35000               |

 Keep in mind that the 8mhz CPU is not enough to use the 50khz
 sampling rate.

 The STe driver comes with 3 distinct flavors. The flavor is selected
 during the init (`zingzong_init()`) depending on the left/right
 blending factor (see zingzong_cmap()')

 |   `lr8`   | STe driver flavor               |
 |-----------|---------------------------------|
 |   `128`   | Mono with boosted dynamic range |
 | `0`/`256` | Stereo with full separation     |
 |  Others   | Blended Stereo (default)        |


 * Falcon / DMA-16bit driver

 The Falcon driver is always playing in 16-bit stereo mode with CPU
 mixing. It fully supports channel mapping and blending.

 |   Rate   | Remark  |   Rate   | Remark  |   Rate   | Remark  |
 |----------|---------|----------|---------|----------|---------|
 |  8195 hz | `ZZ_LQ` |  9834 hz |         | 12292 hz | `ZZ_FQ` |
 | 12517 hz | STe     | 16390 hz |         | 19668 hz |         |
 | 24585 hz | `ZZ_MQ` | 25033 hz | STe     | 32780 hz | `ZZ_HQ` |
 | 49170 hz |         | 50066 hz | STe     |          |         |


--------------------------------------------------------------------------

	byte  zingzong_mute(byte clr, byte set);

  * bits `#4-7` represent muted channels A to D respectively.
  * bits `#0-3` represent ignored channels A to D respectively.

 If a bit is set the corresponding channel is muted or ignored.

 Muted channels are not sent to the mixer. More exactly they are
 forced into a stopped state. Muted channels still have their command
 sequence parsed normally and may for instance generate an
 error. Ignored channels are completely bypassed.

#### Parameters

* `clr` are the bits to clear in the mute/ignore control byte (NAND).
* `set` are the bits to set  in the mute/ignore control byte (OR).

#### Return value

`zingzong_mute()` returns the previous value of the mute/ignore
control byte.

#### Examples

	 zingzong_mute(0,0);      /* Read the current status */
	 zingzong_mute(15,0);     /* Ignore all */
	 zingzong_mute(0,255);    /* Play all */
	 zingzong_mute(255,val);  /* Set the status to `val` */

--------------------------------------------------------------------------

	long  zingzong_cmap(byte map, word lr8);

#### Return value

`zingzong_cmap()` returns the previous value of `map` and `lr8` packed
into a long word such as `d0.l = lr8.w:map.w`. If the player is not
running (before `zingzong_init()` or after `zingzong_stop()`) the
default values are affected.

#### Parameters

##### `byte map`

`map` set the channel mapping. That is how the 4 voices `A/B/C/D` are
mapped to the stereo output.

Use `-1` to read the current value (all unsupported values left the
parameter unchanged)


  | `map` |  Left pair (`lP`) | Right pair (`rP`) |
  |-------|-------------------|-------------------|
  | `-1`  |         unchanged | unchanged         |
  |  `0`  |          `lP=A+B` | `rP=C+D`          |
  |  `1`  |          `lP=A+C` | `rP=B+D`          |
  |  `2`  |          `lP=A+D` | `rP=B+C`          |


##### `word lr8`

`lr8` is the left/right blending factor. It's use to blend the L/R
pairs into the `L`/`R` stereo channels.

Use `-1` to read the current value (all unsupported values left the
parameter unchanged)

  | `lr8`  | Channel blending | Left out (`L`) | Right out (`R`) |
  |--------|------------------|----------------|-----------------|
  |  `-1`  | Read only        |    unchanged   |    unchanged    |
  |   `0`  | Full separated   |    `100%lP`    |    `100%rP`     |
  |  `64`  | Smooth blend     | `75%lP 25%rP ` |  `25%lP 75%rP`  |
  |  `128` | Mono             | `50%lP 50%rP`  |  `50%lP 50%rP`  |
  |  `192` | Smooth reversed  | `25%lP 75%rP`  |  `75%lP 25%rP`  |
  |  `256` | Full reversed    |    `100%rP`    |    `100%lP`     |


#### Notice

  * The channel mapping/blending only makes sens for stereo drivers.
  * Mapping should be supported by all stereo drivers.
  * Blending might not be fully supported by stereo drivers. However
	reversing the mapping using blending should always be supported.

--------------------------------------------------------------------------

### Examples

Have a look at

[test_tos.s](http://github.com/benjihan/zingzong/blob/master/src/zz-m68k/test_tos.s)
 or
[test_sndh.s](http://github.com/benjihan/zingzong/blob/master/src/zz-m68k/test_sndh.s)
