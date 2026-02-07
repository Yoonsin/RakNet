#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <string> 
#include <iostream>

using namespace std;

class CSharedMemory
{
private:
    int m_shmid;
    key_t m_key;
    char* m_shared_memory;
    char* read_data;

public:
    ~CSharedMemory();
    void setShmId(int key);
    void setKey(key_t key);
    void setupSharedMemory(int size);
    void attachSharedMemory();
    void copyToSharedMemory(string str);
    void clearSharedMemory();
    string readDataSharedMemory(int m_shmid);
    void close();
};

class CSemaphore
{
private:
    int m_semid;
    key_t m_key;
    struct sembuf sem_open = { 0, -1, SEM_UNDO | IPC_NOWAIT };
    struct sembuf sem_close = { 0, 1, SEM_UNDO | IPC_NOWAIT };

public:
    void setSemId(int id);
    void setKey(key_t key);
    void setupSemaphore(int nSems);
    void waitSemaphore();
    void releaseSemaphore();
    void close();
};