#!/usr/bin/python
#
# Compute zingzong-aga step to paula period LUT table.
#

if __name__ == "__main__":
    paula_pal  = 7093789.2
    paula_ntsc = 7159090.5
    R = lambda x : int(round(x))
    
    print "/* Sampling   PAL         NTSC       */"
    for khz in range(4,21):
        print '/* %02ukHz */ {' % khz, 
        for mode in range(2):
            if mode == 0:
                paula, sep = paula_pal, ','
            else:
                paula, sep = paula_ntsc, ' },'
            y = (65536. / 2000.) * paula
            x = y / khz
            print ('0x%08x'+sep) % R(x),
        print
