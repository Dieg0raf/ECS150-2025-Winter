#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <deque>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <typeinfo>
#include <vector>

#include "FileService.h"
#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "HttpService.h"
#include "HttpUtils.h"
#include "MyServerSocket.h"
#include "MySocket.h"
#include "dthread.h"
#include "pthread.h"

using namespace std;

int PORT = 8080;
int THREAD_POOL_SIZE = 1;
int BUFFER_SIZE = 1;
string BASEDIR = "static";
string SCHEDALG = "FIFO";
string LOGFILE = "/dev/null";

vector<HttpService*> services;
deque<MySocket*> buffer;
pthread_mutex_t threadLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t emptied = PTHREAD_COND_INITIALIZER;
pthread_cond_t filled = PTHREAD_COND_INITIALIZER;

HttpService*
find_service(HTTPRequest* request)
{
    // find a service that is registered for this path prefix
    for (unsigned int idx = 0; idx < services.size(); idx++) {
        if (request->getPath().find(services[idx]->pathPrefix()) == 0) {
            return services[idx];
        }
    }

    return NULL;
}

void invoke_service_method(HttpService* service, HTTPRequest* request, HTTPResponse* response)
{
    stringstream payload;

    // invoke the service if we found one
    if (service == NULL) {
        // not found status
        response->setStatus(404);
    } else if (request->isHead()) {
        service->head(request, response);
    } else if (request->isGet()) {
        service->get(request, response);
    } else {
        // The server doesn't know about this method
        response->setStatus(501);
    }
}

void handle_request(MySocket* client)
{
    HTTPRequest* request = new HTTPRequest(client, PORT);
    HTTPResponse* response = new HTTPResponse();
    stringstream payload;

    // read in the request
    bool readResult = false;
    try {
        payload << "client: " << (void*)client;
        sync_print("read_request_enter", payload.str());
        readResult = request->readRequest();
        sync_print("read_request_return", payload.str());
    } catch (...) {
        // swallow it
    }

    if (!readResult) {
        // there was a problem reading in the request, bail
        delete response;
        delete request;
        sync_print("read_request_error", payload.str());
        return;
    }

    HttpService* service = find_service(request);
    invoke_service_method(service, request, response);

    // send data back to the client and clean up
    payload.str("");
    payload.clear();
    payload << " RESPONSE " << response->getStatus() << " client: " << (void*)client;
    sync_print("write_response", payload.str());
    cout << payload.str() << endl;
    client->write(response->response());

    delete response;
    delete request;

    payload.str("");
    payload.clear();
    payload << " client: " << (void*)client;
    sync_print("close_connection", payload.str());
    client->close();
    delete client;
}

void* consumer(void* arg)
{
    while (true) {
        set_log_file(LOGFILE);
        // cout << "Thread " << *(int*)arg << " loop through seeking requests!" << endl;
        if (dthread_mutex_lock(&threadLock) != 0) {
            cerr << "Error occurred when trying to acquire lock in worker thread" << endl;
            continue;
        }

        while (buffer.empty()) {
            // cout << "Thread " << *(int*)arg << " is waiting for requests (buffer empty)." << endl;
            // if buffer is empty, wait until filled (release lock to allow other threads to continue)
            if (dthread_cond_wait(&filled, &threadLock) != 0) {
                cerr << "Condition wait failed in worker thread" << endl;
                dthread_mutex_unlock(&threadLock); // unlock
                continue;
            }
            // cout << "Thread " << *(int*)arg << " has been awoken." << endl;
        }

        MySocket* currentClient = buffer.front();
        // cout << "Thread " << *(int*)arg << " Size of buffer BEFORE popping front: " << buffer.size() << endl;
        buffer.pop_front();
        // cout << "Thread " << *(int*)arg << " Size of buffer AFTER popping front: " << buffer.size() << endl;

        if (dthread_mutex_unlock(&threadLock) != 0) {
            cerr << "Failed to release lock in worker thread" << endl;
        } // unlock to allow other threads to access buffer

        // cout << "Thread " << *(int*)arg << " is processing a request. Size of buffer: " << buffer.size() << endl;
        if (dthread_cond_signal(&emptied) != 0) {
            cerr << "Failed to signal condition variable in worker thread" << endl;
        }
        // if (dthread_cond_signal(&emptied) == 0) {
        //     cout << "Thread " << *(int*)arg << ": Signaled emptied, new buffer size() " << buffer.size() << endl;
        // } // signal to main thread, that queue has more room
        handle_request(currentClient);
    }

    return NULL;
    // while (true) {
    //     set_log_file(LOGFILE);
    //     dthread_mutex_lock(&threadLock)

    //     while (buffer.size() == 0) {
    //       dthread_mutex_
    //     }
    // }
}

