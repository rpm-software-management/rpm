#
# Makefile.mak for the beecrypt library
#
# To be used with Microsoft's nmake utility;
# Will need the Visual C Processor Pack installed.
#
# Copyright (c) 2000, 2001 Virtual Unlimited B.V.
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

DEFS= \
	win32/beecrypt.def

LIBS= \
	advapi32.lib \
	gdi32.lib \
	kernel32.lib \
	user32.lib \
	winmm.lib

LIBPATH="C:\Program Files\Microsoft Visual Studio\VC98\Lib"
JAVAPATH="C:\jdk1.3\include"


ASFLAGS=/nologo /c /coff /Gd
CFLAGS=/nologo /TC /MT /GD /Ox /G5 /DHAVE_CONFIG_H /I.
LDFLAGS=/nologo /machine:IX86 /libpath:$(LIBPATH) $(LIBS)
RCFLAGS=/r /L 0x409 /FObeecrypt.res
JAVAFLAGS=/DJAVAGLUE=1 /I$(JAVAPATH) /I$(JAVAPATH)\win32

OBJECTS= \
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
	elgamal.obj \
	endianness.obj \
	entropy.obj \
	fips180.obj \
	fips180opt.obj \
	fips186.obj \
	hmac.obj \
	hmacmd5.obj \
	hmacsha1.obj \
	hmacsha256.obj \
	javaglue.obj \
	md5.obj \
	mp32.obj \
	mp32opt.obj \
	mp32barrett.obj \
	mp32number.obj \
	mp32prime.obj \
	mtprng.obj \
	rsa.obj \
	rsakp.obj \
	rsapk.obj \
	sha256.obj \
	timestamp.obj \
	beecrypt.dll.obj \
	beecrypt.res

	
all: .\beecrypt.dll .\beetest.exe

beecrypt.dll: $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) /dll /def:$(DEFS) /out:beecrypt.dll /implib:beecrypt.lib

beetest.exe: beecrypt.lib beetest.obj
	$(LD) $(LDFLAGS) beetest.obj beecrypt.lib

beecrypt.dll.obj: win32/beecrypt.dll.c
	$(CC) $(CFLAGS) /c win32/beecrypt.dll.c

beecrypt.res: win32/beecrypt.rc
	$(RC) $(RCFLAGS) win32/beecrypt.rc

javaglue.obj: javaglue.c
	$(CC) $(CFLAGS) $(JAVAFLAGS) /c javaglue.c

blowfishopt.obj: win32/masm/blowfishopt.i586.asm
	$(AS) $(ASFLAGS) /Foblowfishopt.obj /c win32/masm/blowfishopt.i586.asm

fips180opt.obj: win32/masm/fips180opt.i586.asm
	$(AS) $(ASFLAGS) /Fofips180opt.obj /c win32/masm/fips180opt.i586.asm

mp32opt.obj: win32/masm/mp32opt.i386.asm
	$(AS) $(ASFLAGS) /Fomp32opt.obj /c win32/masm/mp32opt.i386.asm

clean:
	del *.obj
