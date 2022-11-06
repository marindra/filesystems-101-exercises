#include <solution.h>
#include <ext2fs/ext2fs.h>
#include <errno.h>
#include <unistd.h>

#include <stdio.h>

char findInode(int img, struct ext2_inode* inp, int inNum, struct ext2_super_block* supBl) {
	// read super block
	if (pread(img, supBl, sizeof(struct ext2_super_block)/*SUPERBLOCK_SIZE*/, 1024/*SUPERBLOCK_OFFSET*/) < 0) {
		return 1;
	}

	if (supBl->s_magic != EXT2_SUPER_MAGIC) {
		return 2;
	}

	//read group
	struct ext2_group_desc grDesc = {0};
	size_t groupNum = (inNum - 1) / supBl->s_inodes_per_group;
	const size_t blSize = 1024 << (supBl->s_log_block_size);
	if (pread(img, &grDesc, sizeof(struct ext2_group_desc), blSize * (supBl->s_first_data_block+1) + sizeof(struct ext2_group_desc) * groupNum) < 0) {
		return 1;
	}

	// read inode
	size_t num_in_gr = (inNum - 1) % (supBl->s_inodes_per_group);
	if (pread(img, inp, supBl->s_inode_size, grDesc.bg_inode_table * blSize + num_in_gr * (supBl->s_inode_size)) < 0) {
		return 1;
	}

	return 0;
};

int copyDirectBlock(int out, size_t blockPos, int img, size_t blSize, size_t* lastSize) {
	if (*lastSize == 0) {
		return 2; // We read all file
	}

	//char buf[blSize];
	char* buf = calloc(blSize, sizeof(char));

	if (pread(img, buf, (blSize > *lastSize) ? *lastSize : blSize, blockPos*blSize) < 0) {
		free(buf);
		return 1;
	}

	int len = write(out, buf, (blSize > *lastSize) ? *lastSize : blSize);
	if (len < 0) {
		free(buf);
		return 1;
	}
	
	free(buf);
	if ((size_t)len >= *lastSize) {
		*lastSize = 0;
	} else {
		*lastSize -= len;
	}
	// We check *lastSize == 0 in the beginning of functions so I don't see the neccessary to check it here
//	printf("Here\n");
	return 0;
};

int copyIndirectBlock(int out, size_t blockPos, int img, size_t blSize, size_t* lastSize) {
	if (*lastSize == 0) {
		return 2; // We read all file
	}

	//int block[blSize / sizeof(int)];
	int* block = calloc(blSize / sizeof(int)+1, sizeof(int));
	if (pread(img, block, blSize, blockPos * blSize) < 0) {
		free(block);
		return 1;
	}

	for (size_t i = 0; i < blSize / sizeof(int); ++i) {
		int code = copyDirectBlock(out, block[i], img, blSize, lastSize);
		if (code == 1) {
			free(block);
			return 1;
		} else if (code == 2) {
			free(block);
			return 2;
		}
	}
	free(block);
	return 0;
};

int copyDoubleIndirectBlock(int out, size_t blockPos, int img, size_t blSize, size_t* lastSize) {
	//int block[blSize / sizeof(int)];
        int* block = calloc(blSize / sizeof(int)+1, sizeof(int));
        if (pread(img, block, blSize, blockPos * blSize) < 0) {
        	free(block);
                return 1;
        }

        for (size_t i = 0; i < blSize / sizeof(int); ++i) {
        	
                int code = copyIndirectBlock(out, block[i], img, blSize, lastSize);
                if (code == 1) {
                	free(block);
                        return 1;
                } else if (code == 2) {
                	free(block);
                	return 0;
                }
        }
        free(block);
        return 0;
};


int copyFromInodeToFD(struct ext2_inode* inpIn, int out, int img, size_t blSize, size_t* lastSize) {
	//In this fuction and in functions "copy**Block"
	// 0 - all was correct
	// 1 - there was error in pread/write
	// 2 - there was no neccessary to read all 14 blocks so "break" or (*lastSize == 0) was in functions

	// 12 direct blocks
	for (int i = 0; i < EXT2_NDIR_BLOCKS; ++i) {
		int code = copyDirectBlock(out, inpIn->i_block[i], img, blSize, lastSize);
		if (code == 1) {
			return 1;
		} else if (code == 2) {
			return 0;
		}
	};

	// 1 indirect block
	int code = copyIndirectBlock(out, inpIn->i_block[EXT2_IND_BLOCK], img, blSize, lastSize);
	if (code == 1) {
		return 1;
	} else if (code == 2) {
		return 0;
	}


	if (*lastSize == 0) {
		return 0; // We read all file
	}

	// 1 double indirect
	code = copyDoubleIndirectBlock(out, inpIn->i_block[EXT2_DIND_BLOCK], img, blSize, lastSize);
	if (code == 1) {
		return 1;
	} else if (code == 2) {
		return 0;
	}
	return 0;
};

int dump_file(int img, int inode_nr, int out)
{
	(void) img;
	(void) inode_nr;
	(void) out;

	/* implement me */

	struct ext2_inode inputInode = {0};
	struct ext2_super_block supBl = {0};

	int res = findInode(img, &inputInode, inode_nr, &supBl);
	if (res == 1) {
		return -errno;
	} else if (res == 2) { // img is not ext2
		return 0;
	}

	size_t totalSize = inputInode.i_size;

	if (copyFromInodeToFD(&inputInode, out, img, 1024 << supBl.s_log_block_size, &totalSize) != 0) {
		return -errno;
	}

	return 0;
}
