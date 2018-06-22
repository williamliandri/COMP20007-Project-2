# COMP20007 - Design of Algorithms Project 2
## Dynamic Hash Tables
##
## Compile the Main Program:
### make
## Run the Main Program:
### ./a2 -t [table type] -s [starting size]
### List of table types:
### ~ linear: Linear probing hash table.
### ~ xtndbl1: Single-key extendible hash table.
### ~ cuckoo: Cuckoo hash table.
### ~ xtndbln: Multi-key extendible hash table.
### ~ xuckoo: Extendible cuckoo hash table.
### ~ xuckoon: Multi-key extendible cuckoo hash table.
## Command options:
## ~ i number: Insert number to the hash table.
## ~ l number: Look up whether number is in the hash table.
## ~ p: Print the current content of the hash table.
## ~ s: Print some statistics about the table state.
## ~ h: Print a list of available commands.
## ~ q: Quit the program.
## Clean the Program:
## make clean
##
## cmdgen.c is a program to generate some inserts and lookups commands to test the hash table.
## Compile the CMD Program:
### make cmdgen
## Run the CMD Program:
### ./cmdgen [no. of insert commands] [no. of lookup commands] > [name of the text file to save list of the commands]


