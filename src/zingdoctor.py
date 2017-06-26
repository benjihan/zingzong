#!/usr/bin/env python3
#
# zingdoctor.py -- A quartet file doctor

# SET file format
#
# offset        type    length  name            comments
# --------------------------------------------------------------------------------
# 0             byte    1       sampling rate   ?
# 1             byte    1       # of sample     +1 [X]
# 2             char[7] 20
# 142           long    20      sample offset
# 222           void     0
#
# Instruments:
# off+0        long     1       loop-point      fp16; -1 for no loop
# off+4        long     1       size            fp16
#
# AVR file format
#
# offset        type    length  name            comments
# --------------------------------------------------------------------------------
# 0     char    4       ID              format ID == "2BIT"
# 4     char    8       name            sample name (unused space filled with 0)
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
#                                       0=5.485 Khz, 1=8.084 Khz, 2=10.971 Khz,
#                                       3=16.168 Khz, 4=21.942 Khz, 5=32.336 Khz
#                                       6=43.885 Khz, 7=47.261 Khz
#                                       -1 (0xff)=no defined Frequence
# 23    byte    3       sample rate     in Hertz
# 26    long    1       size            in bytes (2*bytes in stereo)
# 30    long    1       loop begin      0 for no loop
# 34    long    1       loop size       equal to 'size' for no loop
# 38    byte    26      reserved        filled with 0
# 64    byte    64      user data
# 128   bytes   ?       sample data     (12 bits samples are coded on 16 bits :
#                                       0000 xxxx xxxx xxxx)
# -------------------------------------------------------------------------------
import sys,os
from struct import unpack
from math import log
import traceback

def info(s):
    sys.stdout.write(str(s))

def warn(s):
    sys.stderr.write("WARNING: "+str(s))

def error(s):
    sys.stderr.write("zingdoctor: "+str(s))

class Error(Exception):
    def __init__(self,msg,exit_code=1):
        self.exit_code = int(exit_code)
        super().__init__(msg)

# def step2note(istp):
#     return int( round (log(istp/65536.0,2.0) * 12.0 * 256.0) )

# def note2step(note):
#     return int(round(2.0**(note/(256*12.0))*65536.0))

# class Seq:

#     note_min = -24*256
#     note_max = +24*256

#     class Err : Exception:
#         pass

#     def self.check(clean=True):
#         c = chr(self.c)

#         if c == 'N':
#             if self.l < 1 or self.l > 0xFFFF:
#                 raise SeqErr('Invalid "N"ote length -- %04x'%self.l)
#             note = step2note(self.s)
#             if (note & 255) or note < note_min or note > note_max:
#                 raise SeqErr('Invalid "N"ote step -- %08x'%self.s)
#             if clean: self.p = 0

#         elif c == 'V':
#             if (self.p & 0xFFFFFF83):
#                 raise SeqErr('Invalid "V"oice parameter -- %08x'%self.p)
#             if clean:
#                 self.s = self.l = 0
#                 self.p = self.p & (31<<2)
#         elif c == 'R':
#             if self.l < 1 or self.l > 0xFFFF:
#                 raise SeqErr('Invalid "R"est length -- %04x'%self.l)
#             if clean: self.s = self.p = 0

#         elif c == 'S':
#             if self.l < 1 or self.l > 0xFFFF:
#                 raise SeqErr('Invalid "R"est length -- %04x'%self.l)
#             # We could probably the slide test range (self.p)
#             if clean: pass

#         elif c == 'l':
#             if clean: self.l = self.s = self.p = 0

#         elif c == 'L':
#             if clean:
#                 self.l = self.s = 0
#                 self.p = self.p & 0xFFFF0000
#             if not (self.p & 0xFFFF0000):
#                 raise Seq.Err('"L"oop count can not be 0')

#         elif c == 'F':
#             if clean: self.l = self.s = self.p = 0

#         else:
#             raise Seq.Err('unknown command -- %s %04x'%(repr(c),self.c))

#     def fromtuple(self, t):
#         self.c, self.l, self.s, self.p = t
#         self.check()

#     def fromstr(self.s):
#         self.fromtuple(s.unpack('>UULL'))

#     def __init__(self, cmd = None):
#         self.c = self.l = self.s = self.p = 0
#         if cmd is not None:
#             t = type(cmd)
#             if t is tuple: self.fromtuple(cmd)
#             else: self.fromstr(str(cmd))

# class Chan:
#     class Err : Exception: pass

#     def __init__(self, num):
#         if num <0 or num > 3:
#             raise Chan.Err('Invalid channel number -- %s'%repr(num))

#         self.num = num          # channel number [0..3]
#         self.seq = []           # sequence array
#         self.duration = None    # duration in tick (None=unknown)

