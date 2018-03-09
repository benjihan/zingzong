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
    +$10|  bra.w  zingzong_vers  ;; Get version string
    +$14|  bra.w  zingzong_stat  ;; Get current status (0=OK)
    +$18|  bra.w  zingzong_samp  ;; Get effective samplign rate
    +$1c|  bra.w  zingzong_driv  ;; Get internal dri_t struct
    +$20|  bra.w  zingzong_core  ;; Get internal core_t struct


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
    char* zingzong_vers(void);
    byte  zingzong_stat(void);
    long  zingzong_samp(void);
    void* zingzong_driv(void);
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
 |    0   |  Auto detect             | Amiga first then "_SND" cookie |
 |    1   |  Amiga/Paula             | Must be loaded in chipmem      |
 |    2   |  STf (PSG+Timer-A)       |                                |
 |    3   |  STe (mono-8bit DMA)     |                                |
 |    4   |  Falcon (16bit-DMA)      | Do not use the Falcon DSP.     |


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
 | `ZZ_MQ` | 25033 hz | 14000-27999           |
 | `ZZ_HQ` | 50066 hz | >=28000               |

 * Falcon / DMA-16bit driver

 The current driver does not use the extra sampling rate available to
 the Falcon. Plus the falcon does not have a 6.2Khz mode. The ZZ_LQ
 mode fallback to 12.5khz.

--------------------------------------------------------------------------

    byte  zingzong_mute(byte clr, byte set);
	
  * bits `#4-7` represent muted channels A to D respectively.
  * bits `#0-3` represent ignored channels A to D respectively.

 If a bit is set the corresponding channel is muted or ignored.
 
 Muted channels are not sent to the mixer. More exactly they are
 forced into stopped state. Muted channels still have the command
 sequence parsed normally and may for instance generate an error. In
 the contrary ignored channels that are completely bypassed.

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


### Examples

 Have a look at `test_tos.s` or `test_sndh.s.`
