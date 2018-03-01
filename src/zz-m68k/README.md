# Zingzong Microdeal Quartet player for ATARI ST and AMIGA

## zingzong binary player


### zingzong.bin stub

	+$00 bra.w player_init
	+$04 bra.w player_kill
	+$08 bra.w player_play


### Prototypes

	long player_init(bin_t * song, bin_t * vset, long dri, long spr);
	void player_kill(void);
	void player_play(void);

	typedef struct {
		void * ptr;   /* pointer to file data (if 0 use bin_t::dat[]) */
		long   max;   /* maximum size available in the buffer */
		long   len;   /* actual file size */
		char   dat[]; /* optional data buffer */
	} bin_t;

 * The player is compiled using gcc m68k C ABI in PC relative mode:
   * The parameters have to be pushed in the stack in backward order.
   * Return value is stored in register d0.
   * Registers d0/d1/a0/a1 are not saved.
 * The player expects to have sufficient privilege to access the hardware.
 * The stack usage is about 400 bytes.


#### Driver identifiers

 |  Value |           Driver         |             Remarks            |
 |--------|--------------------------|--------------------------------|
 |    0   |  Auto detect             | Amiga first then "_SND" cookie |
 |    1   |  Amiga/Paula             | Must be loaded in chipmem      |
 |    2   |  STf (PSG+Timer-A)       |                                |
 |    3   |  Ste (mono-8bit DMA)     |                                |
 |    4   |  Falcon (mono/16bit-DMA) | Could be much better           |


#### Sampling rate

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

 | Symbol  | Sampling | Direct sampling range      |
 |---------|----------|----------------------------|
 | `ZZ_FQ` |  12khz   | 7000-13999                 |
 | `ZZ_LQ` |  6.2khz  | < 7000                     |
 | `ZZ_MQ` |  25khz   | 14000-27999                |
 | `ZZ_HQ` |  50khz   | >=28000                    |

 * Falcon / DMA-16bit driver

 The current driver does not use the extra sampling rate available to
 the Falcon. Plus the falcon does not have a 6.2Khz mode. The ZZ_LQ
 mode fallback to 12khz.

### Examples

 Have a look at `test_tos.s` or `test_sndh.s.`
