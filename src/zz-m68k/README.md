# Zingzong player for ATARI ST and AMiGA

## zingzong binary player

### zingzong.bin stub
    +$00 bra.w player_init
	+$04 bra.w player_kill
	+$08 bra.w player_play
	
### Prototypes
	long player_init(bin_t * song, bin_t * vset, long dri, long spr);
	void player_kill(void);
	void player_play(void);
	
	struct bin_t {
		void * ptr;   /* pointer to file data (if 0 use bin_t::dat[])
		long   max;   /* maximum size available in the buffer */
		long   len;   /* actual file size */
		char   dat[]; /* optional data buffer */
	};


### Examples

 Have a look at `test_tos.s` or `test_sndh.s.`
 
