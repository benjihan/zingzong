#!/usr/bin/env python3
#
# zingdoctor.py -- A quartet file doctor
#
# Copyright (c) 2017-2023 Benjamin Gerard AKA Ben^OVR
#
# ----------------------------------------------------------------------------
#
# ============================================================================
# .4V file format
# ----------------------------------------------------------------------------
# offset        type    length  name            comments
# ............................................................................
# 0             word    1       sampling rate   replay rate
# 2             word    1       measure         for the editor only
# 4             word    1       tempo           for the editor only
# 6             byte    1       time signature
# 7             byte    1       time signature
# 8             byte    8       reserved        (we might use that)
# 16            byte    12      sequence        1st sequence of channel #1
# 28            byte    12      sequence        next sequence ...
#
# Sequence
# ............................................................................
# 0             word    1       command         {P,R,S,V,l,L,F}
# 2             word    1       duration        never 0
# 4             long    1       step            sample step (fp16)
# 8             long    1       parameter
#
#
# ============================================================================
# .SET file format
# ----------------------------------------------------------------------------
# offset        type    length  name            comments
# ............................................................................
# 0             byte    1       sampling rate   ?
# 1             byte    1       # of sample     +1 [X]
# 2             char[7] 20
# 142           long    20      sample offset
# 222           void     0
#
# Instruments:
# ............................................................................
# off+0        long     1       loop-point      fp16; -1 for no loop
# off+4        long     1       size            fp16
# off+8        byte     size    pcm data        unsigned 8-bit PCM
#
#
# ============================================================================
# AVR file format
# ----------------------------------------------------------------------------
# offset        type    length  name            comments
# ............................................................................
# 0     char    4       ID              format ID == "2BIT"
# 4     char    8       name            sample name (0 filled)
# 12    short   1       mono/stereo     0=mono, -1 (0xffff)=stereo
#                                       With stereo, samples are alternated,
#                                       the first voice is the left :
#                                       (LRLRLRLRLRLRLRLRLR...)
# 14    short   1       resolution      8, 12 or 16 (bits)
# 16    short   1       signed or not   0=unsigned, -1 (0xffff)=signed
# 18    short   1       loop or not     0=no loop, -1 (0xffff)=loop on
# 20    short   1       MIDI note       0xffnn, where 0<=nn<=127
#                                       0xffff means "no MIDI note defined"
# 22    byte    1       Replay speed    Frequence in the Replay software
#                                       0=5.485-Khz  1=8.084-Khz, 2=10.971-Khz
#                                       3=16.168-Khz 4=21.942-Khz 5=32.336-Khz
#                                       6=43.885-Khz 7=47.261-Khz
#                                       -1 (0xff)=no defined Frequence
# 23    byte    3       sample rate     in Hertz
# 26    long    1       size            in bytes (2*bytes in stereo)
# 30    long    1       loop begin      0 for no loop
# 34    long    1       loop size       equal to 'size' for no loop
# 38    byte    26      reserved        filled with 0
# 64    byte    64      user data
# 128   bytes   ?       sample data     12 bits samples are coded on 16 bits
#                                       0000 xxxx xxxx xxxx
# ----------------------------------------------------------------------------

import sys, os, traceback
from struct import unpack, pack
from math import log, gcd
from getopt import gnu_getopt as getopt, error as GetOptError
from fractions import Fraction
from os.path import splitext, split

# ----------------------------------------------------------------------------
#  Globales
# ----------------------------------------------------------------------------

version     = 'zingdoctor 0.9.1'
opt_verbose = 0
opt_mode    = None
opt_unroll  = None

# ----------------------------------------------------------------------------
#  Messages
# ----------------------------------------------------------------------------

def dmsg(s):
    if opt_verbose >= 2:
        sys.stdout.write(str(s)+"\n")

def mesg(s):
    if opt_verbose >= 0:
        sys.stdout.write(str(s)+"\n")

def imsg(s):
    if opt_verbose >= 1:
        sys.stdout.write(str(s)+"\n")

def wmsg(s):
    if opt_verbose >= -1:
        sys.stderr.write("WARNING: "+str(s)+"\n")

error_object = None
def set_error_object(obj):
    global error_object
    save = error_object
    if obj: error_object = str(obj)
    else:   error_object = None
    return save

def emsg(s):
    e = "zingdoctor: "
    if error_object: e += error_object + ": "
    e += str(s)+"\n"
    sys.stderr.write(e)

# ----------------------------------------------------------------------------
#  Parent class for all our error exceptions
# ----------------------------------------------------------------------------

