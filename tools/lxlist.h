/*
** Module   :LXLIST.H
** Abstract :
**
** Copyright (C) Sergey I. Yevtushenko
** Last Update :Sun  21-04-96
*/


#ifndef  __LXLIST_H
#define  __LXLIST_H

#define OffsetToLX 0x3C

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;

typedef struct
{
    byte _L;
    byte _X;
    byte BOrtder;
    byte WOrder;
    dword FormatLevel;
    word CPUType;
    word OSType;
    dword ModuleVersion;
    dword ModuleFlags;
    dword ModuleNumOfPages;
    dword EIP_NumObject;
    dword EIP;
    dword ESP_NumObject;
    dword ESP;
    dword PageSize;
    dword PageOffsetShift;
    dword FixupSectionSize;
    dword FixupSectionChecksum;
    dword LoaderSectionSize;
    dword LoaderSectionChecksum;
    dword ObjectTableOff;
    dword NumObjectsInModule;
    dword ObjectPageTableOff;
    dword ObjectIterPagesOff;
    dword ResourceTableOff;
    dword NumResourceTableEntries;
    dword ResidentNameTableOff;
    dword EntryTableOff;
    dword ModuleDirectivesOff;
    dword NumModuleDirectives;
    dword FixupPageTableOff;
    dword FixupRecordTableOff;
    dword ImportModuleTblOff;
    dword NumImportModEntries;
    dword ImportProcTblOff;
    /* other skipped */
} LXheader;

#endif //__LXLIST_H

