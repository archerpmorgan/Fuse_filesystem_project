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
int getFirstFreeDirectoryIndex(struct csc452_root_directory *root) {
	int i;
	for(i = 0; i < MAX_DIRS_IN_ROOT; i++) {
		if(strlen(root->directories[i].dname) <= 0) { // intended function returns i if this directory is not taken
			return i;
		}
	}
	return -1;
}

/*
 * returns the first index of files in root that is free 
 * returns -1 on error (all directories in root are in use)
 *
*/
int getFirstFreeFileIndex(struct csc452_directory_entry *dir) {
	int i;
	for(i = 0; i < MAX_FILES_IN_DIR; i++) {
		if(strlen(dir->files[i].fname) <= 0) { // intended function returns i if this directory is not taken
			return i;
		}
	}
	return -1;
}



/*
 * returns index into root if directory in root directory, -1 otherwise
 *
*/
int directory_exists(char* dirname, FILE* fp){

	struct csc452_root_directory root;
	fseek(fp, 0, SEEK_SET);
	fread(&root, sizeof(struct csc452_root_directory), 1, fp); 
	int i;
	for (i = 0; i <MAX_DIRS_IN_ROOT; i++){
		if (strcmp(root.directories[i].dname, dirname) == 0 ){
			return i;
		}
	}
	return -1;
}

