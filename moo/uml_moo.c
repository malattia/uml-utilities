/* Copyrighted (C) 2001 RidgeRun,Inc (glonnon@ridgerun.com)
 * With modifications by Jeff Dike, James McMechan, and Steve Schmidtke.
 * Licensed under the GPL
 */ 

#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <byteswap.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include "cow.h"

static inline int ubd_test_bit(int bit, unsigned long *data)
{
	int bits, n, off;

	bits = sizeof(data[0]) * 8;
	n = bit / bits;
	off = bit % bits;
	return((data[n] & (1 << off)) != 0);
}

static inline void ubd_set_bit(int bit, unsigned long *data)
{
	int bits, n, off;

	bits = sizeof(data[0]) * 8;
	n = bit / bits;
	off = bit % bits;
	data[n] |= (1 << off);
}

int create_backing_file(char *in, char *out)
{
	int cow_fd, back_fd, in_fd, out_fd;
	struct stat buf;
	unsigned long *bitmap;
	unsigned long bitmap_len;
	int sectors;
	void *sector, *zeros;
	__u64 size, offset, i; /* i is u64 to prevent 32 bit overflow */
	int data_offset;
        __u32 magic;
        char *backing_file;
        time_t mtime;
        int sectorsize, bitmap_offset, perms, n;

	if((cow_fd = open(in,  O_RDONLY)) < 0){
		perror("COW file open");
		exit(1);
	}

	if(read_cow_header(cow_fd, &magic, &backing_file, &mtime, &size,
			   &sectorsize, &bitmap_offset)){
		fprintf(stderr, "Reading COW header failed\n");
		exit(1);
	}

	if(stat(backing_file, &buf) < 0) {
		perror("Stating backing file");
		exit(1);
	}
	if(buf.st_size != size){
		fprintf(stderr,"Size mismatch (%ld vs %ld) of COW header "
			"vs backing file\n", (long int) size, 
			(long int) buf.st_size);
		exit(1);
	}
	if(buf.st_mtime != mtime) {
		fprintf(stderr,"mtime mismatch (%ld vs %ld) of COW "
			"header vs backing file\n", mtime, buf.st_mtime);
		exit(1);
	}

	perms = (out != NULL) ? O_RDONLY : O_RDWR;

	if((back_fd = open(backing_file, perms)) < 0){
		perror("Opening backing file");
		exit(1);
	}

	if(out != NULL){
		if((out_fd = creat(out, 0644)) < 0){
			perror("Output file open");
			exit(1);
		}
	}
	else out_fd = back_fd;

	cow_sizes(size, sectorsize, bitmap_offset, &bitmap_len, &data_offset);
		
	bitmap = (unsigned long *) malloc(bitmap_len);
	if(bitmap == NULL) {
		perror("Can't allocate bitmap");
		exit(1);
	}

	if(pread(cow_fd, bitmap, bitmap_len, bitmap_offset) != bitmap_len){
		perror("Reading COW bitmap");
		exit(1);
	}

	sectors = size / sectorsize;
	sector = malloc(sectorsize);
	zeros = malloc(sectorsize);
	if((sector == NULL) || (zeros == NULL)){
		perror("Malloc of buffers");
		exit(1);
	}

	memset(zeros, 0, sectorsize);

	for(i = 0; i < sectors; i++){
		offset = i * sectorsize;
		if(ubd_test_bit(i, bitmap)){
			offset += data_offset;
			in_fd = cow_fd;
		}
		else in_fd = back_fd;

		/* If we're doing a destructive merge, and the backing file
		 * sector is up to date, then there's nothing to do.
		 */
		if((out_fd == back_fd) && (in_fd == back_fd))
			continue;

		if(pread(in_fd, sector, sectorsize, offset) != sectorsize){
			perror("Reading data");
			exit(1);
		}

		/* Sparse file creation - if the sector is all zeros and it's
		 * not the last sector (which always gets written out in order
		 * to make the file size right), maybe it doesn't need to be
		 * written out.
		 */

		if((i < sectors - 1) && !memcmp(sector, zeros, sectorsize)){
			/* If we're doing a non-destructive merge, then zero 
			 * sectors can just be skipped.
			 */
			if(out_fd != back_fd)
				continue;

			/* Otherwise, we are doing a destructive merge, and
			 * we need to check the backing file for a zero
			 * sector.
			 */
			n = pread(back_fd, sector, sectorsize, i * sectorsize);
			if(n != sectorsize){
				perror("Checking backing file for zeros");
				exit(1);
			}

			if(!memcmp(sector, zeros, sectorsize))
				continue;

			/* The backing file sector isn't zeros, so the sector
			 * of zeros in the COW file needs to be written out.
			 */
			memset(sector, 0, sectorsize);
		}

		/* The offset can't be 'offset' because that's used as an
		 * input offset, which may be offset by the COW header.
		 */
		n = pwrite(out_fd, sector, sectorsize, i * sectorsize);
		if(n != sectorsize){
			perror("Writing data");
			exit(1);
		}
	}
	free(bitmap);
	free(sector);
	free(zeros);
	close(cow_fd);
	close(out_fd);
	close(back_fd);
	return(0);
}

int Usage(char *prog) {
	fprintf(stderr, "%s usage:\n", prog);
	fprintf(stderr, "\t%s <COW file> <new backing file>\n", prog);
	fprintf(stderr, "\t%s -d <COW file>\n", prog);
	fprintf(stderr, "Creates a new filesystem image from the COW file "
		"and its backing file.\n");
	fprintf(stderr, "Specifying -d will cause a destructive, in-place "
		"merge of the COW file into\n");
	fprintf(stderr, "its current backing file\n");
	fprintf(stderr, "%s supports version 1 and 2 COW files.\n", prog);
	exit(1);
}
    
int main(int argc, char **argv)
{
	char *prog = argv[0];
	int in_place = 0;

	argv++;
	argc--;

	if(argc == 0)
		Usage(prog);

	if(!strcmp(argv[0], "-d")){
		in_place = 1;
		argv++;
		argc--;
	}

	if(in_place){
		if(argc != 1) Usage(prog);
		create_backing_file(argv[0], NULL);
	}
	else {
		if(argc != 2) Usage(prog);
		create_backing_file(argv[0], argv[1]);
	}
	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
