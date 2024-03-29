/* * * * * * * * *
 * Dynamic hash table using cuckoo hashing, resolving collisions by switching
 * keys between two tables with two separate hash functions
 *
 * created for COMP20007 Design of Algorithms - Assignment 2, 2017
 * by William Liandri (wliandri@student.unimelb.edu.au)
 * 
 * The program is cited from linear.c and linear.h by Matt Farrugia with
 * some modifications to suit the purpose
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "cuckoo.h"

#define USED true // To indicate the slot is used
#define NOT_USED false // To indicate the slot is still available
#define FOUND true // To indicate the key can be found in the table
#define NOT_FOUND false // To indicate the key cannot be found in the table


/*********************************** STRUCT **********************************/
// an inner table represents one of the two internal tables for a cuckoo
// hash table. it stores two parallel arrays: 'slots' for storing keys and
// 'inuse' for marking which entries are occupied
typedef struct inner_table {
	int64 *slots;	// array of slots holding keys
	bool  *inuse;	// is this slot in use or not?
} InnerTable;

// helper structure to store statistics gathered
typedef struct stats {
	int load_table1; // number of keys that have been inserted into Table 1
	int load_table2; // number of keys that have been inserted into Table 2
	int time;		// how much CPU time has been used to insert/lookup 
					// keys in this table
} Stats;

// a cuckoo hash table stores its keys in two inner tables
struct cuckoo_table {
	InnerTable *table1; // first table
	InnerTable *table2; // second table
	int size;			// size of each table
	int load;			 // total number of keys that have been inserted
	Stats stats;		 // collection of statistic about this hash table
};

/****************************** HELPER FUNCTIONS *****************************/
// The initialise_cuckoo_table and double_cuckoo_table is cited from Matt
// Farrugia with some modifications

// Helper functions to set up the internals of a cuckoo hash table struct with
// new arrays of size 'size'
static void initialise_cuckoo_table(CuckooHashTable *table, int size);

// Helper function to initialise the InnerTable
static InnerTable *initialise_inner_table(InnerTable *innertable, int size);

// Helper function to double the size of the cuckoo hash table
static void double_cuckoo_table(CuckooHashTable *table);

/**************************** FUNCTION DEFINITIONS ***************************/
// initialise a cuckoo hash table with 'size' slots in each table
CuckooHashTable *new_cuckoo_hash_table(int size) {
	CuckooHashTable *table = malloc(sizeof *table);
	assert(table);
	
	// Set up the internals of the table struct with arrays of size 'size'
	initialise_cuckoo_table(table, size);
	
	table->stats.time = 0;
	
	return table;
}

// free all memory associated with 'table'
void free_cuckoo_hash_table(CuckooHashTable *table) {
	assert(table);
	
	// Free the slots and inuse arrays
	free(table->table1->slots);
	free(table->table1->inuse);
		
	free(table->table2->slots);
	free(table->table2->inuse);
	
	// Free the array of inner tables
	free(table->table1);
	free(table->table2);
	
	// Free the table struct itself
	free(table);
}

// insert 'key' into 'table', if it's not in there already
// returns true if insertion succeeds, false if it was already in there
bool cuckoo_hash_table_insert(CuckooHashTable *table, int64 key) {
	assert(table);
	int start_time = clock(); // Start timing
	
	int H1 = h1(key) % table->size, H2 = h2(key) % table->size, H = H1;
	int insert_table = 1, time_kicked_keys = 0;
	int64 kick_key;
	InnerTable *temp_table = table->table1;
	
	// Double the size of the table if it has been full
	if (table->load == 2*table->size - 1) {
		double_cuckoo_table(table);
		table->stats.time = clock() - start_time; // Add time elapsed
		return cuckoo_hash_table_insert(table, key);
	}
	
	// Check whether the key has been inserted or not in Table 1
	if (table->table1->slots[H1] == key && table->table1->inuse[H1] == USED) {
		table->stats.time = clock() - start_time; // Add time elapsed
		return false;
	}
	
	// Check whether the key has been inserted or not in Table 2
	else if (table->table2->slots[H2] == key && 
		table->table2->inuse[H2] == USED) {
		table->stats.time = clock() - start_time; // Add time elapsed
		return false;
	}
		
	// The table has not been inserted before, try to insert the key
	else {
			
		// Try to find an empty slot
		while (temp_table->inuse[H] == USED  && 
			time_kicked_keys != 2*(table->size)) {		
			kick_key = temp_table->slots[H];
			time_kicked_keys++;
				
			temp_table->slots[H] = key;
			key = kick_key;
			
			// If it kicked the key from the Table 1, need to insert 
			// the kicked key to the Table 2 
			if (insert_table == 1) {
				
				// Mark the next table that will be visited is table 2
				insert_table = 2;
				
				// Update the temp_table and the hash
				temp_table = table->table2;
				H = h2(key) % table->size;
				
			}
				
			// If it kicked the key from the Table 2, need to insert 
			// the kicked key to the Table 1
			else if (insert_table == 2) {
				
				// Mark the next table that will be visited is table 1
				insert_table = 1;
				
				// Update the temp_table and the hash
				temp_table = table->table1;
				H = h1(key) % table->size;
			}
		}	
		
		// The number of kicked key is equal to the 2 times the table size,
		// indicates that there is a cycling, so we need to grow the table
		if (time_kicked_keys == 2*(table->size)) {
			double_cuckoo_table(table);
			table->stats.time = clock() - start_time; // Add time elapsed
			return cuckoo_hash_table_insert(table, key);
		}
		
		// Otherwise, just insert the key to the empty slot
		else {

			// We have found the empty slot, try to insert it
			temp_table->slots[H] = key;
			temp_table->inuse[H] = USED;
			table->load++;
			
			// Update the loaded keys 
			if (insert_table == 1) {
				table->stats.load_table1++;
			}
			else if (insert_table == 2) {
				table->stats.load_table2++;
			}
			
			table->stats.time = clock() - start_time; // Add time elapsed
			return true;
		}
	}
	
}