/*
 * returns 1 if file in some directory, 0 otherwise
 *
*/
int file_exists(char* dirname, char* fname, FILE* fp){
	int dirloc = directory_exists(dirname, fp);
	if (dirloc < 0) {
		return -1;
	}
	struct csc452_root_directory root = {0};
	struct csc452_directory_entry dir = {0};
	fseek(fp, 0, SEEK_SET);
	fread(&root, sizeof(struct csc452_root_directory), 1, fp);
	fseek(fp, root.directories[dirloc].nStartBlock * BLOCK_SIZE, SEEK_SET);
	fread(&dir, sizeof(struct csc452_directory_entry), 1, fp);
	int j;
	for (j = 0; j <MAX_FILES_IN_DIR; j++){
		if (strcmp(dir.files[j].fname, fname) == 0 ){
			return j;
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
	   	fp = fopen(".disk", "rb");
	   
	   	struct csc452_root_directory root = {0};
	   
		fseek(fp, 0, SEEK_SET);
		
		fread(&root, sizeof(struct csc452_root_directory), 1, fp);   	
		
		char directory[MAX_FILENAME + 1], filename[MAX_FILENAME + 1], extension[MAX_EXTENSION + 1];
		strcpy(directory,"");
		strcpy(filename,"");
		strcpy(extension,"");

		sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
		
		// search filesystem for object specified by path
		struct csc452_directory_entry dir = {0};

		int dex = directory_exists(directory, fp);
		fseek(fp, 0, SEEK_SET);
		int fex = file_exists(directory, filename, fp);
		fseek(fp, 0, SEEK_SET);
		fclose(fp);

		printf("dex and fex are %d and %d\n", dex, fex);

		if (dex < 0 ) { // directory does not exist
			res = -ENOENT;
		}
		else if (dex >=0 && (strcmp(filename, "") == 0)) { //directory 
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
		}
		else if (dex >= 0 && fex >= 0) { // existing file
			fseek(fp, root.directories[dex].nStartBlock, SEEK_SET);	
			fread(&dir, sizeof(struct csc452_directory_entry), 1, fp);
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_nlink = 2;
			stbuf->st_size = dir.files[fex].fsize;
		}
		else { // file does not exist
			res = - ENOENT;
		}
	}
	printf("getattr returned\n");
	return res;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int csc452_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{

	printf("path is %s\n", path);
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;
	char directory[MAX_FILENAME + 1], filename[MAX_FILENAME + 1], extension[MAX_EXTENSION + 1];
	strcpy(directory,"\0\0\0\0\0\0\0\0\0");
	strcpy(filename,"\0\0\0\0\0\0\0\0\0");
	strcpy(extension,"\0\0\0\0");
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	// open disk
	FILE * fp;
	fp = fopen (".disk", "rb+");
	if (! fp) {
		printf("%s\n", "Could not open disk");
		fclose(fp);
		return -ENOSPC;
	}
	//read in root
	struct csc452_root_directory *root = (struct csc452_root_directory*) calloc(1, sizeof(struct csc452_root_directory));
	fread(root, sizeof(struct csc452_root_directory), 1, fp);

	//check existence, plus get index if exists
	int dir_index = directory_exists(directory, fp);

	// allocate dir
	struct csc452_directory_entry *dir = (struct csc452_directory_entry*) calloc(1, sizeof(struct csc452_directory_entry));


	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") != 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);

		// acquire directory
		fseek(fp, root->directories[dir_index].nStartBlock * BLOCK_SIZE, SEEK_SET);
		fread(dir, sizeof(struct csc452_directory_entry), 1, fp);

		//list all files
		for (int i = 0; i < MAX_FILES_IN_DIR; i++){
			if (strcmp(dir->files[i].fname, "") != 0){ // if we have a directory with a name
				filler(buf, dir->files[i].fname, NULL, 0);
			}
		}
	}
	else { // ls in the root directory (all we implement for sprint 1)
		int i;
		for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
			if (strcmp(root->directories[i].dname, "") != 0){ // if we have a directory with a name
				filler(buf, root->directories[i].dname, NULL, 0);
			}
		}
	}
	fclose(fp);
	return 0;
}
/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{

	int i, index;
	(void) path;
	(void) mode;
	char directory[MAX_FILENAME + 1], filename[MAX_FILENAME + 1], extension[MAX_EXTENSION + 1];
	strcpy(directory,"\0\0\0\0\0\0\0\0\0");
	strcpy(filename,"\0\0\0\0\0\0\0\0\0");
	strcpy(extension,"\0\0\0\0");

	FILE * fp;
	fp = fopen (".disk", "rb+");
	if (! fp) {
		printf("%s\n", "Could not open disk");
		return -ENOSPC;
	}
	//save start position of file to return to later
	unsigned long root_position;
	fflush(fp);
	root_position = ftell(fp);
	
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	if (directory_exists(directory, fp) >= 0) {
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
	struct csc452_directory newDirectory = {0};
	strcpy(newDirectory.dname,directory);
	
	//read in root
	fseek(fp, root_position, SEEK_SET);
	struct csc452_root_directory *root = (struct csc452_root_directory*) calloc(1, sizeof(struct csc452_root_directory));
	fread(root, sizeof(struct csc452_root_directory), 1, fp);  
	if(root->nDirectories +1 > MAX_DIRS_IN_ROOT) {
		printf("nDirectories = %d\n",root->nDirectories);
		return -ENOSPC;
	}

	//10218 = 5*2^11 - 22 
	//this is the block number the bit map starts on
	fseek(fp,10218*BLOCK_SIZE, SEEK_SET);
	unsigned long bitmap_position;
	fflush(fp);
	bitmap_position = ftell(fp);
	char bitmap[BLOCK_SIZE*20];

	int found = 0;
	fread(bitmap, sizeof(bitmap), 1, fp); 
	for(i = 1; i < 10218; i++) {
		if(bitmap[i] == '\0') {
			//theres space for a dir at i
			root->nDirectories++;
			newDirectory.nStartBlock = i;
			bitmap[i] = (char) 1;
			index = getFirstFreeDirectoryIndex(root);
			root->directories[index] = newDirectory;
			found = 1;
			break;
		}
	}
	if(found == 0) {
		return -EDQUOT;
	}	
	//write bitmap back
	fseek(fp, bitmap_position, SEEK_SET);
	fwrite(&bitmap, BLOCK_SIZE*20, 1, fp);

	printf("GOOBAGOOBA %d %d", index, root->directories[index].nStartBlock);
	//write root back
	fseek(fp, root_position, SEEK_SET);
	fwrite(root, sizeof(struct csc452_root_directory), 1, fp);

	//write new directory
	struct csc452_directory_entry emptydir = {0}; 
	fseek(fp,i*BLOCK_SIZE, SEEK_SET);
	fwrite(&emptydir, sizeof(struct csc452_directory), 1, fp);
	
	fclose(fp);
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
	int i;
	(void) path;
	(void) mode;
    (void) dev;
    char directory[MAX_FILENAME + 1], filename[MAX_FILENAME + 1], extension[MAX_EXTENSION + 1];
	strcpy(directory,"\0\0\0\0\0\0\0\0\0");
	strcpy(filename,"\0\0\0\0\0\0\0\0\0");
	strcpy(extension,"\0\0\0\0");

	FILE * fp;
	fp = fopen (".disk", "rb+");
	if (! fp) {
		printf("%s\n", "Could not open disk");
		return -ENOSPC;
	}
	//save start position of file to return to later
	unsigned long root_position;
	fflush(fp);
	root_position = ftell(fp);
	
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	if (file_exists(directory, filename, fp) > 0) {
		printf("file already exists\n");
		fclose(fp);
		return -EEXIST;
	}
	int dir_index = directory_exists(directory, fp);
	if (dir_index < 0) {
		fclose(fp);
		return -ENOTDIR;
	}
	if (strlen(filename) < 1) {
		fclose(fp);
		return -EPERM;
	}
	if (strlen(directory) > 8) {
		fclose(fp);
		return -ENAMETOOLONG;
	}
	//if not only under root
	if (strchr(filename, '/')){
		fclose(fp);
		return -EPERM;
	}

	//read in root
	fseek(fp, root_position, SEEK_SET);
	struct csc452_root_directory *root = (struct csc452_root_directory*) calloc(1, sizeof(struct csc452_root_directory));
	fread(root, sizeof(struct csc452_root_directory), 1, fp); 

	// find block to store the file at
	fseek(fp,10218*BLOCK_SIZE, SEEK_SET);
	unsigned long bitmap_position;
	fflush(fp);
	bitmap_position = ftell(fp);
	char bitmap[BLOCK_SIZE*20];
	int found = 0;
	fread(bitmap, sizeof(bitmap), 1, fp); 
	for(i = 1; i < 10218; i++) {
		if(bitmap[i] == '\0') {
			//theres space for a dir at i
			bitmap[i] = (char) 1;
			found = 1;
			break;
		}
	}
	if(found == 0) {
		return -EDQUOT;
	}	

	//read directory entry
	struct csc452_directory_entry *dir = (struct csc452_directory_entry*) calloc(1, sizeof(struct csc452_directory_entry));
	int dirloc = root->directories[dir_index].nStartBlock;

	fseek(fp, BLOCK_SIZE*dirloc, SEEK_SET);
	fread(dir, sizeof(csc452_directory_entry), 1, fp);
	//check if no space in d
	if (dir->nFiles > MAX_FILES_IN_DIR) {
		fclose(fp);
		printf("err7\n");
		return ENOSPC;
	}

	// modify directory entry to contain new file pointer
	int index = getFirstFreeFileIndex(dir);
	strcpy(dir->files[index].fname, filename);
	strcpy(dir->files[index].fext, extension);

	dir->files[index].nStartBlock = i;
	dir->nFiles += 1;

	//write bitmap back
	fseek(fp, bitmap_position, SEEK_SET);
	fwrite(&bitmap, BLOCK_SIZE*20, 1, fp);

	//write root back
	fseek(fp, root_position, SEEK_SET);
	fwrite(root, sizeof(struct csc452_root_directory), 1, fp);

	//write updated directory
	fseek(fp, BLOCK_SIZE*dirloc, SEEK_SET);
	fwrite(dir, sizeof(struct csc452_directory_entry), 1, fp);

	fclose(fp);
	
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
	//int i;
	(void) path;
	//(void) mode;
	char directory[MAX_FILENAME + 1], filename[MAX_FILENAME + 1], extension[MAX_EXTENSION + 1];
	strcpy(directory,"\0\0\0\0\0\0\0\0\0");
	strcpy(filename,"\0\0\0\0\0\0\0\0\0");
	strcpy(extension,"\0\0\0\0");

	FILE * fp;
	fp = fopen (".disk", "rb+");
	if (! fp) {
		printf("%s\n", "Could not open disk");
		return -ENOSPC;
	}
	//save start position of file to return to later
	unsigned long root_position;
	fflush(fp);
	root_position = ftell(fp);
	
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if (directory_exists(directory, fp) < 0) {
		printf("directory does not exist\n");
		return -ENOENT;
	}
	if (strcmp(filename,"") != 0 && strcmp(extension,"") != 0) {
		printf("path is not a directory\n");
		return -ENOTDIR;
	}

	//read in root
	fseek(fp, root_position, SEEK_SET);
	struct csc452_root_directory *root = (struct csc452_root_directory*) calloc(1, sizeof(struct csc452_root_directory));
	fread(root, sizeof(struct csc452_root_directory), 1, fp);

	fseek(fp,10218*BLOCK_SIZE, SEEK_SET);
	unsigned long bitmap_position;
	fflush(fp);
	bitmap_position = ftell(fp);
	char bitmap[BLOCK_SIZE*20];
	fread(bitmap, sizeof(bitmap), 1, fp); 


	int i;
	int found = 0;
	//get the index of the directory to be removed
	for(i = 0; i < MAX_DIRS_IN_ROOT; i++) {
		if(strcmp(root->directories[i].dname,directory) == 0) {
			found = 1;
			break;
		}
	}  

	if(found == 0) {
		//throw error
		return -ENOENT;
	}
	//delete values (set to null or zero)

	//+ 1 for reasons only Archer knows
	bitmap[i + 1] = (char) 0;
	//initialize sets it to zero
	struct csc452_directory newDirectory = {0};
	struct csc452_directory_entry emptyEntry = {0};
	
	// clear out old directory block
	fseek(fp, BLOCK_SIZE * (root->directories[i].nStartBlock), SEEK_SET);
	fwrite(&emptyEntry, BLOCK_SIZE, 1, fp);

	root->directories[i] = newDirectory;
	root->nDirectories = root->nDirectories - 1;
	

	//write bitmap
	fseek(fp, bitmap_position, SEEK_SET);
	fwrite(&bitmap, BLOCK_SIZE*20, 1, fp);

		//write root back
	fseek(fp, root_position, SEEK_SET);
	fwrite(root, sizeof(struct csc452_root_directory), 1, fp);
	
	fclose(fp);

	return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
		int i;
		char directory[MAX_FILENAME + 1], filename[MAX_FILENAME + 1], extension[MAX_EXTENSION + 1];
		strcpy(directory,"\0\0\0\0\0\0\0\0\0");
		strcpy(filename,"\0\0\0\0\0\0\0\0\0");
		strcpy(extension,"\0\0\0\0");
		sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

		//error check------------------------
        (void) path;
		FILE * fp;
		 fp = fopen (".disk", "rb+");
		 if (! fp) {
		 	printf("%s\n", "Could not open disk");
		 	fclose(fp);
		 	return -ENOSPC;
		}

		//get index of directory and file in the dir if it exists, return otherwise
		int dex = directory_exists(directory, fp);
		fseek(fp, 0, SEEK_SET);
		int fex = file_exists(directory, filename, fp);
		fseek(fp, 0, SEEK_SET);

		printf("fex and dex are %d %d\n", fex, dex);

        if (fex < 0 || dex < 0) {
        	printf("file does not exist.\n");
        	fclose(fp);
        	return -ENOENT;
        }

        //make root and directory-----------------------------
        int dirloc;
		struct csc452_root_directory root = {0};
		struct csc452_directory_entry dir = {0};
		fseek(fp, 0, SEEK_SET);
		fread(&root, sizeof(struct csc452_root_directory), 1, fp);
		dirloc = root.directories[dex].nStartBlock;
		printf("start block of directory %d\n", dirloc);
		fseek(fp, BLOCK_SIZE * dirloc, SEEK_SET);
		fread(&dir, sizeof(struct csc452_directory_entry), 1, fp);

		//read in bitmap -----------------------------------------
		fseek(fp,10218*BLOCK_SIZE, SEEK_SET);
		unsigned long bitmap_position;
		fflush(fp);
		bitmap_position = ftell(fp);
		char bitmap[BLOCK_SIZE*20];
		fread(bitmap, sizeof(bitmap), 1, fp);  

		// clear bitmap
		struct csc452_file_directory file = dir.files[fex];
		int fileSize = file.fsize;
		int start = (int) file.nStartBlock;
		for(i = 0; i < fileSize; i++) {
			bitmap[start + i + 1] = (char) 0;
		}

		//delete all blocks of the file
		int file_start_loc = dir.files[fex].nStartBlock;
		struct csc452_file_directory emptyFile = {0};
		for(i = 0; i <fileSize; i++) {
			fseek(fp, BLOCK_SIZE * (file_start_loc + i), SEEK_SET);
			fwrite(&emptyFile, BLOCK_SIZE, 1, fp);
		}

		//update dir
		dir.nFiles--;
		struct csc452_file_directory empty = {0};
		dir.files[fex] = empty;

		printf("new number of files is %d\n", dir.nFiles);
		printf("new of deleted file, %s\n", dir.files[fex].fname);
		printf("writing file back to dirloc: %d\n", dirloc);

		//write directory back
		fseek(fp, BLOCK_SIZE * dirloc, SEEK_SET);
		fwrite(&dir, sizeof(struct csc452_directory_entry), 1, fp);

		//write root back
		fseek(fp, 0, SEEK_SET);
		fwrite(&root, sizeof(struct csc452_root_directory), 1, fp);

		//write bitmap back
		fseek(fp, bitmap_position, SEEK_SET);
		fwrite(&bitmap, BLOCK_SIZE*20, 1, fp);		

		fclose(fp);
		printf("%s\n", "unlink returns");
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
