/* Copyrighted (C) 2001 RidgeRun,Inc (glonnon@ridgerun.com)
 * With modifications by Jeff Dike, James McMechan, and Steve Schmidtke.
 * Licensed under the GPL
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
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

int create_backing_file(int argc, char *argv[])
{
	char *in = argv[1];
	char *out = argv[2];
	int cow_fd, back_fd, in_fd, out_fd;
	struct stat64 buf;
	unsigned long *bitmap;
	int bitmap_len;
	int blocks;
	char * block;
	int i;
	u64 offset;
        union cow_header header;
	struct cow_header_common common;
	int data_offset;
        unsigned long magic, version;
        char *backing_file;
        time_t mtime;
        u64 size;
        int sectorsize, bitmap_offset;

	if((cow_fd = open(in,  O_RDONLY)) < 0){
		perror("COW file open");
		exit(1);
	}
	if((out_fd = creat(out, 0644)) < 0){
		perror("Output file open");
		exit(1);
	}

	if(read(cow_fd, &common, sizeof(common)) != sizeof(common)){
		perror("COW common header read");
		exit(1);
	}
	if(lseek64(cow_fd, 0, SEEK_SET) != 0){
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

	if(stat64(backing_file, &buf) < 0) {
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

	if((back_fd = open(backing_file, O_RDONLY)) < 0){
		perror("Opening backing file");
		exit(1);
	}

	sizes(size, sectorsize, bitmap_offset, &bitmap_len, &data_offset);
		
	bitmap = (unsigned long *) malloc(bitmap_len);
	if(bitmap == NULL) {
		perror("Can't allocate bitmap");
		exit(1);
	}

	if(lseek64(cow_fd, bitmap_offset, SEEK_SET) < 0){
		perror("Seeking to COW bitmap");
		exit(1);
	}

	if(read(cow_fd, bitmap, bitmap_len) != bitmap_len){
		perror("Reading COW bitmap");
		exit(1);
	}

	blocks = bitmap_len * 8;
	block = (char *) malloc(sectorsize);
	if(block == NULL){
		perror("Malloc of buffer");
		exit(1);
	}
	
	for(i = 0; i < blocks; i++){
		offset = i * sectorsize;
		if(ubd_test_bit(i,bitmap)){
			offset += data_offset;
			in_fd = cow_fd;
		}
		else in_fd = back_fd;
		if(lseek64(in_fd, offset, SEEK_SET) < 0){
			perror("Seeking into data");
			exit(1);
		}

		if(read(in_fd, block, sectorsize) != sectorsize){
			perror("Reading data");
			exit(1);
		}

		if(write(out_fd, block, sectorsize) != sectorsize){
			perror("Writing data");
			exit(1);
		}
	}
	free(bitmap);
	free(block);
	close(cow_fd);
	close(out_fd);
	close(back_fd);
	return 0;
}

int usage(char *prog) {
	printf("%s usage:\n",prog);
	printf("%s <COW file> <new backing file>\n", prog);
	printf("creates a new filesystem image from the COW file and its.\n");
	printf("backing file.\n");
	printf("%s supports version 1 and 2 COW files.\n", prog);
	return 0;
}
    
int main(int argc, char **argv)
{
	if(argc == 3) create_backing_file(argc, argv);
	else usage(argv[0]);
	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
