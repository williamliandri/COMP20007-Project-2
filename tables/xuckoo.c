/* * * * * * * * *
* Dynamic hash table using a combination of extendible hashing and cuckoo
* hashing with a single keys per bucket, resolving collisions by switching keys 
* between two tables with two separate hash functions and growing the tables 
* incrementally in response to cycles
*
* created for COMP20007 Design of Algorithms - Assignment 2, 2017
* by William Liandri (wliandri@student.unimelb.edu.au)
*
* The program is cited from xtndbl1.c and xtndbl1.h by Matt Farrugia with some
* modifications to suit the purpose.
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "xuckoo.h"

// macro to calculate the rightmost n bits of a number x
#define rightmostnbits(n, x) (x) & ((1 << (n)) - 1)

#define FOUND true // To indicate the key can be found in the table
#define NOT_FOUND false // To indicate the key cannot be found in the table

/*********************************** STRUCT **********************************/
// a bucket stores a single key (full=true) or is empty (full=false)
// it also knows how many bits are shared between possible keys, and the first 
// table address that references it
typedef struct bucket {
	int id;		// a unique id for this bucket, equal to the first address
				// in the table which points to it
	int depth;	// how many hash value bits are being used by this bucket
	bool full;	// does this bucket contain a key
	int64 key;	// the key stored in this bucket
} Bucket;

// helper structure to store statistics gathered
typedef struct stats {
	int time;		// how much CPU time has been used to insert/lookup keys
					// in this table
} Stats;

// an inner table is an extendible hash table with an array of slots pointing 
// to buckets holding up to 1 key, along with some information about the number 
// of hash value bits to use for addressing
typedef struct inner_table {
	Bucket **buckets;	// array of pointers to buckets
	int size;			// how many entries in the table of pointers (2^depth)
	int depth;			// how many bits of the hash value to use (log2(size))
	int nkeys;			// how many keys are being stored in the table
} InnerTable;

// a xuckoo hash table is just two inner tables for storing inserted keys
struct xuckoo_table {
	InnerTable *table1;
	InnerTable *table2;
	Stats stats;		// collection of statistics about this hash table
};

/****************************** HELPER FUNCTIONS *****************************/
// Helper function to initialise the InnerTable
static InnerTable *initialise_inner_table(InnerTable *innertable);

// Helper functions to create a new bucket first referenced from 
// 'first_address', based on 'depth' bits of its keys' hash values
static Bucket *new_bucket(int first_address, int depth);

// Helper function to free the memory of the InnerTable
void free_xuckoo_innertable(InnerTable *innertable);

// Helper function to double the table of bucket pointers, duplicating the
// bucket pointers in the first half into the new second half of the table
static void double_table(InnerTable *innertable);

// Helper function to reinsert a key into the hash table after splitting
// a bucket
static void reinsert_key(InnerTable *innertable, int64 key, int table_no);

// Helper function to split the bucket in 'table' at address 'address',
// growing table if necessary
static void split_bucket(InnerTable *innertable, int address, int table_no);

/**************************** FUNCTION DEFINITIONS ***************************/
// initialise an extendible cuckoo hash table
XuckooHashTable *new_xuckoo_hash_table() {
	XuckooHashTable *table = malloc(sizeof *table);
	assert(table);
	
	// Allocate memory for the first table
	table->table1 = initialise_inner_table(table->table1);
	
	// Allocate memory for the second table
	table->table2 = initialise_inner_table(table->table2);
	
	table->stats.time = 0;
	
	return table;
}


// free all memory associated with 'table'
void free_xuckoo_hash_table(XuckooHashTable *table) {
	assert(table);
	
	// Free the first inner table
	free_xuckoo_innertable(table->table1);
	
	// Free the second inner table
	free_xuckoo_innertable(table->table2);
	
	// Free the array of bucket pointers in the first and second table
	free(table->table1->buckets);
	free(table->table2->buckets);
	
	// Free the table struct itself
	free(table);
}