class Error(Exception):
    def __init__(self,msg,exit_code=1):
        self.exit_code = int(exit_code)
        super().__init__(msg)

######################################################################
#
# Song related class
#
######################################################################

def step2note(istp):
    return int( round (log(istp/65536.0,2.0) * 12.0 * 256.0) )

def note2step(note):
    return int(round(2.0**(note/(256*12.0))*65536.0))

class Seq:
    class Err(Error): pass

    note_min = -30*256
    note_max = +30*256

    def __str__(self):
        return '[%05d] %c %04X %08X %04X-%04X' \
            % (int(self.i), chr(self.c),
               self.l,self.s,self.p>>16,self.p&0xFFFF)

    def Check(self,clean=True):
        c = chr(self.c)

        if c == 'P':
            if self.l < 1 or self.l > 0xFFFF:
                raise Seq.Err('Invalid "P"lay length -- %04x'%self.l)
            note = step2note(self.s)
            if (note & 255) or note < Seq.note_min or note > Seq.note_max:
                raise Seq.Err('Invalid "P"lay step -- %08x (%04x)' \
                              %(self.s,note))
            if clean: self.p = 0

        elif c == 'V':
            if self.p & ~(31*4):
                raise Seq.Err('Invalid "V"oice parameter -- %08x'%self.p)
            if self.p > 20*4:
                raise Seq.Err('Invalid "V"oice number -- %u'%(self.p>>2))
            if clean: self.s = self.l = 0

        elif c == 'R':
            if self.l < 1 or self.l > 0xFFFF:
                raise Seq.Err('Invalid "R"est length -- %04x'%self.l)
            if clean: self.s = self.p = 0

        elif c == 'S':
            if self.l < 1 or self.l > 0xFFFF:
                raise Seq.Err('Invalid "R"est length -- %04x'%self.l)
            # We could probably the slide test range (self.p)
            if clean: pass

        elif c == 'l':
            if clean: self.l = self.s = self.p = 0

        elif c == 'L':
            if clean:
                self.l = self.s = 0
                self.p = self.p & 0xFFFF0000

        elif c == 'F':
            if clean: self.l = self.s = self.p = 0

        else:
            raise Seq.Err('unknown command -- %s %04x'%(repr(c),self.c))

    def FromTuple(self,t):
        self.c, self.l, self.s, self.p = t
        self.Check()

    def FromStr(self,s):
        self.FromTuple(unpack('>HHII',s))

    def __init__(self, cmd=None):
        self.c = self.l = self.s = self.p = self.i = None

        if cmd is not None:
            t = type(cmd)
            if t is tuple:
                self.FromTuple(cmd)
            else:
                self.FromStr(cmd)


class Loop:
    def __str__(self):
        return 'L{%u..%s}%sx%u' \
            % (self.beg, repr(self.end), repr(self.cnt), self.tic)

    def __init__(self, beg):
        self.beg = beg          # Loop point
        self.end = None
        self.cnt = None
        self.tic = 0

class Chan:
    class Err(Error): pass

    def __str__(self):
        return '%c[%u]/%s/%s' \
            % ( self.tag,
                len(self.seq),
                str(self.tic),
                str(self.lvl) )


    def __init__(self, num):
        if num < 0 or num > 3:
            raise Chan.Err('Invalid channel number -- '+repr(num))
        self.num = num          # channel number [0..3]
        self.tag = chr(65+self.num)
        self.seq = []           # sequence array
        self.tic = None         # duration in tick
        self.lvl = None         # maximum loop depth

