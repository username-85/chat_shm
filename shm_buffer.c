#include "shm_buffer.h"

#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <semaphore.h>

#define BUF_STRNUM 25

#define SHM_NAME "/chat_buf"
#define SHM_MUTEX "/chat_buf_mutex"
#define SHM_SIZE (sizeof(struct buffer))
#define NEXT_IND(index) (((index)+ 1) % BUF_STRNUM)

static int shm_fd;
struct buffer *shm_buffer;
static sem_t *shm_mutex;

// some kind of ring buffer of strings
struct buffer {
	int pcount; // number of processes that have opend the buffer
	int begin;  // index of string that is first
	int end;    // index of last string 
	char strings[BUF_STRNUM][BUF_STRLEN];
};

static void mutex_wait(void);
static void mutex_signal(void);
static int create_mutex(void);
static void delete_mutex(void);
static int inc_pcount(void);
static int dec_pcount(void);
static int get_pcount(void);

int open_shm_buffer(void)
{
	create_mutex();

	shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	if (shm_fd == -1) {
		if (shm_fd == -1) {
			perror("shm_open");
			return -1;
		}
	}

	if (ftruncate(shm_fd, SHM_SIZE) == -1) {
		perror("ftruncate");
		close(shm_fd);
		return -1;
	}

	void *addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE,
	                  MAP_SHARED, shm_fd, 0);
	if (addr == MAP_FAILED) {
		perror("mmap");
		close(shm_fd);
		return -1;
	}
	shm_buffer = addr;

	inc_pcount();

	return 0;
}

void close_shm_buffer(void)
{
	if (!shm_buffer)
		return;

	dec_pcount();

	if (get_pcount() >= 1)
		return;

	if (shm_mutex)
		delete_mutex();

	if (shm_unlink(SHM_NAME) == -1)
		perror("shm_unlink");
	int r = munmap(shm_buffer, SHM_SIZE);
	if (r != 0)
		perror("munmap");
}

int add_str_to_shm(char *str)
{
	if (!shm_buffer)
		return -1;

	mutex_wait();

	int add_index = shm_buffer->end;
	strncpy(shm_buffer->strings[add_index], str, BUF_STRLEN - 1);;
	shm_buffer->strings[add_index][BUF_STRLEN - 1] = '\0';

	if (shm_buffer->begin == NEXT_IND(shm_buffer->end))
		shm_buffer->begin = NEXT_IND(shm_buffer->begin);

	shm_buffer->end = NEXT_IND(shm_buffer->end);

	mutex_signal();

	return 0;
}

char *get_str_from_shm(void)
{
	static int read_pos;

	mutex_wait();

	if (read_pos == shm_buffer->end) {
		mutex_signal();
		return NULL;
	}

	char *ret = shm_buffer->strings[read_pos];
	read_pos = NEXT_IND(read_pos);
	mutex_signal();
	return ret;
}

static int create_mutex(void)
{
	shm_mutex = sem_open(SHM_MUTEX, O_CREAT, S_IRUSR | S_IWUSR, 1);
	if (shm_mutex == SEM_FAILED) {
		perror("sem_open");
		return -1;
	}
	return 0;
}

static void delete_mutex(void)
{
	if (!shm_mutex)
		return;

	if (sem_unlink(SHM_MUTEX) == -1)
		perror("sem_unlink");
}

static void mutex_wait(void)
{
	if (!shm_mutex) {
		fprintf(stderr, "mutex_wait .shm_mutex is not initialised.\n");
		return;
	}

	int val;
	sem_getvalue(shm_mutex, &val);

	if (sem_wait(shm_mutex) == -1)
		perror("sem_wait");
}

static void mutex_signal(void)
{
	if (!shm_mutex) {
		fprintf(stderr,
		        "mutex_signal shm_mutex is not initialised.\n");
		return;
	}

	int val;
	sem_getvalue(shm_mutex, &val);

	if (sem_post(shm_mutex) == -1)
		perror("sem_post");
}

static int inc_pcount(void)
{
	if (!shm_buffer)
		return -1;

	mutex_wait();
	shm_buffer->pcount++;
	mutex_signal();

	return 0;
}

static int dec_pcount(void)
{
	if (!shm_buffer)
		return -1;

	mutex_wait();
	shm_buffer->pcount--;
	mutex_signal();

	return 0;
}

static int get_pcount(void)
{
	if (!shm_buffer)
		return -1;

	mutex_wait();
	int ret = shm_buffer->pcount;
	mutex_signal();

	return ret;
}