// insert 'key' into 'table', if it's not in there already
// returns true if insertion succeeds, false if it was already in there
bool xuckoo_hash_table_insert(XuckooHashTable *table, int64 key) {
	assert(table);
	int start_time = clock();
	
	int total_kicked_keys = 0, hash;
	int64 kick_key;
	InnerTable *innertable = table->table1;

	// Calculate table address
	int hash_1 = h1(key), hash_2 = h2(key);
	int address_1 = rightmostnbits(table->table1->depth, hash_1);
	int address_2 = rightmostnbits(table->table2->depth, hash_2);
	int address = address_1;
	
	// Check the key in table 1
	if (table->table1->buckets[address_1]->full && 
		table->table1->buckets[address_1]->key == key) {
		table->stats.time += clock() - start_time; // Add time elapsed
		return false;
	}
		
	// Check the key in table 2
	if (table->table2->buckets[address_2]->full && 
		table->table2->buckets[address_2]->key == key) {
		table->stats.time += clock() - start_time; // Add time elapsed
		return false;
	}
	
	// Check which table that has the least keys
	int insert_table = 1;
	if (table->table1->nkeys > table->table2->nkeys) {
		insert_table = 2;
	}
	
	// Try to find an empty slot and check if there is a cycle
	while (innertable->buckets[address]->full && 
		total_kicked_keys != 2*(table->table1->size)) {
		kick_key = innertable->buckets[address]->key;
		total_kicked_keys++;
		
		innertable->buckets[address]->key = key;
		key = kick_key;
		
		// If it kicked the key from the Table 1, need to insert the kicked 
		// key to the Table 2 
		if (insert_table == 1) {
			
			// Mark the next table that will be visited is table 2
			insert_table = 2;
				
			// Update the temp_table and the hash
			innertable = table->table2;
			address = rightmostnbits(innertable->depth, h2(key));
		}
		
		// If it kicked the key from the Table 2, need to insert the kicked 
		// key to the Table 1
		else if (insert_table == 2) {
			
			// Mark the next table that will be visited is table 2
			insert_table = 1;
				
			// Update the temp_table and the hash
			innertable = table->table1;
			address = rightmostnbits(innertable->depth, h1(key));
		}	
	}
	
	// The number of kicked key is equal to the 2 times the table size,
	// indicates that there is a cycling, so we need to grow the table
	if (total_kicked_keys == 2*(table->table1->size)) {
		
		// Make space on the smallest size table to have space to 
		// insert the key
		while (innertable->buckets[address]->full) {
			
			// If the first table has size smaller than or equal to the second
			// table's size, choose the first table
			if (table->table1->size <= table->table2->size) {
				innertable = table->table1;
				hash = h1(key);
				address = rightmostnbits(innertable->depth, hash);
				insert_table = 1;
			}
			
			// Otherwise, choose the second table
			else {
				innertable = table->table2;
				hash = h2(key);
				address = rightmostnbits(innertable->depth, hash);
				insert_table = 2;
			}
			
			split_bucket(innertable, address, insert_table);
			
			// and recalculate address because we might now need more bits
			address = rightmostnbits(innertable->depth, hash);
		}
	}
	
	// There is now space for the key, so we can just insert it
	innertable->buckets[address]->key = key;
	innertable->buckets[address]->full = true;
	innertable->nkeys++;
	
	table->stats.time += clock() - start_time; // Add time elapsed
	return true;
	
}


// lookup whether 'key' is inside 'table'
// returns true if found, false if not
bool xuckoo_hash_table_lookup(XuckooHashTable *table, int64 key) {
	assert(table);
	int start_time = clock(); // Start timing
	
	// Calculate the address on the first and second table for this key
	int address_table1 = rightmostnbits(table->table1->depth, h1(key));
	int address_table2 = rightmostnbits(table->table2->depth, h2(key));
	
	// Look for the key in that bucket (unless it's empty)

	// Check the key at the first table
	if (table->table1->buckets[address_table1]->full &&
		table->table1->buckets[address_table1]->key == key) {
		table->stats.time += clock() - start_time; // Add time elapsed
		return FOUND;
	}
	
	// Check the key at the second table
	if (table->table2->buckets[address_table2]->full &&
		table->table2->buckets[address_table2]->key == key) {
		table->stats.time += clock() - start_time; // Add time elapsed
		return FOUND;
	}
	
	return NOT_FOUND;
}


// print the contents of 'table' to stdout
void xuckoo_hash_table_print(XuckooHashTable *table) {
	assert(table != NULL);

	printf("--- table ---\n");

	// loop through the two tables, printing them
	InnerTable *innertables[2] = {table->table1, table->table2};
	int t;
	for (t = 0; t < 2; t++) {
		// print header
		printf("table %d\n", t+1);

		printf("  table:               buckets:\n");
		printf("  address | bucketid   bucketid [key]\n");
		
		// print table and buckets
		int i;
		for (i = 0; i < innertables[t]->size; i++) {
			// table entry
			printf("%9d | %-9d ", i, innertables[t]->buckets[i]->id);

			// if this is the first address at which a bucket occurs, print it
			if (innertables[t]->buckets[i]->id == i) {
				printf("%9d ", innertables[t]->buckets[i]->id);
				if (innertables[t]->buckets[i]->full) {
					printf("[%llu]", innertables[t]->buckets[i]->key);
				} else {
					printf("[ ]");
				}
			}

			// end the line
			printf("\n");
		}
	}
	printf("--- end table ---\n");
}


// print some statistics about 'table' to stdout
void xuckoo_hash_table_stats(XuckooHashTable *table) {
	assert(table);
	
	// Print some stats about state of the table 1
	printf("--- table 1 stats ---\n");
	printf("           current table 1 size: %d slots\n", table->table1->size);
	printf("total number of keys in table 1: %d\n", table->table1->nkeys);
	printf("                    load factor: %.3f%%\n\n", 
		table->table1->nkeys * 100.0 / table->table1->size);
	
	// Print some stats about state of the table 2
	printf("--- table 2 stats ---\n");
	printf("           current table 2 size: %d slots\n", table->table2->size);
	printf("total number of keys in table 2: %d\n", table->table2->nkeys);
	printf("                    load factor: %.3f%%\n\n", 
		table->table2->nkeys * 100.0 / table->table2->size);
	
	// Calculate CPU usage in seconds and print this
	float seconds = table->stats.time * 1.0 / CLOCKS_PER_SEC;
	printf("    CPU time spent: %.6f sec\n", seconds);
	
	printf("--- end stats ---\n");
}

