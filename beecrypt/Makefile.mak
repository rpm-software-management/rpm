
#
# Makefile.mak for the beecrypt library
#
# To be used with Microsoft's nmake utility;
# Will need the Visual C Processor Pack installed.
#
# Copyright (c) 2000, 2001, 2002 Virtual Unlimited B.V.
#
# Author: Bob Deblier <bob.deblier@pandora.be>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
# 

AS=ml.exe
CC=cl.exe
LD=link.exe
RC=rc.exe

LIBS= \
	advapi32.lib \
	gdi32.lib \
	kernel32.lib \
	user32.lib \
	winmm.lib

LIBPATH="C:\Program Files\Microsoft Visual Studio\VC98\Lib"
JAVAPATH="C:\j2sdk1.4.0\include"


# To enable SSE2 optimization, add switch /DUSE_SSE2 to ASFLAGS
ASFLAGS=/nologo /c /coff /Gd # /DUSE_SSE2
CFLAGS=/nologo /TC /MT /GM /Ox /G6 /I.
# CFLAGS=/nologo /TC /MT /GM /ZI /G6 /I.
LDFLAGS=/nologo /fixed:no /machine:IX86 /libpath:$(LIBPATH) $(LIBS) # /DEBUG
RCFLAGS=/r /L 0x409 /FObeecrypt.res
JAVAFLAGS=/DJAVAGLUE=1 /I$(JAVAPATH) /I$(JAVAPATH)\win32

# To compile Java support, add file javaglue.obj to this list
OBJECTS= \
	aes.obj \
	aesopt.obj \
	base64.obj \
	beecrypt.obj \
	blockmode.obj \
	blockpad.obj \
	blowfish.obj \
	blowfishopt.obj \
	dhaes.obj \
	dldp.obj \
	dlkp.obj \
	dlpk.obj \
	dlsvdp-dh.obj \
	dsa.obj \
	elgamal.obj \
	endianness.obj \
	entropy.obj \
	fips186.obj \
	hmac.obj \
	hmacmd5.obj \
	hmacsha1.obj \
	hmacsha256.obj \
	md5.obj \
	memchunk.obj \
	mp.obj \
	mpopt.obj \
	mpbarrett.obj \
	mpnumber.obj \
	mpprime.obj \
	mtprng.obj \
	rsa.obj \
	rsakp.obj \
	rsapk.obj \
	sha1.obj \
	sha1opt.obj \
	sha256.obj \
	timestamp.obj \
	beecrypt.res

all: .\beecrypt.dll

beecrypt.dll: $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) /dll /out:beecrypt.dll /implib:beecrypt.lib

beecrypt.res: beecrypt.rc
	$(RC) $(RCFLAGS) beecrypt.rc

javaglue.obj: javaglue.c
	$(CC) $(CFLAGS) $(JAVAFLAGS) /c javaglue.c

aesopt.obj: masm\aesopt.i586.asm
        $(AS) $(ASFLAGS) /Foaesopt.obj /c masm\aesopt.i586.asm

blowfishopt.obj: masm\blowfishopt.i586.asm
	$(AS) $(ASFLAGS) /Foblowfishopt.obj /c masm\blowfishopt.i586.asm

sha1opt.obj: masm\sha1opt.i586.asm
	$(AS) $(ASFLAGS) /Fosha1opt.obj /c masm\sha1opt.i586.asm

mpopt.obj: masm\mpopt.x86.asm
	$(AS) $(ASFLAGS) /Fompopt.obj /c masm\mpopt.x86.asm

clean:
	del *.obj
