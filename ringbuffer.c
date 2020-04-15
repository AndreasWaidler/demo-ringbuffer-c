// Link with -pthread.

#define _DEFAULT_SOURCE
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define RING_SIZE 6
#define SLEEP_MICROS (3*1000*1000 + (rand() % (3 * 1000 * 1000)))
#define STATE_PROD_CALC (1<<0)
#define STATE_PROD_WAIT (1<<1)
#define STATE_CONS_CALC (1<<2)
#define STATE_CONS_WAIT (1<<3)

uint8_t ring[RING_SIZE];
size_t  head = 0; // Index of first used element in ring buffer.
size_t  tail = 0; // Index of first free slot in ring, i.e. last element + 1.
uint8_t full = 0; // because head == tail if buffer completely full or empty

uint8_t state = 0;

pthread_mutex_t lock;
sem_t fill, empty;

void print()
{
	/* ATTENTION:
	 *   printing the threads' and buffer's states obviously requires
	 *   accessing that data. This function MUST be called while holding the
	 *   mutex 'lock'.
	 */

	char  str[7 * RING_SIZE + 24] = { 0 }; // '\r' state ring ']' '\0'
	char* iter = str;
	*(iter++) = '\r';
	/* *(iter++) = '\n'; */

	char pc = state & STATE_PROD_CALC ? 'c' : ' ';
	char pw = state & STATE_PROD_WAIT ? 'w' : ' ';
	char cc = state & STATE_CONS_CALC ? 'c' : ' ';
	char cw = state & STATE_CONS_WAIT ? 'w' : ' ';
	// Mind the space for the trailing null byte.
	iter += snprintf(iter, 22, "PROD[%c%c] CONS[%c%c]   ", pc, pw, cc, cw);

	for (size_t i = 0; i < RING_SIZE; ++i) {
		char d = i == 0    ? '[' : '|';
		char h = i == head ? '>' : ' ';
		char t = i == tail ? '<' : ' ';

		char hex[3] = "__"; // Two nibbles and a tailing null byte
		if ((head  < tail && head <= i && i < tail)
		||  (tail  < head && !(tail <= i && i < head))
		||  (head == tail && full))
			snprintf(hex, 3, "%02X", ring[i]);

		// Mind the space for the trailing null byte.
		iter += snprintf(iter, 8, "%c%c%c %s ", d, h, t, hex);
	}

	iter += snprintf(iter, 2, "]");

	printf(str);
	fflush(stdout);
}

void set(uint8_t bits)
{
	pthread_mutex_lock(&lock);

	state |= bits;
	print();

	pthread_mutex_unlock(&lock);
}

void clr(uint8_t bits)
{
	pthread_mutex_lock(&lock);

	state &= ~bits;
	print();

	pthread_mutex_unlock(&lock);
}

void put(uint8_t item)
{
	// Only one thread may modify the buffer at a time
	pthread_mutex_lock(&lock);

	ring[tail] = item;
	tail = (tail + 1) % RING_SIZE;
	full = tail == head;

	print();

	pthread_mutex_unlock(&lock);
}

uint8_t get()
{
	// Only one thread may modify the buffer at a time
	pthread_mutex_lock(&lock);

	uint8_t item = ring[head];
	head = (head + 1) % RING_SIZE;
	full = !(tail == head);

	print();

	pthread_mutex_unlock(&lock);

	return item;
}

uint8_t produce()
{
	set(STATE_PROD_CALC);

	usleep(SLEEP_MICROS);
	uint8_t item = rand();

	clr(STATE_PROD_CALC);
	return item;
}

void consume(uint8_t item)
{
	set(STATE_CONS_CALC);

	/* Do something useful with the item. */
	(void) item;
	usleep(2 * SLEEP_MICROS);

	clr(STATE_CONS_CALC);
}

void initialize()
{
	pthread_mutex_init(&lock, NULL);
	sem_init(&fill, 0, 0);           // Initialize to 0
	sem_init(&empty, 0, RING_SIZE);  // Initialize to buffer size

	srand(time(NULL));
}

void* producer_thread_main(void* arg)
{
	(void) arg;

	while (1) {
		uint8_t item = produce(); // Produce a new item

		// Alternatively to using sem_wait(), you can use sem_trywait()
		// and print a message if sem_wait() would block.
		if (sem_trywait(&empty)) {
			set(STATE_PROD_WAIT);

			// sem_trywait() could not decrement the counter, but we
			// still want to block until an empty slot is available.
			sem_wait(&empty);

			clr(STATE_PROD_WAIT);
		}

		put(item);

		// Signal consumer threads that an item is ready
		sem_post(&fill);
	}
}

void* consumer_thread_main(void* arg)
{
	(void) arg;

	while (1) {
		// Wait for an item in the buffer and claim it for this consumer
		if (sem_trywait(&fill)) {
			set(STATE_CONS_WAIT);

			// sem_trywait() could not decrement the counter, but we
			// still want to block until an empty slot is available.
			sem_wait(&fill);

			clr(STATE_CONS_WAIT);
		}

		uint8_t item = get();

		// Signal producer threads that an buffer slot is empty again
		sem_post(&empty);

		consume(item); // Do something useful with the item
	}
}

int main()
{
	pthread_t prod, cons;

	initialize();

	print();

	if (pthread_create(&cons, NULL, consumer_thread_main, NULL)) {
		fprintf(stderr, "failed to start consumer\n");
		return 1;
	}

	sleep(5); // Let output print info about the consumer being blocked.

	if (pthread_create(&prod, NULL, producer_thread_main, NULL)) {
		fprintf(stderr, "failed to start producer\n");
		return 2;
	}


	pthread_join(prod, NULL);
	pthread_join(cons, NULL);

	return 0;
}