# class Song:

#     def parse_header(self, buf, check = True):
#         khz, bar, spd, sig, null = buf.unpack('>UUU2B8s')
#         if check:
#             pass

#     def parse(self, buf):
#         l = len(buf)
#         if l < 16*4*12:
#             raise Song.Err('invalid song (too few data)')
#         self.parse_header(buf[0:16])
#         o = 16                  # offset in buffer
#         k = i = 0               # channel, row
#         while (o+11 < l):
#             seq = Seq(buf[o:o+12])
#             o += 12
#             cmd = chr(seq.c)

#             if chan is None:
#                 self.chan.append(Chan(k))
#                 chan = chan[-1]
#             chan.seq.append(seq)

#             if cmd == 'F':
#                 k,i = k+1
#                 chan = None


#             if k == 4: break

#         if k != 4:
#             raise Song.Err('invalid song (incomplete sequence #%d)'%k)
#         if o != l:
#             warn('%d garbage bytes at end of song' % l-o)


#     def __init__(self):
#         self.chan = []



class Avr:
    """ AVR sample format """

    spd = [ 0, 5485, 8084,10971,16168,21942,32336,43885,47261 ]

    class Err(Error): pass

    def __str__(self):
        return "AVR<%s%u %06u-%06u %d-kHz (%d)>" % \
            ( "us"[int(self.sign)], self.width,
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
            raise Exception('AVR invalid speed -- %u' % spd)
        if spr < 2000 or spr > 96000:
            raise Exception('AVR invalid sampling rate -- %u' % spr)

        if spd and spr and Avr.spd[spd] != spr:
            raise Exception('AVR conflictiong sampling rate -- %u/%u'
                             % (Avr.spd[spd],spr))

        self.fcc    = b[0]
        self.name   = b[1].decode().strip(' \x00')
        self.chans  = int(b[2]==0xffff)+1
        self.width  = b[3]
        self.sign   = b[4] == 0xffff
        self.loop   = b[5] == 0xffff
        self.midi   = (b[6]>>8, b[6]&0xFF)
        self.spd    = Avr.spd[spd] or spr
        self.spr    = spr or self.spd
        self.size   = b[8]
        self.lbeg   = b[9]
        self.lend   = b[10]
        self.user   = b[12]
        # extra
        self.llen   = 0
        if self.loop: self.llen = self.lend-self.lbeg

class Inst:

    class Err(Error): pass

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
            raise Inst.Err("I#%02d invalid name length (%d)" \
                           % (num,len(name)))
        if addr < 8 or addr >= datasz:
            raise Inst.Err("I#%02d start address out of range %d >= %d" \
                           % (num,addr,datasz))
        if addr+size > datasz:
            raise Inst.Err("I#%02d end address out of range %d > %d" \
                           % (num,addr+size,datasz))
        if loop > size:
            raise Inst.Err("I#%02d loop out of range %d > %d" \
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
        if addr & 1:
            raise Vset.Err('I#%02u: odd address -- %08x' % (i,addr))
        loop, size = unpack('>2L',data[addr-8:addr])
        if loop == 0xFFFFFFFF: loop = 0
        if size & 0xFFFF:
            raise Vset.Err('I#%02u: invalid size (MSW not 0) -- %08x'%(i,size))
        if loop & 0xFFFF:
            raise Vset.Err('I#%02u: invalid loop (MSW not 0) -- %08x'%(i,loop))
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
        if adr & 1: return None
        if adr < beg or adr+Vset.minInstrumentSize > end: return None

        try:
            inst = Inst.Decode(i,self.names,self.offs,self.data)
            assert adr == inst.addr-8
            assert inst.addr+inst.size <= len(self.data)

            spc  = adr - beg
            inst.avr = spc >= 120 \
                       and self.data[adr-120:adr-120+4] == b'2BIT'
            return inst
        except Vset.Err:
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
        recheck = True
        while recheck:
            recheck = False
            ordered = sorted([i for i in self.inst if i], key=lambda x: x.addr)
            self.nbi = len(ordered)

            # for ins in ordered:
            #     info("ORD "+str(ins)+"\n");

            # Check space between instruments
            pend = 222
            pred = None
            for i in range(len(ordered)+1):
                if recheck: break
                if i < len(ordered):
                    ins = ordered[i]
                    ins.order = i
                    beg = ins.addr-8
                    end = (ins.addr+ins.size+1) & -2
                else:
                    ins = None
                    end = beg = len(self.data)

                spc = beg-pend      # space in between instruments


                if spc < 0:
                    raise Vset.Err(
                        'I#%02d: overlaps on its predecessor by %d\n>> %s'\
                        % (ins.num, spc,str(pred)))

                if ins and ins.avr is None:
                    ins.avr = spc >= 120 and \
                              self.data[beg-120:beg-120+4] == b'2BIT'

                # Decode AVR header
                if ins and ins.avr:
                    beg -= 120
                    spc -= 120

                # info("\nbetween\n>> %s\n<< %s\n-> %d\n" %
                #      (str(pred),str(ins), spc))

                # Have some extra data. That might be unroll space
                # inserted or a lost instrument. Trying to decode it as an
                # instrument and keep it if its a valid one. We remove
                # unused instruments later on optimizing pass.

                # info("Looking for I#%02d in [%06u..%06u] +%d\n"
                #      % (self.nbi,pend,end,end-pend))
                inst = self.DiscoverThisInstrument(self.nbi,pend,beg)

                # This is a bit faster but discovering one by one
                # allow to discover in order. It might be safer but in
                # the other hand we migth miss instruments in case
                # there is a hole. We'll do more check in case the
                # song reference such instrument.
                #
                # inst = self.DiscoverOneInstrument(pend,end)

                if inst:
                    info("++ %s\n" % inst)
                    assert self.inst[inst.num] is None
                    self.inst[inst.num] = inst
                    recheck = True

                pend = end
                pred = ins

        # info("(%02d) %36s +%d\n" % (self.nbi,"-"*36,len(self.data)-pend))
        self.PrintInstruments()


    def DecodeInstruments(self):
        for i in range(0,self.nbi):
            self.inst[i] = Inst.Decode(i,self.names,self.offs,self.data)

    def PrintInstruments(self):
        info("-"*39 + "\n")
        info(str(self) + "\n")
        info("-"*39 + "\n")
        for i in self.inst:
            if i: info("%s\n"%i)
        info("-"*39 + "\n")

    def FromData(self, data):
        self.data = data
        self.DecodeHeader()
        self.DecodeInstruments()
        self.CheckAndDiscover()

    def __init__(self, data, name):
        self.name = name
        self.nbi = self.spr = 0
        self.inst = [ None ] * 20
        self.data = [ ]
        self.FromData(data)

def main(argc, argv):

    vsetpath = songpath = infopath = None
    vsetdata = songdata = infodata = None
    #
    vsetpath = argv[1]
    f = open(vsetpath,'rb')
    hd = f.read(20)
    qid, qsng, qset, qinf = unpack(">8s3L",hd)
    if qid == b"QUARTET\0":
        info("QUARTET detected: set:%u song:%u info:%u\n" %
             (qset, qsng, qinf))
        songpath = infopath = vsetpath
        songdata = f.read(qsng)
        vsetdata = f.read(qset)
        infodata = f.read(qinf)
        rem = f.read()
        if rem:
            warn("unexpected garbage at end of "+repr(vsetpath))

        vset = Vset(vsetdata,
                    os.path.splitext(os.path.basename(vsetpath))[0])

    else:
        vsetdat = hd+f.read()




    # for songpath in argv[2:]:
    #     pass

    # inp = open(argv[1],'rb')

    # hd = inp.read(222)
    # s = S.unpack('>BB140s20L',hd)
    # # print repr(s)

    # header = {
    #     'spr': s[0],
    #     'nbi': s[1]-1,
    #     'ins': [ ]
    # }

    # nbi = header['nbi']
    # if nbi > 20:
    #     raise Error('VSET invalid number of instrument', header['nbi'])

    # print 'SPR: %d\nNBI: %d' % ( header['spr'], header['nbi'] )

    # # nbi = 20
    # for i in range(nbi):
    #     x = header['ins']
    #     x.append( { 'name': s[2][i*7:i*7+7].strip(' \x00'),
    #                 'offset': s[3+i]
    #     } )
    #     print '#%02d: %s %d' % ( i+1, repr(x[-1]['name']), x[-1]['offset'])

    # for i in range(nbi):
    #     ins = header['ins'][i]
    #     if not ins['offset']: continue

    #     inp.seek(ins['offset']-128+8)

    #     avr,dat = read_avr(inp)
    #     print(avr)
    #     if avr['chans'] != 1:
    #         raise Error('AVR #%d is not mono' % i+1)
    #     if avr['width'] != 8:
    #         raise Error('AVR is not 8-bit (%d)'%avr['width'])


    #     if False:
    #         out = open('%02d-%s.avr' % (i+1, ins['name']), 'wb')
    #         out.write(dat)
    #         out.close()

    return 0

if __name__ == '__main__':
    try:
        code = main( len(sys.argv), sys.argv )
    except Error as e:
        error(str(e))
        code = e.exit_code
    except BaseException as e:
        traceback.print_exc()
        code = 128
    sys.exit(code)