int main(int argc, char* argv[])
{

    signal(SIGPIPE, SIG_IGN);
    int option;

    while ((option = getopt(argc, argv, "d:p:t:b:s:l:")) != -1) {
        switch (option) {
        case 'd':
            BASEDIR = string(optarg);
            break;
        case 'p':
            PORT = atoi(optarg);
            break;
        case 't':
            THREAD_POOL_SIZE = atoi(optarg);
            break;
        case 'b':
            BUFFER_SIZE = atoi(optarg);
            break;
        case 's':
            SCHEDALG = string(optarg);
            break;
        case 'l':
            LOGFILE = string(optarg);
            break;
        default:
            cerr << "usage: " << argv[0] << " [-p port] [-t threads] [-b buffers]" << endl;
            exit(1);
        }
    }

    set_log_file(LOGFILE);
    vector<pthread_t> threadPool;
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_t tid;
        int* thread_id = new int(i);

        if (dthread_create(&tid, NULL, consumer, thread_id) != 0) {
            cerr << "Error occured when creating thread" << endl;
        }
    }

    set_log_file(LOGFILE);

    sync_print("init", "");
    MyServerSocket* server = new MyServerSocket(PORT);

    // The order that you push services dictates the search order
    // for path prefix matching
    services.push_back(new FileService(BASEDIR));

    while (true) {
        sync_print("waiting_to_accept", "");
        // cout << "\nWaiting to accept" << endl;

        MySocket* client = server->accept(); // returns a unique client socket object (represents an individual client connection)

        if (dthread_mutex_lock(&threadLock) != 0) {
            cerr << "Failed to aquire lock in main thread" << endl;
        } // aquire lock

        // int currentBufSize = buffer.size();
        // cout << "Current buffer size: " << buffer.size() << "/" << BUFFER_SIZE << endl;
        while ((int)buffer.size() >= BUFFER_SIZE) {
            // cout << "Waiting to add to buffer" << endl;
            // cout << "Waiting buffer size: " << buffer.size() << "/" << BUFFER_SIZE << endl;

            if (dthread_cond_wait(&emptied, &threadLock) != 0) {
                cerr << "Condition wait failed in main thread" << endl;
                dthread_mutex_unlock(&threadLock); // unlock
                continue;
            } // if buffer is full wait until it's empty

            // cout << "Main thread was awoken!" << endl;
            // cout << "Space available in buffer now" << endl;
            // cout << "Available waiting buffer size: " << buffer.size() << "/" << BUFFER_SIZE << endl;
        }

        buffer.push_back(client); // add new request to buffer
        // cout << "Client added to buffer. Buffer size: " << buffer.size() << endl;

        if (dthread_cond_broadcast(&filled) != 0) {
            cerr << "Failed to broadcast condition variable in main thread" << endl;
        } // signal that buffer had been filled up

        if (dthread_mutex_unlock(&threadLock) != 0) {
            cerr << "Failed to release lock in main thread" << endl;
        }

        sync_print("client_accepted", "");
        // handle_request(client);
    }
}

