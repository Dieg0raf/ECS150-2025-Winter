Jobs jobs;
const int NUM_THREADS = 4;
pthread_mutex_t threadLock;
pthread_cond_t hasJob;

int main(int argc, char *argv[]) {
  // fill in all of the jobs. (can be more than 4 jobs)
  jobs = parseMakefile(argc, argv);

  // create the working threads
  vector<pthread_t> workingThreads;
  for (int i = 0; i < NUM_THREADS; i++) { 
    pthread_t threadId;
    pthread_create(&threadId, NULL, jobThread, NULL);
    workingThreads.push_back(threadId);
  }


  lock(threadLock)
  while (!jobs.hasJobThatCanCompile()) {
    wait(&hasJob ,threadLock);
  }
   
  unlock(threadLock)

  std::cout << "All jobs are done." << std::endl;

}

void *jobThread(void *threadArg) {
  CompilationJob job;

  lock(threadLock);

  while ((job = jobs.getNextJobThatCanCompile()) == NULL) {
    wait(&hasJob ,threadLock);
  }


  unlock(threadLock);

  compile(job);

  lock(threadLock);

  unlock(threadLock);
}
