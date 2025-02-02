#include <iostream>
#include <pthread.h>

using namespace std;

struct ThreadData
{
    int id;
    string name;
};

void *threadFunc(void *arg)
{
    ThreadData *data_struct = (ThreadData *)arg;
    cout << "Hello from " << data_struct->name  << " thread!" << endl;
    return nullptr;
}

int main(int argc, char *argv[])
{
    pthread_t thread1;
    pthread_t thread2;

    ThreadData data1 = {1 ,"first"};
    ThreadData data2 = {2 ,"second"};

    cout << "Creating two threads!" << endl;
    
    pthread_create(&thread1, nullptr, threadFunc, (void *)&data1);
    pthread_create(&thread2, nullptr, threadFunc, (void *)&data2);

    pthread_join(thread1, nullptr);
    pthread_join(thread2, nullptr);

    cout << "Threads have finished!" << endl;

    return 0;
}
