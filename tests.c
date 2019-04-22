#include <stdio.h>
#include <stdlib.h>

/**

Idea is to write initial root directory to disk file. Perhaps we don't need to do this (maybe FUSE does it...? OR
maybe its already there and all zeros is just the reeasonable start state for the root struct) but for testing it 
will be handy anyway, for now

UPDATE

root directory struct is all zeros for empty root. So the disk is initialized with the root struct already in there.

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>

//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//The attribute packed means to not align these things
struct csc452_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct csc452_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct csc452_file_directory) - sizeof(int)];
} ;

typedef struct csc452_root_directory csc452_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct csc452_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct csc452_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct csc452_directory) - sizeof(int)];
} ;

typedef struct csc452_directory_entry csc452_directory_entry;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE)

struct csc452_disk_block
{
	//All of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct csc452_disk_block csc452_disk_block;

/*
 * returns the first index of directories in root that is free 
 * returns -1 on error (all directories in root are in use)
 *
*/
int getFirstFreeDirectoryIndex(struct csc452_root_directory root) {
	int i;
	for(i = 0; i < MAX_DIRS_IN_ROOT; i++) {
		if(strcmp(root.directories[i].dname, "")) { // intended function returns i if this directory is not taken
			return i;
		}
	}
	return -1;
}


/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {
		
		// read in root from disl
		FILE * fp;
	   	fp = fopen (".disk", "ab+");
	   	struct csc452_root_directory root;
		fseek(fp, 0, SEEK_SET);
		fread(&root, sizeof(csc452_root_directory), 1, fp);   	
		
		char directory[MAX_FILENAME + 1], filename[MAX_FILENAME + 1], extension[MAX_EXTENSION + 1];
		strcpy(directory,"");
		strcpy(filename,"");
		strcpy(extension,"");
		printf("got here 2");

		sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
		
		// search filesystem for object specified by path
		struct csc452_directory_entry dir;
		int i,j;
		for (i = 0; i <root.nDirectories; i++){
			//printf("got here 4\n");
			if (strcmp(root.directories[i].dname, directory) == 0 ){
				
				if (strcmp(filename, "") == 0 && strcmp(extension, "") == 0) {
					printf("got here 5\n");
					//its a directory
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					fclose(fp);
					return res;
				}
				else {
					fseek(fp, root.directories[i].nStartBlock, SEEK_SET);	
					fread(&dir, sizeof(csc452_directory_entry), 1, fp); 
					for (j = 0; j < dir.nFiles; j++) {
						if (strcmp(dir.files[j].fname, filename) == 0) {
							if (strcmp(dir.files[j].fext, extension) == 0) {
								stbuf->st_mode = S_IFREG | 0666;
								stbuf->st_nlink = 2;
								stbuf->st_size = dir.files[j].fsize;
								fclose(fp);
								return res;
							}
						}
					}
				}
			}
		}
	  	fclose(fp);
		//Else return that path doesn't exist
		res = -ENOENT;
	}
	return res;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
	int i;
	(void) path;
	(void) mode;
	char directory[MAX_FILENAME + 1], filename[MAX_FILENAME + 1], extension[MAX_EXTENSION + 1];
	strcpy(directory,"");
	strcpy(filename,"");
	strcpy(extension,"");
		


	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	struct stat stbuf;
	if (csc452_getattr(path, &stbuf) == 0) {
		return EEXIST;
	}
	if (strlen(directory) > 8) {
		return ENAMETOOLONG;
	}

	//if not only under root
	if (strchr(filename, '/')){
		return EPERM;
	}

	//make a new directory
	struct csc452_directory newDirectory;
	strcpy(newDirectory.dname,directory);


	FILE * fp;
	fp = fopen (".disk", "ab+");
	
	struct csc452_root_directory root;
	// = (struct csc452_root_directory);
	fseek(fp, 0, SEEK_SET);
	fread(&root, sizeof(csc452_root_directory), 1, fp);  

	if(root.nDirectories +1 > MAX_DIRS_IN_ROOT) {
		return ENOSPC;
	}

	//10218 = 5*2^11 - 22 
	//this is the block number the bit map starts on
	fseek(fp,10218*BLOCK_SIZE, SEEK_SET);
	char bitmap[BLOCK_SIZE*20];

	int found = 0;

	fread(bitmap, sizeof(bitmap), 1, fp); 
	for(i = 1; i < 10218; i++) {
		if(bitmap[i] == '\0') {
			//theres space for a dir at i
			root.nDirectories++;
			newDirectory.nStartBlock = i;
			bitmap[i] = (char) 1;
			root.directories[getFirstFreeDirectoryIndex(root)] = newDirectory;
			fclose(fp);
			found = 1;
			break;
		}
	}

	if(found == 0) {
		return EDQUOT;
	}	

	fp = fopen (".disk", "ab+");
	fseek(fp, 0, SEEK_SET);
	fwrite(&root, sizeof(csc452_root_directory), 1, fp);

	fseek(fp,i*BLOCK_SIZE, SEEK_SET);
	fwrite(&newDirectory, sizeof(struct csc452_directory), 1, fp);

	return 0;
}


void test_getattr() {

}

void test_mkdir(){

	csc452_mkdir("/folder1", 1);

}






int main() {

	test_getattr();
	test_mkdir();

	// FILE * fp;
 //   	fp = fopen (".disk", "ab+");

 //   	struct csc452_root_directory root;
 //   	root.nDirectories = 0;

	// fseek(fp, 0, SEEK_SET);
	// fwrite(&root, sizeof(csc452_root_directory), 1, fp);   	

 //  	fclose(fp);

}