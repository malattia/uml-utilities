/* copyrighted (C) 2001 RidgeRun,Inc (glonnon@ridgerun.com)
 * Licensed under the GPL
 */ 

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef long long u64;

// taken from ubd_user.c

struct cow_header {
	int magic;
	int version;
	char backing_file[256];
	time_t mtime;
	u64 size;
	int sectorsize;
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

static void sizes(u64 size, int sectorsize, int *bitmap_len_out,
		  int *data_offset_out)
{
	*bitmap_len_out = (size + sectorsize - 1) / (8 * sectorsize);

	*data_offset_out = sizeof(struct cow_header) + *bitmap_len_out;
	*data_offset_out = (*data_offset_out + sectorsize - 1) / sectorsize;
	*data_offset_out *= sectorsize;
}

int create_backing_file(int argc, char *argv[])
{
	char *in = argv[1];
	char *out = argv[2];
	int cow_fd,back_fd,out_fd;
	int retval;
	struct stat64 buf;
	unsigned long *bitmap;
	int bitmap_len;
	int blocks;
	char * block;
	int i = 0;
	u64 offset;
	struct cow_header header;
	int data_offset;
	int err;

	if((cow_fd = open(in,  O_RDONLY)) < 0){
		perror("cow file open\n");
		exit(1);
	}
	if((out_fd = creat(out,0777)) < 0){
		perror("cow file open\n");
		exit(1);
	}
	if((retval = read(cow_fd,&header, sizeof(header))) != 
	   sizeof(header)) {
		perror("cow header read\n");
		exit(1);
	}
	if(header.magic != COW_MAGIC) {
		fprintf(stderr,"magic doesn't match\n");
		exit(1);
	}
	if(header.version != COW_VERSION) {
		fprintf(stderr,"version doesn't match\n");
		exit(1);
	}
	if(stat64(header.backing_file,&buf) < 0) {
		perror("can't stat back file\n");
		exit(1);
	}
	if(buf.st_size != header.size){
		fprintf(stderr,"Size mismatch (%ld vs %ld) of COW header "
			"vs backing file\n", (long int) buf.st_size, 
			(long int) header.size);
		exit(1);
	}
	if(buf.st_mtime != header.mtime) {
		fprintf(stderr,"mtime mismatch (%ld vs %ld) of COW "
			"header vs backing file\n", buf.st_mtime, 
			header.mtime);
		exit(1);
	}
	if((back_fd = open(header.backing_file,O_RDONLY)) < 0){
		perror("back file open\n");
		exit(1);
	}

	sizes(header.size,header.sectorsize,&bitmap_len,&data_offset);
		
	bitmap = (unsigned long *) malloc(bitmap_len);
	if(bitmap == NULL) {
		fprintf(stderr,"can't allocate a bitmap");
		exit(1);
	}
	err = lseek64(cow_fd,sizeof(header),SEEK_SET);
	if(err < 0) 
		exit(1);
	err = read(cow_fd,bitmap,bitmap_len);
	if(err != bitmap_len)
		exit(1);
	blocks = bitmap_len * 8;
	block = (char *) malloc(sizeof(header.sectorsize));
	
	while(i < blocks) {
		if(ubd_test_bit(i,bitmap)) {
			offset = data_offset + i * header.sectorsize;
			err = lseek(cow_fd,offset,SEEK_SET);
			if(err < 0)
				exit(1);
			err = read(cow_fd,block,header.sectorsize);
			if(err != header.sectorsize )
				exit(1);
		} else {
			offset = i * header.sectorsize;
			err = lseek(back_fd,offset,SEEK_SET);
			if(err < 0)
				exit(1);
			err = read(back_fd,block,header.sectorsize);
			if(err != header.sectorsize)
				exit(1);
		}
		err = write(out_fd,block,header.sectorsize);
		if(err != header.sectorsize)
			exit(1);
		i++;
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
	printf("creates a new backing file from the cow file.\n");
	return 0;
}


    
int main(int argc, char *argv[]) {
	if(argc == 3)
		create_backing_file(argc,argv);
	else
		usage(argv[0]);
	return 0;
}


/*
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
