#include <iostream>
#include <pthread.h>

using namespace std;

const int BUFFER_SIZE = 5;
int buffer[BUFFER_SIZE];
pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int data = 0; // Shared resource
bool ready = false; // Flag to indicate data availability

void* producer(void* arg)
{
    pthread_mutex_lock(&lock1); // Lock before modifying shared resource
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        buffer[i] = i; // Produce data
    }
    ready = true;
    cout << "Producer: Data produced!" << endl;

    pthread_cond_signal(&cond); // Signal consumer that data is ready
    pthread_mutex_unlock(&lock1); // Unlock the mutex

    return nullptr;
}

void* consumer(void* arg)
{
    pthread_mutex_lock(&lock1); // Lock before checking condition

    while (!ready) {
        // Wait for producer to signal that data is ready (lock is released and thread is put to sleep)
        pthread_cond_wait(&cond, &lock1);
    }

    for (int i = 0; i < BUFFER_SIZE; ++i) {
        int bufData = buffer[i]; // Consume data
        cout << "Consumer: Data consumed = " << bufData << endl;
    }

    pthread_mutex_unlock(&lock1); // Unlock the mutex
    return nullptr;
}

int main()
{
    pthread_t prodThread, consThread;

    // Create producer and consumer threads
    pthread_create(&consThread, nullptr, consumer, nullptr);
    pthread_create(&prodThread, nullptr, producer, nullptr);

    // Wait for threads to complete
    pthread_join(prodThread, nullptr);
    pthread_join(consThread, nullptr);

    // Destroy mutex and condition variable
    pthread_mutex_destroy(&lock1);
    pthread_cond_destroy(&cond);

    return 0;
}
