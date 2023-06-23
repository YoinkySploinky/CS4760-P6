#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/msg.h>
#include <fstream>

#define BUFF_SZ sizeof ( int ) * 10

using namespace std;
const int PERMS = 0644;

//message buffer
struct msgbuffer {
   long mtype; //needed
   int pid; //pid of child
   int page; //page
   int memAddress; //memory address
   int dirtyBit; //dirt bit if set to -1 means worker is terminating
};


int main(int argc, char *argv[]) {

  struct msgbuffer buf;
  buf.mtype = 1;
  int msqid = 0;
  key_t key;
    
  if ((key = ftok("oss.cpp", 'B')) == -1) {
    perror("ftok");
    exit(1);
   }
   
  if ((msqid = msgget(key, PERMS)) == -1) {
    perror("msgget in child");
    exit(1);
  }
    
  msgbuffer rcvbuf;
  msgbuffer sndbuf;
  srand((unsigned) time(NULL) * getpid());
  
  int page, request, randCheck, readOrWrite, totalAddresses = 0;
  
  //worker will loop asking for memory addresses 1000 times and then will terminate
  while(totalAddresses < 1000) {
    page = (0 + rand() % 32); 
    request = page * 1024 + (0 + rand() % 1023);
    randCheck = 1 + rand() % 100;
    //if 85 or lower read else write
    if (randCheck < 85) {
      readOrWrite = 0;
    }
    else {
      readOrWrite = 1;
    }
    totalAddresses++;
    
    sndbuf.mtype = getppid();
    sndbuf.pid = getpid();
    sndbuf.page = page;
    sndbuf.memAddress = request;
    sndbuf.dirtyBit = readOrWrite;
    if (msgsnd(msqid, &sndbuf, 20, 0) == -1) {
      perror("msgsnd to parent failed\n");
      exit(1);
    }
      
    if ( msgrcv(msqid, &rcvbuf, 20, getpid(), 0) == -1) {
      perror("failed to receive message from parent\n");
      exit(1);
    }
    
  }
  
  //sets Dirty bit to a neg number which lets oss know that this worker is terminating
  readOrWrite = -1;
  
  
  //Second msg send to catch the break option from the while loop
  sndbuf.mtype = getppid();
  sndbuf.pid = getpid();
  sndbuf.page = page;
  sndbuf.memAddress = request;
  sndbuf.dirtyBit = readOrWrite;
  if (msgsnd(msqid, &sndbuf, 20, 0) == -1) {
    perror("msgsnd to parent failed\n");
    exit(1);
  }
  if ( msgrcv(msqid, &rcvbuf, 20, getpid(), 0) == -1) {
    perror("failed to receive message from parent\n");
    exit(1);
  }
  
  return 0;
}