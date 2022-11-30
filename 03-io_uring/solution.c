#include <solution.h>
#include <liburing.h>
#include <sys/stat.h>
#include <errno.h>
//#include <memory.h>
#include <stdio.h> //!!!!
#include <stdlib.h>

const size_t BlockSize = 1 << 18;

struct inputFileDesc {
	int in;
	off_t offset;
	size_t leftToRead;
};

struct outputFileDesc {
	int out;
	size_t leftToWrite;
};

struct myData {
	off_t offset;
	char* buf;
	size_t bufSize;
	int isRead;
};

void startRead(struct io_uring* ringPtr, struct inputFileDesc* inDPtr) {
	struct io_uring_sqe* sqe = io_uring_get_sqe(ringPtr);

	struct myData* dataP = (struct myData*) malloc(sizeof(struct myData));
	dataP->bufSize = (BlockSize <= inDPtr->leftToRead) ? BlockSize : inDPtr->leftToRead;
	
	dataP->buf = (char*) malloc((dataP->bufSize) * sizeof(char));
	dataP->offset = inDPtr->offset;
	dataP->isRead = 1; // true
	inDPtr->leftToRead -= dataP->bufSize;
	inDPtr->offset += (dataP->bufSize);
	//printf("I want read. offset is %ld\n", dataP->offset);

	io_uring_prep_read(sqe, inDPtr->in, dataP->buf, dataP->bufSize, dataP->offset);

	io_uring_sqe_set_data(sqe, dataP);
        io_uring_submit(ringPtr);
}

void startWrite(struct io_uring* ringPtr, struct outputFileDesc* outDPtr, struct myData* dataP) {
	struct io_uring_sqe* sqe = io_uring_get_sqe(ringPtr);

        outDPtr->leftToWrite -= dataP->bufSize;
	//printf("I want write. offset is %ld\n", dataP->offset);

        io_uring_prep_write(sqe, outDPtr->out, dataP->buf, dataP->bufSize, dataP->offset);

	dataP->isRead = 0;
        io_uring_sqe_set_data(sqe, dataP);
        io_uring_submit(ringPtr);

}

struct myData* getData(struct io_uring* ringPtr, int* res) {
	struct io_uring_cqe* cqe = NULL;
	io_uring_wait_cqe(ringPtr, &cqe);
	struct myData* dataP = io_uring_cqe_get_data(cqe);
	*res = cqe->res;
	io_uring_cqe_seen(ringPtr, cqe);
	if (*res < 0) { // if error occured we finish work, so memory should be clear
		free(dataP->buf);
		free(dataP);
		return NULL;
	}
	
	if (dataP->isRead == 0) { // it was writing
		free(dataP->buf);
		free(dataP);
		return NULL;
	} else {
		return dataP;
	}
}

int copy(int in, int out)
{
	(void) in;
	(void) out;

	/* implement me */

	const unsigned int Entries = 4;

	struct inputFileDesc inD;
	inD.in = in;
	inD.offset = 0;

	struct outputFileDesc outD;
	outD.out = out;

	{
		struct stat inStat;
		if (fstat(inD.in, &inStat) == -1) {
			// It's not read or write, so we don't return -errno
			return 1;
		}
		inD.leftToRead = inStat.st_size;
		outD.leftToWrite = inStat.st_size;
		//printf("Total size is %ld\n", inD.leftToRead);
	}

	struct io_uring ring;
	if (io_uring_queue_init(Entries, &ring, 0) != 0) {
		// It's not read or write, so we don't return -errno
		return 2;
	}

	int notWritedEvents = 0; // it can't be more than 4

	for (int i = 0; (i < 4) && (inD.leftToRead > 0); ++i) {
		startRead(&ring, &inD);
		++notWritedEvents;
		//printf("left to read is %ld\n", inD.leftToRead);
	}
	
	//printf("Read count is %d\n", notWritedEvents);
	struct myData* dataP = NULL;
	int res = 0;
	while ((notWritedEvents != 0) || (outD.leftToWrite != 0)) {
		//printf("I'm at the beginning of cycle\n");
		dataP = getData(&ring, &res);
		if (res < 0) {
			printf("ooops! smth wrong! %d\n", res);
			return res; //-errno
		}
		if (dataP == NULL) { // we finish one writing in out
			--notWritedEvents;
			//printf("writed\n");
			if (inD.leftToRead > 0) {
				startRead(&ring, &inD);
				++notWritedEvents;
			}
		} else { // we finish one reading from file
			//printf("readed\n");
			if (outD.leftToWrite > 0) {
				startWrite(&ring, &outD, dataP);
			}
			//printf("Now not writed events is %d and left to write is %ld\n", notWritedEvents, outD.leftToWrite);
		}
	}

	return 0;
}