/************************** INITIALISE INNER TABLE ***************************/
// Helper function to initialise the InnerTable
static InnerTable *initialise_inner_table(InnerTable *innertable) {
	innertable = malloc(sizeof (InnerTable));
	assert(innertable);
	
	innertable->size = 1;
	innertable->buckets = malloc(sizeof *innertable->buckets);
	assert(innertable->buckets);
	innertable->buckets[0] = new_bucket(0, 0);
	innertable->depth = 0;
	innertable->nkeys = 0;
	
	return innertable;
	
}

/******************************** NEW BUCKET *********************************/
// Helper functions to create a new bucket first referenced from 
// 'first_address', based on 'depth' bits of its keys' hash values
static Bucket *new_bucket(int first_address, int depth) {
	Bucket *bucket = malloc(sizeof *bucket);
	assert(bucket);
	
	bucket->id = first_address;
	bucket->depth = depth;
	bucket->full = false;
	
	return bucket;
	
}

/************************** FREE XUCKOO INNERTABLE ***************************/
// Helper function to free the memory of the InnerTable
void free_xuckoo_innertable(InnerTable *innertable) {
	int i;
	
	// Looping backwards through the array of pointers, freeing buckets only 
	// as we reach their first reference
	// (If we loop through forwards, we would not know which reference was
	// last)
	
	for (i = innertable->size - 1; i >= 0; i--) {
		if (innertable->buckets[i]->id == i) {
			free(innertable->buckets[i]);
		}
	}
}
 
/******************************* DOUBLE TABLE ********************************/
// Helper function to double the table of bucket pointers, duplicating the
// bucket pointers in the first half into the new second half of the table
static void double_table(InnerTable *innertable) {
	int size = innertable->size*2;
	assert(size < MAX_TABLE_SIZE && "error: table has grown too large!");
	
	// Get a new array of twiec as many bucket pointers
	innertable->buckets = realloc(innertable->buckets, 
		(sizeof *innertable->buckets) * size);
	assert(innertable->buckets);
	
	// Copy pointers down
	int i;
	for (i = 0; i < innertable->size; i++) {
		innertable->buckets[innertable->size + i] = innertable->buckets[i];	
	}
	
	// Finally, increase the table size and the depth we are using to hash
	// keys
	innertable->size = size;
	innertable->depth++;
	
}

/******************************* REINSERT KEY ********************************/
// Helper function to reinsert a key into the specified hash table after 
// splitting a bucket
static void reinsert_key(InnerTable *innertable, int64 key, int table_no) {
	int address, hash = h1(key);

	if (table_no == 2) {
		hash = h2(key);	
	}
	
	address = rightmostnbits(innertable->depth, hash);
	innertable->buckets[address]->key = key;
	innertable->buckets[address]->full = true;
}

/******************************* SPLIT BUCKET ********************************/
// Helper function to split the bucket in 'table' at address 'address',
// growing table if necessary
static void split_bucket(InnerTable *innertable, int address, int table_no) {
	
	// FIRST,
	// do we need to grow the table?
	if (innertable->buckets[address]->depth == innertable->depth) {
		// yep, this bucket is down to its last pointer
		double_table(innertable);
	}
	// either way, now it's time to split this bucket


	// SECOND,
	// create a new bucket and update both buckets' depth
	Bucket *bucket = innertable->buckets[address];
	int depth = bucket->depth;
	int first_address = bucket->id;

	int new_depth = depth + 1;
	bucket->depth = new_depth;

	// new bucket's first address will be a 1 bit plus the old first address
	int new_first_address = 1 << depth | first_address;
	Bucket *newbucket = new_bucket(new_first_address, new_depth);
	
	// THIRD,
	// redirect every second address pointing to this bucket to the new bucket
	// construct addresses by joining a bit 'prefix' and a bit 'suffix'
	// (defined below)

	// suffix: a 1 bit followed by the previous bucket bit address
	int bit_address = rightmostnbits(depth, first_address);
	int suffix = (1 << depth) | bit_address;

	// prefix: all bitstrings of length equal to the difference between the new
	// bucket depth and the table depth
	// use a for loop to enumerate all possible prefixes less than maxprefix:
	int maxprefix = 1 << (innertable->depth - new_depth);

	int prefix;
	for (prefix = 0; prefix < maxprefix; prefix++) {
		
		// construct address by joining this prefix and the suffix
		int a = (prefix << new_depth) | suffix;

		// redirect this table entry to point at the new bucket
		innertable->buckets[a] = newbucket;
	}

	// FINALLY,
	// filter the key from the old bucket into its rightful place in the new 
	// table (which may be the old bucket, or may be the new bucket)

	// remove and reinsert the key
	int64 key = bucket->key;
	bucket->full = false;
	reinsert_key(innertable, key, table_no);
}