class Song:
    class Err(Error): pass

    def __str__(self):
        return (
            'sng: "%s" / %2ukHz / %u / %u / %u:%u' %
             ( str(self.name), self.khz, self.bar, self.spd,
               *self.sig )
            ) \
            + "\n    " + str(self.chan[0]) \
            + " " + str(self.chan[1]) \
            + " " + str(self.chan[2]) \
            + " " + str(self.chan[3])


    def ParseHeader(self,buf):
        khz,bar,spd,sigm,sigd,res = unpack('>HHH2B8s',buf)

        if khz < 4 or khz > 20:
            raise Song.Err('sampling rate out of range -- %d'%khz)
        self.khz = khz

        if spd < 4 or spd > 40:
            raise Song.Err('tempo out of range range -- %d'%spd)
        self.spd = spd

        # $$$ TODO: add sanity check for those:
        self.bar = bar
        self.sig = (sigm,sigd)
        if res != b'\0'*8:
            wmsg('reserved data not nil')


    def Check(self, fix=False, nbi=None):

        inst = [ (0,0) ] * 20   # ref-count / play-count

        for chn in self.chan:
            chn.miss_loop = 0
            chn.tic = chn.lvl = 0
            loop = [ ]
            curi = None
            for seq in chn.seq:
                c = chr(seq.c)

                # Check "P"lay / "R"est / "S"lide
                if c in [ 'P', 'R', 'S' ]:
                    # Check duration against tempo
                    if seq.l % self.spd:
                        raise Song.Err(
                            'length is not a multiple tempo %d -- %s' \
                            % (self.spd), str(seq))
                    if loop:
                        loop[-1].tic += seq.l
                    else:
                        chn.tic += seq.l

                    # Check "P"lay
                    if c == 'P':
                        if curi is None:
                            raise Song.Err(
                                'No "V"oice set -- '+str(seq))
                        if nbi is not None and curi >= nbi:
                            raise  Song.Err(
                                '"P"lay invalid "V"oice #'
                                +str(curi)+' -- '+str(seq))
                        inst[curi] = ( inst[curi][0], inst[curi][1]+1 )

                # Check "V"oice
                elif c == 'V':
                    curi = seq.p//4
                    if nbi is None:
                        pass
                    elif curi >= nbi:
                        wmsg('"V"oice out of range (%d>=%d)\n>> %s'
                             % ( curi, nbi, str(seq)))

                # Check loop and count tics
                elif c == 'l':
                    loop.append( Loop(seq.i) )
                elif c == 'L':
                    cnt = (seq.p >> 16) + 1
                    if not loop:
                        lp = Loop(0)
                        loop.append(lp)
                        lp.tic = chn.tic # inherit channel tics
                        chn.tic = 0
                    lp = loop.pop()
                    lp.cnt, lp.end = cnt, seq.i

                    if lp.cnt == 1:
                        pass    # useless loop
                    elif lp.tic == 0:
                        raise Song.Err('Infinite loop '+str(lp))

                    if loop:
                        loop[-1].tic += lp.cnt * lp.tic
                    else:
                        chn.tic += lp.cnt * lp.tic
                    dmsg( chn.tag + ( ('.'*len(loop)) +
                            ( ' += %dx%d +%d'
                              % (lp.cnt,lp.tic,lp.cnt*lp.tic))))


                elif c == 'F':
                    pass

                else:
                    raise Song.Err('unexpected sequence\n>> '
                                   + chn.tag + str(seq))

                chn.lvl = max(int(chn.lvl),len(loop))

            if loop:
                for lp in loop[::-1]:
                    chn.tic += lp.tic
                    seq = chn.seq.pop(lp.beg)
                    wmsg('Delete loop point\n>>' + \
                         chn.tag + str(seq) + '\n>> ' + str(lp))
                    assert seq.c == ord('l') and seq.i == lp.beg
            dmsg( str(chn) )


        self.tic = max([ chn.tic for chn in self.chan ])
        if not self.tic:
            raise Song.Err('Empty song ?\n>> '+str(self))


        for i in range(4):
            for j in range(i+1,4):
                print( self.chan[i].tag,
                       self.chan[i].tic // self.spd,
                       self.chan[j].tag,
                       self.chan[j].tic // self.spd,
                       gcd ( self.chan[i].tic//self.spd,
                             self.chan[j].tic//self.spd ) )




        for chn in self.chan:
            if chn.tic and self.tic % chn.tic:
                wmsg('song duration '
                     + str(self.tic)
                     + ' not a multiple of channel duration'
                     + '\n>> ' + str(chn))


    def Parse(self, buf):
        l = len(buf)
        if l < 16+4*12:
            raise Song.Err('invalid song (too few data)')
        self.ParseHeader(buf[0:16])
        self.chan = [ None, None, None, None ]

        x = 0
        o = 16                  # offset in buffer
        k = i = 0               # channel, row
        chn = None              # current channel
        while (o+11 < l):
            try:
                seq = Seq(buf[o:o+12])
                seq.i = i
            except Seq.Err as e:
                x += 1
                if x < 2:
                    wmsg('%c[%u/%u] %s' % (chr(65+k),o,l,str(e)))
                else:
                    raise Song.Err('%c[%u/%u] %s' % (chr(65+k),o,l,str(e)))

            o,i,cmd = o+12, i+1, chr(seq.c)

            if self.chan[k] is None:
                chn = self.chan[k] = Chan(k)
                chn.off = o-12
            chn.end = o
            chn.seq.append(seq)

            if cmd == 'F':
                k,i,chn = k+1,0,None
                if k == 4: break

        for chn in self.chan:
            if not chn.seq or chr(chn.seq[-1].c) != 'F':
                wmsg('closing channel %c' % chn.tag)
                chn.seq.append(Seq((ord('F'),0,0,0)))

        # if k != 4:
        #     raise Song.Err('invalid song (incomplete sequence %c)'%chr(65+k))

        if o != l:
            wmsg('%d garbage bytes at end of song' % (l-o))


    def __init__(self, data, path):
        base = os.path.basename(path)
        self.name = os.path.splitext(base)[0]
        self.path = path
        self.khz = self.bar = self.spd = 0
        self.sig = (0,0)
        self.chan = [ None, None, None, None ]

        save = set_error_object(base)
        self.Parse(data)
        self.Check()
        set_error_object(save)


######################################################################
#
# Instrument related class
#
######################################################################

class Avr:
    """ AVR sample format """

    spd = [ 0, 5485, 8084,10971,16168,21942,32336,43885,47261 ]

    class Err(Error): pass

    def __str__(self):
        return "AVR<%s%ux%u %06u-%06u @%d>" % \
            ( "us"[int(self.sign)], self.width, self.chans,
              self.size, self.llen, self.spr
            )

    def __init__(self, data):
        b = unpack('>4s8sHHHHH4L26B64s',data)
        if b[0] != b'2BIT':
            raise Avr.Err('AVR missing signature')
        if b[2] not in [ 0, 0xffff ]:
            raise Avr.Err('AVR invalid channel -- %u'%b[2])
        if b[3] not in [ 8,12,16 ]:
            raise Avr.Err('AVR invalid width -- %u'%b[3])
        if b[4] not in [ 0, 0xffff ]:
            raise Avr.Err('AVR invalid sign -- %u'%b[4])
        if b[5] not in [ 0, 0xffff ]:
            raise Avr.Err('AVR invalid loop -- %u'%b[5])

        spd, spr = 0xFF & ((b[7] >> 24)+1), b[7] & 0xFFFFFF
        if spd >= len(Avr.spd):
            raise Avr.Err('AVR invalid speed -- %u' % spd)
        if spr < 2000 or spr > 96000:
            raise Avr.Err('AVR invalid sampling rate -- %u' % spr)

        # # This happen all the time. It seems spd is the best
        # # approximation of spr
        # if spd and spr and Avr.spd[spd] != spr:
        #     raise Avr.Err('AVR conflictiong sampling rate -- %u/%u'
        #                   % (Avr.spd[spd],spr))

        self.fcc   = b[0]
        self.name  = b[1]
        self.chans = int(b[2]==0xffff)+1
        self.width = b[3]
        self.sign  = b[4] == 0xffff
        self.loop  = b[5] == 0xffff
        self.midi  = (b[6]>>8, b[6]&0xFF)
        self.spd   = Avr.spd[spd] or spr
        self.spr   = spr or self.spd
        self.size  = b[8]
        self.lbeg  = b[9]
        self.lend  = b[10]
        self.user  = b[12]
        # extra
        self.llen  = 0
        if self.loop: self.llen = self.lend-self.lbeg


    def CheckAttr(self,name,value,fix=False):
        try:
            if not hasattr(self, name):
                raise Avr.Err("AVR M.I.A. -- "+name)
            current = getattr(self, name)
            if current != value:
                raise Avr.Err("AVR invalid "+name+" -- "+str(current))
        except Avr.Err as e:
            if not fix: raise
            wmsg(e)
            setattr(self, name, value)


    def Check(self,size=0,looplen=0):
        fix = bool(size)

        # Check for required sample format -- mono unsigned byte'
        self.CheckAttr('chans',1,fix)
        self.CheckAttr('width',8,fix)
        ## Too many AVR have this wrong, just ignore
        # self.CheckAttr('sign',False,fix)
        if fix: self.sign = False

        ## we could use AVR info to detect unrolled loop for some
        ## songs like TLB's. We might also test the loop pcm for
        ## silent.

        # if self.loop:
        #     if self.lend != self.size:
        #         wmsg("AVR loop end not at end -- " + str(self.lend-self.size))
        #         self.lend = self.size
        #         self.lbeg = max(0,min(self.lbeg,self.lend-1))
        # else:
        #     self.lbeg,self.lend = 0,self.size

        # if not fix: return

        # if self.size != size:
        #     wmsg("AVR size differ -- %d != %d" % (self.size,size))
        #     self.size = size

        # if self.loop != bool(looplen):
        #     wmsg("AVR loop status differ %s != %s" \
        #          % (str(self.loop), str(bool(looplen))))
        #     self.loop = bool(looplen)

        # self.lbeg = int(self.loop) * (self.size-looplen)
        # self.lend = self.size

class Inst:
    class Err(Error): pass

    maxsize = 128<<10           # 128KiB seems reasonnable

    def __eq__(self,other):
        return \
            type(self) == type(other) and \
            self.addr == other.addr and \
            self.size == other.size and \
            self.loop == other.loop

    def __str__(self):
        return 'I#%02u %6s [%06u+%06u-%06u] %s' \
            % (self.num, repr(self.name[0:6]),
               self.addr, self.size, self.loop,
               str(self.avr))

    def __init__(self,num,name,data,addr,size,loop):

        datasz = len(data)

        if num < 0 or num >= 20:
            raise Inst.Err("I#%02d out of range"%num)

        if len(name) != 7:
            raise Inst.Err("I#%02d invalid name length -- %d" \
                           % (num,len(name)))

        if addr & ~0xFFFFFE:
            raise Inst.Err("I#%02d odd address -- %d" % (num,addr))

        if addr < 8 or addr >= datasz:
            raise Inst.Err("I#%02d start address out of range -- %d >= %d" \
                           % (num,addr,datasz))

        if size <= 0 or size > Inst.maxsize:
            raise Inst.Err("I#%02d size out of range -- %d" % (num,size))


        if addr+size > datasz:
            if datasz & 1: datasz += 1
            if addr+size > datasz:
                raise Inst.Err("I#%02d end address out of range -- %d > %d" \
                               % (num,addr+size,datasz))
            else:
                wmsg("I#%02u is out of range but saved by alignment")

        if loop > size:
            raise Inst.Err("I#%02d loop out of range -- %d > %d" \
                           % (num,loop,size))

        self.num  = num
        self.name = name
        self.data = data
        self.addr = addr
        self.size = size
        self.loop = loop
        self.avr  = None

    def Decode(i, names, offs, data):
        """ Decode binary instrument info an Inst object. """

        if i < 0 or i > 20:
            raise Vset.Err("I#%02d out of range" % i)
        datasz = len(data)
        name = names[7*i:7*i+7]
        addr = offs[i]+8
        loop, size = unpack('>2L',data[addr-8:addr])
        if loop == 0xFFFFFFFF: loop = 0
        if size & 0xFFFF:
            raise Inst.Err('I#%02u: invalid size (MSW not 0) -- %08x'%(i,size))
        if loop & 0xFFFF:
            raise Inst.Err('I#%02u: invalid loop (MSW not 0) -- %08x'%(i,loop))
        size, loop = size >> 16, loop >> 16
        return Inst(i, name, data, addr, size, loop)


class Vset:
    """ Quartet instrument definition (.set) class. """

    minInstrumentSize = 8+16

    class Err(Error): pass

    def __str__(self):
        return 'set: "%s" / %d sounds / %02dkHz' \
            % (str(self.name),self.nbi,self.khz)

    def DecodeHeader(self):
        """ Decode .set header """

        datasz = len(self.data)
        if datasz < 222:
            raise Vset.Err('Not enough data for vset header -- %d'
                           % (datasz-222))
        # Decoding
        self.khz, self.nbi, self.names = unpack('>BB140s',self.data[0:142])
        self.offs = unpack('>20L',self.data[142:222])

        # Checking
        if self.khz < 4 or self.khz > 20:
            raise Vset.Err('sampling rate out of range -- %d'%self.khz)

        self.nbi -= 1
        if self.nbi < 1 or self.nbi > 20:
            raise Vset.Err('instrument count out of range -- %d'%self.nbi)


    def DiscoverThisInstrument(self, i, beg, end):
        """ Discover one specific instrument in range [beg..end]. """

        assert i >= 0 and i < 20
        assert self.inst[i] is None
        assert beg >= 0
        assert beg <= len(self.data)
        assert end >= beg

        end = min(end, len(self.data))

        if end-beg < Vset.minInstrumentSize: return None
        adr = self.offs[i]
        if adr & ~0xFFFFFE: return None
        if adr<beg or adr+Vset.minInstrumentSize>end: return None

        try:
            inst = Inst.Decode(i,self.names,self.offs,self.data)
            assert adr == inst.addr-8
            assert inst.addr+inst.size <= len(self.data)

            spc  = adr - beg
            inst.avr = spc >= 120 \
                       and self.data[adr-120:adr-120+4] == b'2BIT'
            return inst
        except Inst.Err:
            pass
        return None


    def DiscoverOneInstrument(self, beg, end):
        """ Discover one hidden instrument in range [beg..end]. """

        for i in range(20):
            if self.inst[i]: continue
            inst = self.DiscoverThisInstrument(i, beg, end)
            if inst: return inst
        return None


    def CheckAndDiscover(self):

        ordered = sorted([i for i in self.inst if i], key=lambda x: x.addr)

        # Warn for duplicate instruments. We don't do this in the
        # next  loop to avoid duplicate warnings.
        pred = None
        for ins in ordered:
            if ins == pred:
                wmsg("I#%02d and I#%02d are deplcates" % (pred.num,ins.num))
            pred = ins

        recheck = True
        while recheck:
            recheck = False

            # Order instrument by ascending address
            ordered = sorted([i for i in self.inst if i], key=lambda x: x.addr)
            self.nbi = len(ordered)

            # Check space between instruments
            pred, pbeg , pend =  None, 0, 222 # header
            for i in range(len(ordered)+1):
                if recheck: break
                is_first, is_last = i == 0, i == len(ordered)

                if not is_last:
                    ins = ordered[i]
                    ins.order = i
                    beg = ins.addr-8
                    end = ins.addr+ins.size
                    if ins == pred:
                        pred = ins # Not what it seems
                        continue
                else:
                    ins = None
                    beg = end = len(self.data)

                spc = beg-pend      # space in between instruments

                if spc < 0:
                    e = 'Instrument overlaps on its predecessor by %d' % spc
                    e += '\n>> '
                    if pred:
                        e += 'I#%02d ' % pred.num
                    else:
                        e += 'HEAD'
                    e += '%06d:%06d +%d' % (pbeg,pend,pend-pbeg)
                    e += '\n>> '
                    if ins:
                        e += 'I#%02d ' % ins.num
                    else:
                        e += 'END  '
                    e += '%06d:%06d +%d' % (beg,end,end-beg)
                    raise Vset.Err(e)

                if ins and ins.avr is None:
                    ins.avr = False
                    if spc >= 120:
                        fcc = self.data[beg-120:beg-120+4]
                        ins.avr = fcc == b'2BIT'
                        if not ins.avr and fcc[1:4] == b'BIT':
                            # Sometime the AVR is almost there
                            ins.avr = 'maybe'

                # Decode AVR header
                if ins and ins.avr:
                    try:
                        avrdata = bytearray(self.data[beg-120:beg+8])
                        avrdata[0] = ord('2')
                        avr = Avr(avrdata)
                        avr.Check(ins.size,ins.loop)
                        beg -= 120
                        spc -= 120
                        ins.avr = avr
                    except Avr.Err as e:
                        wmsg( ("I#%02d " % ins.num) + str(e))
                        ins.avr = False
                    # finally:
                    #     self.data[save[0]] = save[1]


                # Have some extra data. That might be unroll space
                # inserted or a lost instrument. Trying to decode it as an
                # instrument and keep it if its a valid one. We remove
                # unused instruments later on optimizing pass.
                #
                # DiscoverThisInstrument tries to discover lost instrument
                # in order.
                #
                # DiscoverOneInstrument will discover any instrument
                # that fit in the range.

                inst = None
                if self.nbi<20:
                    inst = self.DiscoverThisInstrument(self.nbi,pend,beg)

                # This is a bit faster but discovering one by one
                # allow to discover in order. It might be safer but in
                # the other hand we migth miss instruments in case
                # there is a hole. We'll do more check in case the
                # song reference such instrument.
                #
                # inst = self.DiscoverOneInstrument(pend,end)

                if inst:
                    self.modified += [ "Added "+str(inst) ]
                    # imsg("%s" % inst)
                    assert self.inst[inst.num] is None
                    self.inst[inst.num] = inst
                    recheck = True

                pred, pbeg, pend = ins, beg, end

        self.PrintInstruments()
        if self.modified:
            mesg("List of modifications:")
            for mod in self.modified:
                mesg("- %s"%mod)

    def DecodeInstruments(self):
        for i in range(0,self.nbi):
            self.inst[i] = Inst.Decode(i,self.names,self.offs,self.data)

    def PrintInstruments(self):
        imsg("="*37)
        mesg(str(self))
        imsg("-"*37)
        for i in self.inst:
            if i: imsg(i)
        imsg("-"*37)

    def FromData(self, data):
        self.data = data
        self.DecodeHeader()
        self.DecodeInstruments()

    def __init__(self, data, path):
        base = os.path.basename(path)
        name = os.path.splitext(base)[0]
        self.name = name
        self.path = path
        save = set_error_object(base)
        self.nbi = self.khz = 0
        self.inst = [ None ] * 20
        self.data = [ ]
        self.modified = [ ]
        self.FromData(data)
        self.CheckAndDiscover()
        set_error_object(save)

######################################################################
#
# Atari charset codec
#
######################################################################


atari_to_unicode=[
    0x00c7,0x00fc,0x00e9,0x00e2,0x00e4,0x00e0,0x00e5,0x00e7,
    0x00ea,0x00eb,0x00e8,0x00ef,0x00ee,0x00ec,0x00c4,0x00c5,
    0x00c9,0x00e6,0x00c6,0x00f4,0x00f6,0x00f2,0x00fb,0x00f9,
    0x00ff,0x00d6,0x00dc,0x00a2,0x00a3,0x00a5,0x00df,0x0192,
    0x00e1,0x00ed,0x00f3,0x00fa,0x00f1,0x00d1,0x00aa,0x00ba,
    0x00bf,0x2310,0x00ac,0x00bd,0x00bc,0x00a1,0x00ab,0x00bb,
    0x00e3,0x00f5,0x00d8,0x00f8,0x0153,0x0152,0x00c0,0x00c3,
    0x00d5,0x00a8,0x00b4,0x2020,0x00b6,0x00a9,0x00ae,0x2122,
    0x0133,0x0132,0x05d0,0x05d1,0x05d2,0x05d3,0x05d4,0x05d5,
    0x05d6,0x05d7,0x05d8,0x05d9,0x05db,0x05dc,0x05de,0x05e0,
    0x05e1,0x05e2,0x05e4,0x05e6,0x05e7,0x05e8,0x05e9,0x05ea,
    0x05df,0x05da,0x05dd,0x05e3,0x05e5,0x00a7,0x2227,0x221e,
    0x03b1,0x03b2,0x0393,0x03c0,0x03a3,0x03c3,0x00b5,0x03c4,
    0x03a6,0x0398,0x03a9,0x03b4,0x222e,0x03c6,0x2208,0x2229,
    0x2261,0x00b1,0x2265,0x2264,0x2320,0x2321,0x00f7,0x2248,
    0x00b0,0x2219,0x00b7,0x221a,0x207f,0x00b2,0x00b3,0x00af ]

######################################################################
#
# Usage
#
######################################################################

def print_usage():
    print("""\
Usage: zingdoctor.py [OPTIONS] file.4q ...
       zingdoctor.py [OPTIONS] file.set [ file1.4v ...] ...

Options:
 -h --help --usage ...... Display this message and exit
 -V --version ........... Print version and copyright and exit
 -d --demux ............. Demux .4q or .quar file
 -c --check ............. Only check (default)
 -f --fix ............... Fix error
 -u --unroll=N .......... Add N-bytes of loop unroll buffer"""
    )

def print_version():
    print("""\
%s

Copyright (c) 2017-2023 Benjamin Gerard AKA Ben^OVR
Licensed under MIT license""" % version)


######################################################################
#
# MAIN
#
######################################################################

def main(argc, argv):
    global opt_verbose, opt_mode, opt_unroll
    vsetpath = songpath = infopath = None
    vsetdata = songdata = infodata = None

    try:
        opts, args = getopt(argv, "hV" "vq" "dcf" "u:",
                            [ 'help','usage','version',
                              'verbose','quiet',
                              'check','fix','demux','unroll='
                            ])
    except GetOptError as e:
        raise Error(str(e))

    for opt,arg in opts:
        new_mod = None

        if opt in [ '-h','--help','--usage' ]:
            print_usage()
            return 0
        elif opt in [ '-V', '--version' ]:
            print_version()
            return 0

        elif opt in [ '-v', '--verbose' ]:
            opt_verbose += 1
        elif opt in [ '-q', '--quiet' ]:
            opt_verbose -= 1

        elif opt in [ '-d', '--demux' ]:
            new_mode = "demux"
        elif opt in [ '-c', '--check' ]:
            new_mode = "check"
        elif opt in [ '-f', '--fix' ]:
            new_mode = "fix"

        elif opt in [ '-u', '--unroll' ]:
            try:
                opt_unroll = int(arg)
                if opt_unroll < 0 or opt_unroll > 8192:
                    raise Error("option "+opt+" out of range -- %d"%opt_unroll)
            except ValueError:
                raise Error("option "+opt+" not an integer -- "+repr(arg))
        else:
            raise Error("option "+opt+" not implemented")

        if new_mode:
            if opt_mode and new_mode != opt_mode:
                raise Error("option "+opt+" incompatible with --"+opt_mode)
            opt_mode = new_mode

    opt_mode = opt_mode or "check"
    args = args[1:]
    if len(args) == 0:
        emsg('Missing argument. Try --help.')
        return 1

    path,i = args[0],1

    dmsg("Input file: %s" % repr(path))

    # "QUARTET" (.4q) file ?
    f = open(path,'rb')
    hd = f.read(8)
    if hd[:4] == b'QUAR':

        if hd != b"QUARTET\0":
            # .quar file
            # 00 'QUAR'
            # 04 offset from start to voice-set
            # 08 number of songs
            # 0C 1st song, offset from start
            # 10 2nd song, ...

            hd += f.read()
            voff, nbsong = unpack(">2L",hd[4:12])
            soff = unpack(">%dL"%nbsong, hd[12:12+4*nbsong])

            mesg("'SC68-QUAR (.quar)' file detected: set=%u songs:%u" %
                 (voff, nbsong))

            # Sort chunks by offset ( CHK-#, ( OFFSET, LENGTH ) )
            sort_chunks = \
                sorted([ (0,(voff,None)), (-1,(len(hd),-1)) ] +
                       [ (i+1,(off,None)) for i,off in enumerate(soff) ],
                       key=lambda x: x[1][0])
            assert sort_chunks[-1][0] == -1
            # Compute lengths
            for i in range(len(sort_chunks)-1):
                k,(o,l) = sort_chunks[i]
                assert l is None
                l = sort_chunks[i+1][1][0] - o
                sort_chunks[i] = ( k, ( o , l ) )
            del sort_chunks[-1]
            chunks = dict( sort_chunks )
            vsetpath = songpath = infopath = path
            vsetdata = hd[ chunks[0][0] : chunks[0][0]+chunks[0][1] ]
            songdata = [ hd[ chunks[i][0] : chunks[i][0]+chunks[i][1] ]
                         for i in range(1,nbsong+1) ]
        else:
            # .4q file
            qid = hd
            qsng, qset, qinf = unpack(">3L",f.read(12))
            mesg("QUARTET (.4q) detected: set=%u song=%u info=%u" %
                 (qset, qsng, qinf))
            vsetpath = songpath = infopath = path
            songdata = f.read(qsng)
            vsetdata = f.read(qset)
            infodata = f.read(qinf)

        rem = len(f.read())
        if rem:
            wmsg("%u unexpected garbage at end of %s" %
                 (rem,repr(path)))

        if opt_mode == 'demux':
            base = splitext(split(path)[1])[0]

            if songdata:
                if type(songdata) is list:
                    name_f = lambda n: base+'-%02u'%n+'.4v'
                else:
                    name_f = lambda n: base+'.4v'
                    songdata = [ songdata ]

                for i,sd in enumerate(songdata):
                    name = name_f(i+1)
                    with open(name,'wb') as wp:
                        print('saving song into %s (%u bytes)'
                              % (repr(name), wp.write(sd)))

            if vsetdata:
                name = base+'.set'
                with open(name,'wb') as wp:
                    print('saving instrument into %s (%u bytes)'
                          % (repr(name), wp.write(vsetdata)))

            if infodata:
                # GB: ultimately it would be better to convert from
                #     AtariST charset to UTF-8 using python codecs
                #     class.
                name = base+'.txt'
                with open(name,'wb') as wp:
                    print('saving info into %s (%u bytes)'
                          % (repr(name), wp.write(infodata)))

            return 0
    elif hd[:4].upper() == b'ICE!':
        raise Error("Input file is ICE! packed");
    else:
        vsetdata = hd+f.read()
        vsetpath = path

    vset = Vset(vsetdata,vsetpath)

    if songdata:
        if type(songdata) is not list:
            songdata = [ songdata ]

        for sd in songdata:
            song = Song(sd,songpath)
            mesg(str(song))


    return int(bool(vset.modified))

if __name__ == '__main__':
    try:
        code = main( len(sys.argv), sys.argv )
    except Error as e:
        emsg(str(e))
        code = e.exit_code
    except BaseException as e:
        traceback.print_exc()
        code = 128
    sys.exit(code)
