/**
 * \file lib/rpmhash.h
 * Hash table implemenation.
 */

#include <string.h>
// Hackery to make sure that macros get expanded
#define __JOIN(a,b) a##b
#define JOIN(a,b) __JOIN(a,b)
#define HASHPREFIX(name) JOIN(HASHTYPE,name)
#define HASHSTRUCT JOIN(HASHTYPE,_s)

typedef struct HASHSTRUCT * HASHTYPE;

/* function pointer types to deal with the datatypes the hash works with */

#define hashFunctionType JOIN(HASHTYPE,HashFunctionType)
#define hashEqualityType JOIN(HASHTYPE,HashEqualityType)
#define hashFreeKey JOIN(HASHTYPE,FreeKey)

typedef unsigned int (*hashFunctionType) (HTKEYTYPE string);
typedef int (*hashEqualityType) (HTKEYTYPE key1, HTKEYTYPE key2);
typedef HTKEYTYPE (*hashFreeKey) (HTKEYTYPE);

#ifdef HTDATATYPE
#define hashFreeData JOIN(HASHTYPE,FreeData)
typedef HTDATATYPE (*hashFreeData) (HTDATATYPE);
#endif

/**
 * Create hash table.
 * If keySize > 0, the key is duplicated within the table (which costs
 * memory, but may be useful anyway.
 * @param numBuckets    number of hash buckets
 * @param fn            function to generate hash value for key
 * @param eq            function to compare hash keys for equality
 * @param freeKey       function to free the keys or NULL
 * @param freeData      function to free the data or NULL
 * @return		pointer to initialized hash table
 */
RPM_GNUC_INTERNAL
HASHTYPE  HASHPREFIX(Create)(int numBuckets,
			     hashFunctionType fn, hashEqualityType eq,
			     hashFreeKey freeKey
#ifdef HTDATATYPE
, hashFreeData freeData
#endif
);

/**
 * Destroy hash table.
 * @param ht            pointer to hash table
 * @return		NULL always
 */
RPM_GNUC_INTERNAL
HASHTYPE  HASHPREFIX(Free)( HASHTYPE ht);

/**
 * Add item to hash table.
 * @param ht            pointer to hash table
 * @param key           key
 * @param data          data value
 */
RPM_GNUC_INTERNAL
void  HASHPREFIX(AddEntry)(HASHTYPE ht, HTKEYTYPE key
#ifdef HTDATATYPE
, HTDATATYPE data
#endif
);

#ifdef HTDATATYPE

/**
 * Retrieve item from hash table.
 * @param ht            pointer to hash table
 * @param key           key value
 * @retval data         address to store data value from bucket
 * @retval dataCount    address to store data value size from bucket
 * @retval tableKey     address to store key value from bucket (may be NULL)
 * @return		1 on success, 0 if the item is not found.
 */
RPM_GNUC_INTERNAL
int  HASHPREFIX(GetEntry)(HASHTYPE ht, HTKEYTYPE key,
		HTDATATYPE** data,
		int * dataCount,
		HTKEYTYPE* tableKey);
#endif

/**
 * Check for key in hash table.
 * @param ht            pointer to hash table
 * @param key           key value
 * @return		1 if the key is present, 0 otherwise
 */
RPM_GNUC_INTERNAL
int  HASHPREFIX(HasEntry)(HASHTYPE ht, HTKEYTYPE key);

/**
 * Print statistics about the hash to stderr
 * This is for debugging only
 * @param ht            pointer to hash table
 */
RPM_GNUC_INTERNAL
void HASHPREFIX(PrintStats)(HASHTYPE ht);
