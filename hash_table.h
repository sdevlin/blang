
#ifndef HASH_TABLE_H
#define HASH_TABLE_H

typedef unsigned (*hash_func_t) ( const char *key );

struct hash_table * hash_table_create( int buckets, hash_func_t func );
void                hash_table_delete( struct hash_table *h );

int    hash_table_size  ( struct hash_table *h );
int    hash_table_insert( struct hash_table *h, const char *key, const void *value, void **old );
void * hash_table_lookup( struct hash_table *h, const char *key );
void * hash_table_remove( struct hash_table *h, const char *key );

void   hash_table_firstkey( struct hash_table *h );
int    hash_table_nextkey( struct hash_table *h, char **key, void **value );

unsigned hash_string( const char *s );

#endif
