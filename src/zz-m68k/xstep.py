#!/usr/bin/env python
#
# Compute zingzong-aga step to paula period LUT table.
#

if __name__ == "__main__":
    paula_pal  = 7093789.2
    paula_ntsc = 7159090.5
    R = lambda x : int(round(x))
    f = 20
    s = 2.**f

    m,M = 0x04C1B<<(f-16),0x50A28<<(f-16)

    print "/* Sampling   PAL         NTSC       */"
    for khz in range(4,21):
        hz = 1000. * khz
        print '/* %02ukHz */ {' % khz, 
        for mode in range(2):
            if mode == 0:
                paula, sep = paula_pal, ','
            else:
                paula, sep = paula_ntsc, ' },'
            x = s * paula / (2. * hz)
            r = R(x)
            print ('0x%08x /* %04u-%04u */'+sep) % \
                (r, r/m, r/M),
        print
