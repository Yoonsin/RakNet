/*
Creator: Tony Do
Copyright: vanhuong.do@asicland.com
Date: 2023.05.12
*/

#include "shared_mem.h"
  
void CSharedMemory::setShmId( int id )
{
    m_shmid = id;
}
  
void CSharedMemory::setKey( key_t key )
{
    m_key = key;
}
 
void CSharedMemory::setupSharedMemory( int size )
{
    // Setup shared memory, 11 is the size 
   if ( ( m_shmid = shmget(m_key, size , 0666)) < 0 )
   {
      printf("[setupSharedMemory] Error getting shared memory id");
      exit( 1 );
   }
}

  
void CSharedMemory::attachSharedMemory()
{
   // Attached shared memory
   if ( ( m_shared_memory = (char*)(shmat( m_shmid , NULL , 0 ))) == (char *)-1)
   {
      printf("[attachSharedMemory] Error attaching shared memory id ");
      exit(1);
   }
}

void CSharedMemory::clearSharedMemory()
{
    if (m_shared_memory) memset(m_shared_memory, 0, sizeof(m_shared_memory));
}
  
void CSharedMemory::copyToSharedMemory( string str )
{
   // copy string to shared memory
   memcpy( m_shared_memory, str.c_str() , str.size() );
}

string CSharedMemory::readDataSharedMemory(int m_shmid)
{
    int shmid = shmget(m_shmid, sizeof(char), 0666);
    // attach to the shared memory segment
    char* data_ = (char*)shmat(shmid, NULL, 0);
    // read the data from the shared memory
    char data_got[1024];
    memcpy(data_got, data_, 1024);
    //cout << "I got:  " << data_got << std::endl ;
    shmdt(data_);
    void* shmdt( void *shmid );
    shmctl( shmid , IPC_RMID, NULL );

	string data_str(data_got);
    
    return data_str;
}
 
void CSharedMemory::close()
{
   sleep(3);
   // Detach and remove shared memory
   void* shmdt( void *m_shmid );
   shmctl( m_shmid , IPC_RMID, NULL ); 
}

CSharedMemory::~CSharedMemory() {
	if (m_shared_memory) shmdt(m_shared_memory);
}

void CSemaphore::setSemId(int id)
{
    m_semid = id;
}

void CSemaphore::setKey(key_t key)
{
    m_key = key;
}

void CSemaphore::setupSemaphore(int nSems)
{
    if ((m_semid = semget(m_key, nSems, 0666)) < 0)
    {
        printf("[setupSemaphore] Error getting Semaphore id");
        printf("Getting a handle to the semaphore failed; errno is %d", errno);
        exit(1);
    }
}

void CSemaphore::waitSemaphore()
{
	semop(m_semid, &sem_open, (size_t)1);
}

void CSemaphore::releaseSemaphore()
{
    semop(m_semid, &sem_close, (size_t)1);
}

void CSemaphore::close()
{
    // Detach and remove Semaphore
    semctl(m_semid, IPC_RMID, NULL);
}



//int main(int argc, const char **argv)
//{
//    int i = 0;
//    while (true)
//    {  
//        i++; 
//        string cpp_send = "Cpp sent " + std::to_string(i);
//        cout << "\nI sent: " << cpp_send << endl;
//        const char* cpp_ = cpp_send.c_str();
//        CSharedMemory m, n; 
//        m.setKey(777);
//        m.setupSharedMemory(12);
//        m.attachSharedMemory(); 
//        m.copyToSharedMemroy((char*)(cpp_));
//        m.close();
//
//        // get the shared memory ID
//        int id_read = 1234;
//        n.setKey(id_read);
//        n.readDataSharedMemory(id_read);
//    }
//}