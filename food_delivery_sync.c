#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define BUFFER_SIZE 10          /* order slots in the zone queue */
#define NUM_RESTAURANTS 3       /* producers */
#define NUM_PARTNERS 3          /* consumers */
#define NUM_ANALYTICS 2         /* readers   */
#define ORDERS_PER_RESTAURANT 5
#define SNAPSHOTS_PER_ANALYTICS 4

int buffer[BUFFER_SIZE];
int in = 0;
int out = 0;
int count = 0;

sem_t empty;                    /* free slots                                  */
sem_t full;                     /* ready orders                                */
sem_t rw_mutex;                 /* held by one writer OR by the group of readers */
sem_t turnstile;                /* FIFO order so analytics cannot starve dispatch */
pthread_mutex_t read_lock;      /* protects read_count                         */
int read_count = 0;

void* restaurant(void* arg) {
    int id = *((int*)arg);
    for (int i = 0; i < ORDERS_PER_RESTAURANT; i++) {
        int order = id * 100 + i;

        sem_wait(&empty);           /* 1. wait for a free slot FIRST */
        sem_wait(&turnstile);       /* 2. take a turn                */
        sem_wait(&rw_mutex);        /* 3. exclusive access           */
        sem_post(&turnstile);

        buffer[in] = order;
        count++;
        printf("Restaurant %d -> Order #%d ready at slot %d   (orders waiting: %d)\n", id, order, in, count);
        in = (in + 1) % BUFFER_SIZE;

        sem_post(&rw_mutex);
        sem_post(&full);

        usleep(rand() % 100000);
    }
    return NULL;
}

void* partner(void* arg) {
    int id = *((int*)arg);
    int total = (NUM_RESTAURANTS * ORDERS_PER_RESTAURANT) / NUM_PARTNERS;

    for (int i = 0; i < total; i++) {
        sem_wait(&full);            /* wait for a ready order FIRST */
        sem_wait(&turnstile);
        sem_wait(&rw_mutex);
        sem_post(&turnstile);

        int order = buffer[out];
        count--;
        printf("Partner %d    <- Picked up Order #%d from slot %d (orders waiting: %d)\n", id, order, out, count);
        out = (out + 1) % BUFFER_SIZE;

        sem_post(&rw_mutex);
        sem_post(&empty);

        usleep(rand() % 150000);
    }
    return NULL;
}

void* analytics(void* arg) {
    int id = *((int*)arg);
    for (int i = 0; i < SNAPSHOTS_PER_ANALYTICS; i++) {
        sem_wait(&turnstile);
        pthread_mutex_lock(&read_lock);
        read_count++;
        if (read_count == 1) sem_wait(&rw_mutex);   /* first reader locks out writers */
        pthread_mutex_unlock(&read_lock);
        sem_post(&turnstile);

        /* ---- read-only section: several dashboards may be here together ---- */
        int filled = count;
        printf("Analytics %d  :  Live dashboard -> %d orders waiting, %d slots free (readers active: %d)\n",
               id, filled, BUFFER_SIZE - filled, read_count);

        pthread_mutex_lock(&read_lock);
        read_count--;
        if (read_count == 0) sem_post(&rw_mutex);   /* last reader lets writers in */
        pthread_mutex_unlock(&read_lock);

        usleep(rand() % 200000);
    }
    return NULL;
}

int main() {
    pthread_t r_threads[NUM_RESTAURANTS], p_threads[NUM_PARTNERS], a_threads[NUM_ANALYTICS];
    int r_ids[NUM_RESTAURANTS], p_ids[NUM_PARTNERS], a_ids[NUM_ANALYTICS];

    srand(time(NULL));

    pthread_mutex_init(&read_lock, NULL);
    sem_init(&empty, 0, BUFFER_SIZE);
    sem_init(&full, 0, 0);
    sem_init(&rw_mutex, 0, 1);
    sem_init(&turnstile, 0, 1);

    printf("=== Food-Delivery Order Server (zone buffer = %d slots) ===\n\n", BUFFER_SIZE);

    for (int i = 0; i < NUM_RESTAURANTS; i++) {
        r_ids[i] = i + 1;
        pthread_create(&r_threads[i], NULL, restaurant, &r_ids[i]);
    }
    for (int i = 0; i < NUM_PARTNERS; i++) {
        p_ids[i] = i + 1;
        pthread_create(&p_threads[i], NULL, partner, &p_ids[i]);
    }
    for (int i = 0; i < NUM_ANALYTICS; i++) {
        a_ids[i] = i + 1;
        pthread_create(&a_threads[i], NULL, analytics, &a_ids[i]);
    }

    for (int i = 0; i < NUM_RESTAURANTS; i++) pthread_join(r_threads[i], NULL);
    for (int i = 0; i < NUM_PARTNERS; i++) pthread_join(p_threads[i], NULL);
    for (int i = 0; i < NUM_ANALYTICS; i++) pthread_join(a_threads[i], NULL);

    pthread_mutex_destroy(&read_lock);
    sem_destroy(&empty);
    sem_destroy(&full);
    sem_destroy(&rw_mutex);
    sem_destroy(&turnstile);

    printf("\nAll %d orders were delivered exactly once. Orders left in buffer: %d\n",
           NUM_RESTAURANTS * ORDERS_PER_RESTAURANT, count);
    return 0;
}
