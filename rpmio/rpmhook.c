#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define RPMHOOK_TABLE_INITSIZE  256
#define RPMHOOK_BUCKET_INITSIZE 5

typedef union {
    char *s;
    int i;
    float f;
    void *p;
} rpmhookArgv;

typedef struct {
    int argc;
    const char *argt;
    rpmhookArgv argv[1];
} rpmhookArgs;

typedef int (*rpmhookFunc)(rpmhookArgs *args, void *data);

typedef struct _rpmhookItem {
    rpmhookFunc func;
    void *data;
    struct _rpmhookItem *next;
} rpmhookItem;

typedef struct {
    unsigned long hash;
    char *name;
    rpmhookItem *item;
} rpmhookBucket;

typedef struct {
    int size;
    int used;
    rpmhookBucket bucket[1];
} rpmhookTable;

rpmhookTable *rpmhookTableNew(int size)
{
    rpmhookTable *table = (rpmhookTable *)calloc(1, sizeof(rpmhookTable)+
                                                 sizeof(rpmhookBucket)*
                                                 (size-1));
    table->size = size;
    return table;
}

rpmhookTable *rpmhookTableFree(rpmhookTable *table)
{
    rpmhookItem *item, *nextItem;
    int i;
    for (i = 0; i != table->size; i++) {
        if (table->bucket[i].name) {
            free(table->bucket[i].name);
            item = table->bucket[i].item;
            while (item) {
                nextItem = item->next;
                free(item);
                item = nextItem;
            }
        }
    }
    free(table);
}

void rpmhookTableRehash(rpmhookTable **table);

int rpmhookTableFindBucket(rpmhookTable **table, const char *name)
{
    /* Hash based on http://www.isthe.com/chongo/tech/comp/fnv/ */
    unsigned long perturb;
    unsigned long hash = 0;
    unsigned char *bp = (unsigned char *)name;
    unsigned char *be = bp + strlen(name);
    rpmhookBucket *bucket;
    if (((*table)->used/2)*3 > (*table)->size)
        rpmhookTableRehash(table);
    int ret;
    while (bp < be) {
        hash ^= (unsigned long)*bp++;
        hash *= (unsigned long)0x01000193;
    }
    perturb = hash;
    ret = hash % (*table)->size;
    bucket = &(*table)->bucket[ret];
    while (bucket->name &&
           (bucket->hash != hash || strcmp(bucket->name, name) != 0)) {
        /* Collision resolution based on Python's perturb scheme. */
        ret = ((ret << 2) + ret + perturb + 1) % (*table)->size;
        perturb >>= 5;
        bucket = &(*table)->bucket[ret];
    }
    if (!bucket->name)
        bucket->hash = hash;
    return ret;
}

void rpmhookTableRehash(rpmhookTable **table)
{
    rpmhookTable *newtable = rpmhookTableNew((*table)->size*2);
    int n, i = 0;
    for (; i != (*table)->size; i++) {
        if ((*table)->bucket[i].name) {
            n = rpmhookTableFindBucket(&newtable, (*table)->bucket[i].name);
            newtable->bucket[n].name = (*table)->bucket[i].name;
            newtable->bucket[n].item = (*table)->bucket[i].item;
        }
    }
    newtable->used = (*table)->used;
    free(*table);
    *table = newtable;
}

void rpmhookTableAddItem(rpmhookTable **table, const char *name,
                         rpmhookFunc func, void *data)
{
    int n = rpmhookTableFindBucket(table, name);
    rpmhookBucket *bucket = &(*table)->bucket[n];
    rpmhookItem **item = &bucket->item;
    if (!bucket->name) {
        bucket->name = strdup(name);
        (*table)->used++;
    }
    while (*item) item = &(*item)->next;
    *item = calloc(1, sizeof(rpmhookItem));
    (*item)->func = func;
    (*item)->data = data;
}

void rpmhookTableDelItem(rpmhookTable **table, const char *name,
                         rpmhookFunc func, void *data)
{
    int n = rpmhookTableFindBucket(table, name);
    rpmhookBucket *bucket = &(*table)->bucket[n];
    rpmhookItem *item = bucket->item;
    rpmhookItem *lastItem = NULL;
    while (item && item->func != func
           && (data == NULL || item->data == data)) {
        lastItem = item;
        item = item->next;
    }
    if (item) {
        if (lastItem)
            lastItem->next = item->next;
        else
            bucket->item = item->next;
        free(item);
        if (!bucket->item) {
            free(bucket->name);
            bucket->name = NULL;
            (*table)->used--;
        }
    }
}

rpmhookArgs *rpmhookArgsParse(const char *argt, va_list ap)
{
    int argc = strlen(argt);
    int i;
    rpmhookArgs *args =
        (rpmhookArgs *)malloc(sizeof(rpmhookArgs)+
                       (argc-1)*sizeof(rpmhookArgv));
    args->argc = argc;
    args->argt = argt;
    for (i = 0; i != argc; i++) {
        switch (argt[i]) {
            case 's':
                args->argv[i].s = va_arg(ap, char *);
                break;
            case 'i':
                args->argv[i].i = va_arg(ap, int);
                break;
            case 'f':
                args->argv[i].f = (float)va_arg(ap, double);
                break;
            case 'p':
                args->argv[i].p = va_arg(ap, void *);
                break;
            default:
                fprintf(stderr,
                    "error: unsupported type '%c' as "
                    "a hook argument\n");
                break;
        }
    }
    return args;
}

void rpmhookTableCall(rpmhookTable **table, const char *name,
                      const char *argt, ...)
{
    rpmhookItem *item;
    rpmhookArgs *args;
    int n;
    va_list ap;
    va_start(ap, argt);
    args = rpmhookArgsParse(argt, ap);
    va_end(ap);
    n = rpmhookTableFindBucket(table, name);
    item = (*table)->bucket[n].item;
    while (item) {
        item->func(args, item->data);
        item = item->next;
    }
    free(args);
}

int myhook(rpmhookArgs *args, void *data)
{
   printf("%s: %d\n", args->argv[0].s, data);
}

int main()
{
    rpmhookTable *table = rpmhookTableNew(RPMHOOK_TABLE_INITSIZE);
    int i;
    char buf[BUFSIZ];
    for (i = 0; i != 100000; i++) {
        sprintf(buf, "%d", i);
        rpmhookTableAddItem(&table, buf, myhook, (void *)i);
    }
    for (i = 0; i != 100000; i++) {
        sprintf(buf, "%d", i);
        rpmhookTableCall(&table, buf, "s", "Hello world!");
    }
    for (i = 0; i != 100000; i++) {
        sprintf(buf, "%d", i);
        rpmhookTableDelItem(&table, buf, myhook, (void *)i);
    }
    rpmhookTableFree(table);
    return 0;
}

/* vim:ts=4:sw=4:et
 */
