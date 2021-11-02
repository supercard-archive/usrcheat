#!/usr/bin/env python2

# turns action-replay cheat into C code.
# input is read from stdin in lines, where each line consists of
# one or more hexadecimal cheat tuple separated by a single space.
# the C code is written to stdout.

import sys

class arcode(object):
	def __init__(self, a, b):
		self.a = a
		self.b = b

f = sys.stdin
codes = []
while 1:
	s = f.readline()
	if s == '': break
	a = s.strip().split(' ')
	assert(len(a) % 2 == 0)
	while len(a):
		code = arcode(a[0], a[1])
		codes.append(code)
		a.pop(0) ; a.pop(0)

def byteswap(s): #44332211 -> 11223344
	return s[6:6+2] + s[4:4+2] + s[2:2+2] + s[0:0+2]
def hex_string(s):
	o = '' ; i = 0 ; l = len(s)
	while i+1 < l:
		o += "\\x%s"%(s[i:i+2])
		i += 2
	return o

indent = 0
def iprint(s):
	global indent
	o = '\t' * indent
	o += s
	print (o)

iprint("void cheat() {")
indent += 1

iprint("u32 offset = 0;")
iprint("u32 datareg = 0;")
iprint("u32 loop = 0;")
cnt = 0
while cnt < len(codes):
	code = codes[cnt]
	# iprint("// %s %s"%(code.a, code.b))
	if code.a[0] == '0':
		if code.a == '00000000':
			raise ValueError('hook codes not supported')
		iprint("*((u32*)0x%07x+offset) = 0x%08x;"%(int(code.a[1:], 16), int(code.b, 16)))
	elif code.a[0] == '1':
		if not code.b.startswith('0000'):
			raise ValueError('1x code parameter needs to start with 0000')
		iprint("*((u16*)0x%07x+offset) = 0x%04x;"%(int(code.a[1:], 16), int(code.b, 16)))
	elif code.a[0] == '2':
		if not code.b.startswith('000000'):
			raise ValueError('2x code parameter needs to start with 000000')
		iprint("*((u8*)0x%07x+offset) = 0x%02x;"%(int(code.a[1:], 16), int(code.b, 16)))
	elif code.a[0] == '3':
		iprint("if (0x%08x > *((u32*)0x%07x)) {"%(int(code.b, 16), int(code.a[1:], 16)))
		indent += 1
	elif code.a[0] == '4':
		iprint("if (0x%08x < *((u32*)0x%07x)) {"%(int(code.b, 16), int(code.a[1:], 16)))
		indent += 1
	elif code.a[0] == '5':
		iprint("if (0x%08x == *((u32*)0x%07x)) {"%(int(code.b, 16), int(code.a[1:], 16)))
		indent += 1
	elif code.a[0] == '6':
		iprint("if (0x%08x != *((u32*)0x%07x)) {"%(int(code.b, 16), int(code.a[1:], 16)))
		indent += 1
	# 7XXXXXXX ZZZZYYYY   IF YYYY > ((not ZZZZ) AND half[XXXXXXX])   ; [offset]
	elif code.a[0] == '7':
		iprint("if (0x%04x > ( (~0x%04x) & *((u16*)0x%07x) ) ) {"%(int(code.b[4:], 16), int(code.b[:4], 16), int(code.a[1:], 16)))
		indent += 1
	# 8XXXXXXX ZZZZYYYY   IF YYYY < ((not ZZZZ) AND half[XXXXXXX])   ; instead of
	elif code.a[0] == '8':
		iprint("if (0x%04x < ( (~0x%04x) & *((u16*)0x%07x) ) ) {"%(int(code.b[4:], 16), int(code.b[:4], 16), int(code.a[1:], 16)))
		indent += 1
	# 9XXXXXXX ZZZZYYYY   IF YYYY = ((not ZZZZ) AND half[XXXXXXX])   ; [XXXXXXX]
	elif code.a[0] == '9':
		iprint("if (0x%04x == ( (~0x%04x) & *((u16*)0x%07x) ) ) {"%(int(code.b[4:], 16), int(code.b[:4], 16), int(code.a[1:], 16)))
		indent += 1
	# AXXXXXXX ZZZZYYYY   IF YYYY <> ((not ZZZZ) AND half[XXXXXXX])  ;/
	elif code.a[0] in 'Aa':
		iprint("if (0x%04x != ( (~0x%04x) & *((u16*)0x%07x) ) ) {"%(int(code.b[4:], 16), int(code.b[:4], 16), int(code.a[1:], 16)))
		indent += 1
	# BXXXXXXX 00000000   offset = word[XXXXXXX+offset]
	elif code.a[0] in 'Bb':
		if code.b != '00000000':
			raise ValueError("Bx argument needs to be all zero")
		iprint("offset = *((u32*)0x%07x + offset);"%(int(code.a[1:], 16)))
	# C0000000 YYYYYYYY   FOR loopcount=0 to YYYYYYYY  ;execute Y+1 times
	elif code.a[0] in 'cC' and code.a[1:] == '0000000':
		iprint("for (loop = 0; loop <= 0x%08x; ++loop) {"%(int(code.b, 16)))
		indent += 1
	elif code.a[0] in 'cC' and code.a[1:] == '4000000':
		iprint("offset = 0x%08x;"%(int(code.b, 16)))
	elif code.a[0] in 'cC' and code.a[1:] == '5000000':
		iprint("if ((loop & 0x%04x) == 0x%04x) ++loop;"%(int(code.b[4:], 16), int(code.b[:4], 16)))
	elif code.a[0] in 'cC' and code.a[1:] == '6000000':
		iprint("*((u32*)0x%08x) = offset;"%(int(code.b, 16)))
	elif code.a[0] in 'dD' and code.a[1:] == '0000000':
		if code.b != '00000000':
			raise ValueError("D0000000 argument needs to be all zero")
		if indent <= 1:
			#raise ValueError("endif without if")
			iprint("// warning: endif without if")
		else:
			iprint("} // endif")
			indent -= 1
	elif code.a[0] in 'dD' and code.a[1:] == '1000000':
		if code.b != '00000000':
			raise ValueError("D1000000 argument needs to be all zero")
		iprint("} // next")
		if indent <= 1:
			raise ValueError("next without for")
		indent -= 1
	elif code.a[0] in 'dD' and code.a[1:] == '2000000':
		if code.b != '00000000':
			raise ValueError("D2000000 argument needs to be all zero")
		iprint("flush();")
		if indent <= 1:
			raise ValueError("next without for")
		while indent > 1:
			indent -= 1
			iprint("} // next/flush")
		iprint("offset = 0;")
		iprint("datareg = 0;")
	elif code.a[0] in 'dD' and code.a[1:] == '3000000':
		iprint("offset = 0x%08x;"%(int(code.b, 16)))
	elif code.a[0] in 'dD' and code.a[1:] == '4000000':
		iprint("datareg += 0x%08x;"%(int(code.b, 16)))
	elif code.a[0] in 'dD' and code.a[1:] == '5000000':
		iprint("datareg = 0x%08x;"%(int(code.b, 16)))
	# D6000000 XXXXXXXX   word[XXXXXXXX+offset]=datareg, offset=offset+4
	elif code.a[0] in 'dD' and code.a[1:] == '6000000':
		iprint("*((u32*)0x%08x + offset) = datareg;"%(int(code.b, 16)))
		iprint("offset += 4;")
	elif code.a[0] in 'dD' and code.a[1:] == '7000000':
		iprint("*((u16*)0x%08x + offset) = datareg;"%(int(code.b, 16)))
		iprint("offset += 2;")
	elif code.a[0] in 'dD' and code.a[1:] == '8000000':
		iprint("*((u8*)0x%08x + offset) = datareg;"%(int(code.b, 16)))
		iprint("++offset;")
	elif code.a[0] in 'dD' and code.a[1:] == '9000000':
		iprint("datareg = *((u32*)0x%08x + offset);"%(int(code.b, 16)))
	elif code.a[0] in 'dD' and code.a[1:] == 'a000000':
		iprint("datareg = *((u16*)0x%08x + offset);"%(int(code.b, 16)))
	elif code.a[0] in 'dD' and code.a[1:] == 'b000000':
		iprint("datareg = *((u8*)0x%08x + offset);"%(int(code.b, 16)))
	elif code.a[0] in 'dD' and code.a[1:] == 'c000000':
		iprint("offset += 0x%08x;"%(int(code.b, 16)))
	elif code.a[0] in 'eE':
		numb = int(code.b, 16)
		processed = 0
		data = ''
		while processed < numb:
			cnt += 1
			temp = codes[cnt]
			data += hex_string(byteswap(temp.a))
			data += hex_string(byteswap(temp.b))
			processed += 8
		iprint("memcpy((void*)(0x%07x+offset), \"%s\", %d);"%(int(code.a[1:], 16), data, numb))
		# EXXXXXXX YYYYYYYY   Copy YYYYYYYY parameter bytes to [XXXXXXXX+offset...]
		# 44332211 88776655   parameter bytes 1..8 for above code  (example)
		# 0000AA99 00000000   parameter bytes 9..10 for above code (padded with 00s)
		# raise("Exxxx copy opcode not implemented yet")
	# FXXXXXXX YYYYYYYY   Copy YYYYYYYY bytes from [offset..] to [XXXXXXX...]
	elif code.a[0] in 'fF':
		iprint("memcpy(((void*)0x%08x), ((void*)offset), 0x%07x);"%(int(code.b, 16), int(code.a[1:], 16)))
	else:
		raise ValueError("unknown opcode")
	cnt += 1

if indent != 1:
	iprint("// warning: loop or if not terminated")

while indent > 0:
	indent -=1
	iprint("}")

