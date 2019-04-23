/*
	FUSE: Filesystem in Userspace


	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452


*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>

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
	printf("calling index func\n");
	int i;
	for(i = 0; i < MAX_DIRS_IN_ROOT; i++) {
		if(strlen(root.directories[i].dname) <= 0) { // intended function returns i if this directory is not taken
			return i;
		}
	}
	return -1;
}


/*
 * returns 1 if directory in root directory, 0 otherwise
 * returns -1 on error (all directories in root are in use)
 *
*/
int directory_exists(char* dirname, FILE* fp){

	struct csc452_root_directory root;
	fseek(fp, 0, SEEK_SET);
	fread(&root, sizeof(struct csc452_root_directory), 1, fp); 
	int i;
	for (i = 0; i <root.nDirectories; i++){
		if (strcmp(root.directories[i].dname, dirname) == 0 ){
			return 1;
		}
	}
	return 0;

}


/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf)
{
	printf("calling getatt\n");
	int res = 0;
	
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {
		
		// read in root from disl
		FILE * fp1;
	   	fp1 = fopen(".disk", "rb");
	   
	   	struct csc452_root_directory root;
	   
		fseek(fp1, 0, SEEK_SET);
		
		fread(&root, sizeof(struct csc452_root_directory), 1, fp1);   	
		
		char directory[MAX_FILENAME + 1], filename[MAX_FILENAME + 1], extension[MAX_EXTENSION + 1];
		strcpy(directory,"");
		strcpy(filename,"");
		strcpy(extension,"");

		sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
		//printf("MAX_DIRS_IN_ROOT = %d\n", MAX_DIRS_IN_ROOT);
		printf("nDirectories = %d\n", root.nDirectories);
		
		// search filesystem for object specified by path
		struct csc452_directory_entry dir;
		int i,j;
		for (i = 0; i <root.nDirectories; i++){
			//printf("get atter here 3.5\n");
			//printf("got here - %d\n", i);
			if (root.directories[i].dname != NULL && strcmp(root.directories[i].dname, directory) == 0 ){
				printf("get atter here 4\n");
				if (strcmp(filename, "") == 0 && strcmp(extension, "") == 0) {
					//its a directory
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					fclose(fp1);
					return res;
				}
				else {
					fseek(fp1, root.directories[i].nStartBlock, SEEK_SET);	
					fread(&dir, sizeof(struct csc452_directory_entry), 1, fp1); 
					for (j = 0; j < dir.nFiles; j++) {
						if (strcmp(dir.files[j].fname, filename) == 0) {
							if (strcmp(dir.files[j].fext, extension) == 0) {
								stbuf->st_mode = S_IFREG | 0666;
								stbuf->st_nlink = 2;
								stbuf->st_size = dir.files[j].fsize;
								fclose(fp1);
								return res;
							}
						}
					}
				}
			}
		}
		printf("get atter here Z\n");
	  	fclose(fp1);
		//Else return that path doesn't exist
		printf("%s", "get here, result is: ");
		res = -ENOENT;
	}
	printf("%d\n", res);
	return res;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int csc452_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{

	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") != 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
	}
	else {
		// All we have _right now_ is root (/), so any other path must
		// not exist. 
		return -ENOENT;
	}

	return 0;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{

	printf("calling mkdir\n");

	int i;
	(void) path;
	(void) mode;
	char directory[MAX_FILENAME + 1], filename[MAX_FILENAME + 1], extension[MAX_EXTENSION + 1];
	strcpy(directory,"");
	strcpy(filename,"");
	strcpy(extension,"");

	FILE * fp;
	FILE * fp2;
	fp = fopen (".disk", "rb+");
	fp2 = fp;
	if (! fp) {
		printf("%s\n", "Could not open disk");
		return -ENOSPC;
	}
	
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if (directory_exists(directory, fp) == 1) {
		printf("directory already exists\n");
		return -EEXIST;
	}
	
	if (strlen(directory) > 8) {
		return -ENAMETOOLONG;
	}

	//if not only under root
	if (strchr(filename, '/')){
		return -EPERM;
	}
	
	//make a new directory
	struct csc452_directory newDirectory;
	directory[strlen(directory)] = '\0';
	printf("d = %s\n",directory);
	for(int i = 0; i < strlen(directory + 1); i++){
		newDirectory.dname[i] = directory[i];
	}
	strcpy(newDirectory.dname,directory);
	
	
	struct csc452_root_directory root;
	// = (struct csc452_root_directory);
	fread(&root, sizeof(struct csc452_root_directory), 1, fp);  

	if(root.nDirectories +1 > MAX_DIRS_IN_ROOT) {
		printf("nDirectories = %d\n",root.nDirectories);
		return -ENOSPC;
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
			printf("directy name before setting = %s\n",newDirectory.dname);
			bitmap[i] = (char) 1;
			int index = getFirstFreeDirectoryIndex(root);
			printf("directory index = %d\n",index);
			root.directories[index] = newDirectory;
			printf("directy name after setting root = %s\n",newDirectory.dname);
			found = 1;
			break;
		}
	}
	fclose(fp);
	fp2 = fopen (".disk", "rb+");
	if(found == 0) {
		printf("returning error no room on disk");
		return -EDQUOT;
	}	


	if(root.nDirectories > 1000000 || root.nDirectories < 0) {
				root.nDirectories = 1;
			}

	printf("nDirectories = %d\n",root.nDirectories);
	printf("directories[0] = %s\n",root.directories[0].dname);
	fwrite(&root, sizeof(struct csc452_root_directory), 1, fp2);

	fseek(fp2,i*BLOCK_SIZE, SEEK_SET);
	fwrite(&newDirectory, sizeof(struct csc452_directory), 1, fp2);
	
	fclose(fp2);

	printf("mkdir finished returned 0\n");
	return 0;
}

/*
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 * Note that the mknod shell command is not the one to test this.
 * mknod at the shell is used to create "special" files and we are
 * only supporting regular files.
 *
 */
static int csc452_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) path;
	(void) mode;
    (void) dev;
	
	return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int csc452_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//read in data
	//return success, or error

	return size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int csc452_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//write data
	//return success, or error

	return size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	  (void) path;

	  return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
        (void) path;
        return 0;
}


/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int csc452_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}

/*
 * Called when we open a file
 *
 */
static int csc452_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int csc452_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations csc452_oper = {
    .getattr	= csc452_getattr,
    .readdir	= csc452_readdir,
    .mkdir		= csc452_mkdir,
    .read		= csc452_read,
    .write		= csc452_write,
    .mknod		= csc452_mknod,
    .truncate	= csc452_truncate,
    .flush		= csc452_flush,
    .open		= csc452_open,
    .unlink		= csc452_unlink,
    .rmdir		= csc452_rmdir
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &csc452_oper, NULL);
}