// lookup whether 'key' is inside 'table'
// returns true if found, false if not
bool cuckoo_hash_table_lookup(CuckooHashTable *table, int64 key) {
	assert(table);
	int start_time = clock(); // Start timing
	
	int H1 = h1(key) % table->size, H2 = h2(key) % table->size;
	
	// Check whether the key is available on Table 1
	if (table->table1->slots[H1] == key && table->table1->inuse[H1] == USED) {
		table->stats.time = clock() - start_time; // Add time elapsed
		return FOUND;
	}
	
	// Check whether the key is avaialbe on Table 2
	if (table->table2->slots[H2] == key && table->table2->inuse[H2] == USED) {\
		table->stats.time = clock() - start_time; // Add time elapsed
		return FOUND;
	}
	
	table->stats.time = clock() - start_time; // Add time elapsed
	return NOT_FOUND;
}

// print the contents of 'table' to stdout
void cuckoo_hash_table_print(CuckooHashTable *table) {
	assert(table);
	printf("--- table size: %d\n", table->size);

	// print header
	printf("                    table one         table two\n");
	printf("                  key | address     address | key\n");
	
	// print rows of each table
	int i;
	for (i = 0; i < table->size; i++) {

		// table 1 key
		if (table->table1->inuse[i]) {
			printf(" %20llu ", table->table1->slots[i]);
		} else {
			printf(" %20s ", "-");
		}

		// addresses
		printf("| %-9d %9d |", i, i);

		// table 2 key
		if (table->table2->inuse[i]) {
			printf(" %llu\n", table->table2->slots[i]);
		} else {
			printf(" %s\n",  "-");
		}
	}

	// done!
	printf("--- end table ---\n");
}


// print some statistics about 'table' to stdout
void cuckoo_hash_table_stats(CuckooHashTable *table) {
	assert(table);
	
	// Print some stats about state of the table in general
	printf("--- table stats ---\n");
	printf("                         current table size: %d slots\n", 
		table->size);
	printf("total number of keys in table 1 and table 2: %d\n", table->load);
	printf("                                load factor: %.3f%%\n", 
		table->load * 100.0 / (2*table->size));
	
	// Calculate CPU usage in seconds and print this
	float seconds = table->stats.time * 1.0 / CLOCKS_PER_SEC;
	printf(" 			     CPU time spent: %.6f sec\n\n", seconds);
	
	// Print some stats about state of the table 1
	printf("--- table 1 stats ---\n");
	printf("           current table 1 size: %d slots\n", table->size);
	printf("total number of keys in table 1: %d\n", table->stats.load_table1);
	printf("                    load factor: %.3f%%\n\n", 
		table->stats.load_table1 * 100.0 / table->size);
	
	// Print some stats about state of the table 2
	printf("--- table 2 stats ---\n");
	printf("           current table 2 size: %d slots\n", table->size);
	printf("total number of keys in table 2: %d\n", table->stats.load_table2);
	printf("                    load factor: %.3f%%\n\n", 
		table->stats.load_table2 * 100.0 / table->size);
	
	printf("--- end stats ---\n");
}

/************************** INITIALISE CUCKOO TABLE **************************/
// Helper functions to set up the internals of a cuckoo hash table struct with
// new arrays of size 'size'
static void initialise_cuckoo_table(CuckooHashTable *table, int size) {
	int i;
	assert(size < MAX_TABLE_SIZE && "error: table has grown too large!");
	
	// Initialise the new memory allocation for Table 1 and Table 2
	table->table1 = initialise_inner_table(table->table1, size);
	table->table2 = initialise_inner_table(table->table2, size);
		
	// Update the new size of the hash table
	table->size = size;
	table->load = 0;
	table->stats.load_table1 = 0;
	table->stats.load_table2 = 0;
	
	// Initialise the boolean for inuse array with NOT_USED 
	for (i = 0; i < table->size; i++) {
		table->table1->inuse[i] = NOT_USED;
		table->table2->inuse[i] = NOT_USED;
	}	
}

/************************** INITIALISE INNER TABLE ***************************/
// Helper function to initialise the InnerTable
static InnerTable *initialise_inner_table(InnerTable *innertable, int size) {
	innertable = malloc((size)*sizeof (InnerTable));
	assert(innertable);
	
	innertable->slots = malloc((size)*sizeof (int64));
	innertable->inuse = malloc((size)*sizeof (bool));
	assert(innertable->slots);
	assert(innertable->inuse);
	
	return innertable;
	
}
/**************************** DOUBLE CUCKOO TABLE  ***************************/ 
// Helper function to double the size of the cuckoo hash table
static void double_cuckoo_table(CuckooHashTable *table) {
	int64 *oldslots1 = table->table1->slots;
	int64 *oldslots2 = table->table2->slots;
	bool *oldinuse1 = table->table1->inuse;
	bool *oldinuse2 = table->table2->inuse;
	int oldsize = table->size, i;
	
	initialise_cuckoo_table(table, table->size * 2);
	
	// Insert the data to the new hash table
	for (i = 0; i < oldsize; i++) {
		
		if (oldinuse1[i] == USED) {
			cuckoo_hash_table_insert(table, oldslots1[i]);
		}
		
		if (oldinuse2[i] == USED) {
			cuckoo_hash_table_insert(table, oldslots2[i]);
		}
	}
	
	// Free the pointers after being used
	free(oldslots1);
	free(oldslots2);
	free(oldinuse1);
	free(oldinuse2);
}

