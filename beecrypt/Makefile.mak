#
# Makefile.mak for the beecrypt library
#
# To be used with Microsoft's nmake utility;
# Will need the Visual C Processor Pack installed.
#
# Copyright (c) 2000, 2001, 2002 Virtual Unlimited B.V.
#
# Author: Bob Deblier <bob@virtualunlimited.com>
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


ASFLAGS=/nologo /c /coff /Gd
CFLAGS=/nologo /TC /MT /GD /GM /Ox /G5 /I. # /ZI
LDFLAGS=/nologo /machine:IX86 /libpath:$(LIBPATH) $(LIBS) # /DEBUG
RCFLAGS=/r /L 0x409 /FObeecrypt.res
JAVAFLAGS=/DJAVAGLUE=1 /I$(JAVAPATH) /I$(JAVAPATH)\win32

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
	javaglue.obj \
	md5.obj \
	memchunk.obj \
	mp32.obj \
	mp32opt.obj \
	mp32barrett.obj \
	mp32number.obj \
	mp32prime.obj \
	mtprng.obj \
	rsa.obj \
	rsakp.obj \
	rsapk.obj \
	sha1.obj \
	sha1opt.obj \
	sha256.obj \
	timestamp.obj \
	beecrypt.res

	
all: .\beecrypt.dll .\beetest.exe

beecrypt.dll: $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) /dll /def:beecrypt.def /out:beecrypt.dll /implib:beecrypt.lib

beetest.obj: tests\beetest.c
	$(CC) $(CFLAGS) /Fobeetest.obj /c tests\beetest.c

beetest.exe: beecrypt.lib beetest.obj
	$(LD) $(LDFLAGS) beetest.obj beecrypt.lib

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

mp32opt.obj: masm\mp32opt.i386.asm
	$(AS) $(ASFLAGS) /Fomp32opt.obj /c masm\mp32opt.i386.asm

clean:
	del *.obj
