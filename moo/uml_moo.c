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
#include <netinet/in.h>

#if __BYTE_ORDER == __BIG_ENDIAN
# define ntohll(x) (x)
# define htonll(x) (x)
#elif __BYTE_ORDER == __LITTLE_ENDIAN
# define ntohll(x)  bswap_64(x)
# define htonll(x)  bswap_64(x)
#else
#error "__BYTE_ORDER not defined"
#endif

typedef long long u64;

/* XXX All these fields should be word-length independent */

struct cow_header_common {
       unsigned long magic;
       unsigned long version;
};

/* taken from ubd_user.c */

#define PATH_LEN_V1 256

struct cow_header_v1 {
	struct cow_header_common common;
	char backing_file[PATH_LEN_V1];
	time_t mtime;
	u64 size;
	int sectorsize;
};

#define PATH_LEN_V2 MAXPATHLEN

struct cow_header_v2 {
	struct cow_header_common common;
	char backing_file[PATH_LEN_V2];
	time_t mtime;
	u64 size;
	int sectorsize;
};
 
union cow_header {
       struct cow_header_v1 v1;
       struct cow_header_v2 v2;
};

#define COW_MAGIC 0x4f4f4f4d  /* mooo */
#define COW_VERSION 1

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

static void sizes(u64 size, int sectorsize, int bitmap_offset, 
		  int *bitmap_len_out, int *data_offset_out)
{
	*bitmap_len_out = (size + sectorsize - 1) / (8 * sectorsize);

        *data_offset_out = bitmap_offset + *bitmap_len_out;
	*data_offset_out = (*data_offset_out + sectorsize - 1) / sectorsize;
	*data_offset_out *= sectorsize;
}

int create_backing_file(char *in, char *out)
{
	int cow_fd, back_fd, in_fd, out_fd;
	struct stat buf;
	unsigned long *bitmap;
	int bitmap_len;
	int sectors;
	void *sector, *zeros;
	u64 size, offset, n, i; /* i is u64 to prevent 32 bit overflow */
        union cow_header header;
	struct cow_header_common common;
	int data_offset;
        unsigned long magic, version;
        char *backing_file;
        time_t mtime;
        int sectorsize, bitmap_offset, perms;

	if((cow_fd = open(in,  O_RDONLY)) < 0){
		perror("COW file open");
		exit(1);
	}

	if(read(cow_fd, &common, sizeof(common)) != sizeof(common)){
		perror("COW common header read");
		exit(1);
	}
	if(lseek(cow_fd, 0, SEEK_SET) != 0){
		perror("seeking back to COW file start");
		exit(1);
	}

        magic = common.magic;
        if(magic == COW_MAGIC) version = common.version;
        else if(magic == ntohl(COW_MAGIC)) version = ntohl(common.version);
	else {
                fprintf(stderr,"magic number (0x%x) doesn't match COW_MAGIC "
			"(0x%x)\n", (unsigned int) magic, COW_MAGIC);
                exit(1);
        }

	if(version == 1){
		if(read(cow_fd, &header.v1, sizeof(header.v1)) != 
		   sizeof(header.v1)){
			perror("Reading full V1 COW header");
			exit(1);
		}
		backing_file = header.v1.backing_file;
		size = header.v1.size;
		mtime = header.v1.mtime;
		sectorsize = header.v1.sectorsize;
		bitmap_offset = sizeof(header.v1);
	}
	else if(version == 2){
		if(read(cow_fd, &header.v2, sizeof(header.v2)) != 
		   sizeof(header.v2)){
			perror("Reading full V2 COW header");
			exit(1);
		}
		backing_file = header.v2.backing_file;
		size = ntohll(header.v2.size);
		mtime = ntohl(header.v2.mtime);
		sectorsize = ntohl(header.v2.sectorsize);
		bitmap_offset = sizeof(header.v2);
	} 
	else {
		fprintf(stderr,"Unknown version %ld\n", version);
                exit(1);
        }

	if(stat(backing_file, &buf) < 0) {
		perror("Stating backing file");
		exit(1);
	}
	if(buf.st_size != size){
		fprintf(stderr,"Size mismatch (%ld vs %ld) of COW header "
			"vs backing file\n", (long int) buf.st_size, 
			(long int) size);
		exit(1);
	}
	if(buf.st_mtime != mtime) {
		fprintf(stderr,"mtime mismatch (%ld vs %ld) of COW "
			"header vs backing file\n", buf.st_mtime, 
			mtime);
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

	sizes(size, sectorsize, bitmap_offset, &bitmap_len, &data_offset);
		
	bitmap = (unsigned long *) malloc(bitmap_len);
	if(bitmap == NULL) {
		perror("Can't allocate bitmap");
		exit(1);
	}

	if(pread(cow_fd, bitmap, bitmap_len, bitmap_offset) != bitmap_len){
		perror("Reading COW bitmap");
		exit(1);
	}

	sectors = bitmap_len * 8;
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

		/* Sparse file creation - if the sector is all zeros, then
		 * we don't need to write it out.  Unless it's the last
		 * sector, in which case it needs to be written in order to
		 * make the file size right.
		 */
		if((i < sectors - 1) && !memcmp(sector, zeros, sectorsize))
			continue;

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
