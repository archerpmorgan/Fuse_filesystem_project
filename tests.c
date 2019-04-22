#include <stdio.h>
#include <stdlib.h>
#include "csc452fuse.c"

/**

Idea is to write initial root directory to disk file. Perhaps we don't need to do this (maybe FUSE does it...? OR
maybe its already there and all zeros is just the reeasonable start state for the root struct) but for testing it 
will be handy anyway, for now

UPDATE

root directory struct is all zeros for empty root. So the disk is initialized with the root struct already in there.

*/




void test_getattr(){

	struct stat info;
	char path[10] = "/";
	int retval = csc452_getattr(path, &info);
	printf("%s%d\n", "getattr test 1 on path / with empty directory: ", retval);


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