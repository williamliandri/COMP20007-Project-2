/* * * * * * * * *
 * Dynamic hash table using linear probing to resolve collisions
 *
 * created for COMP20007 Design of Algorithms - Assignment 2, 2017
 * by Matt Farrugia <matt.farrugia@unimelb.edu.au>
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "linear.h"

// how many cells to advance at a time while looking for a free slot
#define STEP_SIZE 1

// helper structure to store statistics gathered
typedef struct stats {
	int collisions; // calculate how many collisions that happen
	double total_probe; // calculate the total number of slots which must be
					// checked before all keys are inserted to a free space
	int is_recorded_collisions; // a flag whether the number of collisions have
						   // recorded or not
} Stats;

// a hash table is an array of slots holding keys, along with a parallel array
// of boolean markers recording which slots are in use (true) or free (false)
// important because not-in-use slots might hold garbage data, as they may
// not have been initialised
struct linear_table {
	int64 *slots;	// array of slots holding keys
	bool  *inuse;	// is this slot in use or not?
	int size;		// the size of both of these arrays right now
	int load;		// number of keys in the table right now
	Stats stats;	// collection of statistics about this hash table
};


/* * * *
 * helper functions
 */

// set up the internals of a linear hash table struct with new
// arrays of size 'size'
static void initialise_table(LinearHashTable *table, int size) {
	assert(size < MAX_TABLE_SIZE && "error: table has grown too large!");

	table->slots = malloc((sizeof *table->slots) * size);
	assert(table->slots);
	table->inuse = malloc((sizeof *table->inuse) * size);
	assert(table->inuse);
	int i;
	for (i = 0; i < size; i++) {
		table->inuse[i] = false;
	}

	table->size = size;
	table->load = 0;
}


// reinsert a ke into the hash table after doubling the table --- we can assume
// that there will definitely be space for this key because it was already
// inside the hash table previously
static void reinsert_key(LinearHashTable *table, int64 key) {
	int h = h1(key) % table->size, steps = 0;
	
	while (table->inuse[h]) {
		h = (h + STEP_SIZE) % table->size;
		steps++;
	}
	
	table->slots[h] = key;
	table->inuse[h] = true;
	table->load++;
	table->stats.total_probe += steps+1;
	
}


// double the size of the internal table arrays and re-hash all
// keys in the old tables
static void double_table(LinearHashTable *table) {
	int64 *oldslots = table->slots;
	bool  *oldinuse = table->inuse;
	int oldsize = table->size;
	
	initialise_table(table, table->size * 2);

	int i;
	for (i = 0; i < oldsize; i++) {
		if (oldinuse[i] == true) {
			
			reinsert_key(table, oldslots[i]);
			//linear_hash_table_insert(table, oldslots[i]);
		}
	}

	free(oldslots);
	free(oldinuse);
}


/* * * *
 * all functions
 */

// initialise a linear probing hash table with initial size 'size'
LinearHashTable *new_linear_hash_table(int size) {
	LinearHashTable *table = malloc(sizeof *table);
	assert(table);
	
	// set up the internals of the table struct with arrays of size 'size'
	initialise_table(table, size);
	
	table->stats.collisions = 0;
	table->stats.total_probe = 0;
	table->stats.is_recorded_collisions = 0;
	
	return table;
}


// free all memory associated with 'table'
void free_linear_hash_table(LinearHashTable *table) {
	assert(table != NULL);

	// free the table's arrays
	free(table->slots);
	free(table->inuse);

	// free the table struct itself
	free(table);
}


// insert 'key' into 'table', if it's not in there already
// returns true if insertion succeeds, false if it was already in there
bool linear_hash_table_insert(LinearHashTable *table, int64 key) {
	assert(table != NULL);

	// need to count our steps to make sure we recognise when the table is full
	int steps = 0;

	// calculate the initial address for this key
	int h = h1(key) % table->size;
	
	// step along the array until we find a free space (inuse[]==false),
	// or until we visit every cell
	while (table->inuse[h] && steps < table->size) {
		if (table->slots[h] == key) {
			// this key already exists in the table! no need to insert
			return false;
		}
		
		// else, keep stepping through the table looking for a free slot
		h = (h + STEP_SIZE) % table->size;
		
		// the step is 0 and it enters here means there is a collision
		// for the first address !
		if (steps == 0 && table->stats.is_recorded_collisions == 0) {
			table->stats.collisions++;
		}
		
		steps++;	
	}

	// if we used up all of our steps, then we're back where we started and the
	// table is full
	if (steps == table->size) {
		
		// When doubling the table, it indicates that the number of collisions
		// has been recorded
		table->stats.is_recorded_collisions = 1;
		
		// let's make some more space and then try to insert this key again!
		double_table(table);
		
		return linear_hash_table_insert(table, key);

	} else {
		// otherwise, we have found a free slot! insert this key right here
		table->slots[h] = key;
		table->inuse[h] = true;
		table->load++;
		
		// Sum up the total probe and reset the is_recorded_collisions flag		
		table->stats.total_probe += steps+1;
		table->stats.is_recorded_collisions = 0;
		
		return true;
	}
}


// lookup whether 'key' is inside 'table'
// returns true if found, false if not
bool linear_hash_table_lookup(LinearHashTable *table, int64 key) {
	assert(table != NULL);

	// need to count our steps to make sure we recognise when the table is full
	int steps = 0;

	// calculate the initial address for this key
	int h = h1(key) % table->size;

	// step along until we find a free space (inuse[]==false), or until we
	// visit every cell
	while (table->inuse[h] && steps < table->size) {

		if (table->slots[h] == key) {
			// found the key!
			return true;
		}

		// keep stepping
		h = (h + STEP_SIZE) % table->size;
		steps++;
	}

	// we have either searched the whole table or come back to where we started
	// either way, the key is not in the hash table
	return false;
}


// print the contents of 'table' to stdout
void linear_hash_table_print(LinearHashTable *table) {
	assert(table != NULL);

	printf("--- table size: %d\n", table->size);

	// print header
	printf("   address | key\n");

	// print the rows of the hash table
	int i;
	for (i = 0; i < table->size; i++) {
		
		// print the address
		printf(" %9d | ", i);

		// print the contents of the slot
		if (table->inuse[i]) {
			printf("%llu\n", table->slots[i]);
		} else {
			printf("-\n");
		}
	}

	printf("--- end table ---\n");
}


// print some statistics about 'table' to stdout
void linear_hash_table_stats(LinearHashTable *table) {
	assert(table != NULL);
	double load_factor = table->load * 100.0 / table->size;
	printf("--- table stats ---\n");
	
	// print some information about the table
	printf(" current size: %d slots\n", table->size);
	printf(" current load: %d items\n", table->load);
	printf("  load factor: %.3f%%\n", load_factor);
	printf("    step size: %d slots\n", STEP_SIZE);
	
	printf("   collisions: %d\n", table->stats.collisions);
	printf("average probe: %.1lf\n", table->stats.total_probe/table->load*1.0);
	
	printf("--- end stats ---\n");
}
