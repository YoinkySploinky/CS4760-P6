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
#include <errno.h>

#define BUFF_SZ sizeof ( int ) * 10
const int SHMKEY1 = 4201069;
const int SHMKEY2 = 4201070;
const int PERMS = 0644;
using namespace std;

int shmid1;
int shmid2;
int msqid;
int *nano;
int *sec;  
  
//message buffer
struct msgbuffer {
   long mtype; //needed
   int pid; //pid of child
   int page; //page
   int memAddress; //memory address
   int dirtyBit; //dirt bit if set to -1 means worker is terminating
};

//struct for the frame table
struct frameTable {
  int pid; //shows which pid is occupying current frame
  int occupied; //shows if frame is occupied
  int dirtyBit; //shows if frame is read or write
  string head; // shows if current frame is head of FIFO 
};

void myTimerHandler(int dummy) {
  
  shmctl( shmid1, IPC_RMID, NULL ); // Free shared memory segment shm_id
  shmctl( shmid2, IPC_RMID, NULL ); 
  if (msgctl(msqid, IPC_RMID, NULL) == -1) { //Free memory queue
      perror("msgctl");
      exit(1);
   }
  cout << "Oss has been running for 3 seconds! Freeing shared memory before exiting" << endl;
  cout << "Shared memory detached" << endl;
  kill(0, SIGKILL);
  exit(1);

}

static int setupinterrupt(void) { /* set up myhandler for SIGPROF */
  struct sigaction act;
  act.sa_handler = myTimerHandler;
  act.sa_flags = 0;
  return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}

static int setupitimer(void) { /* set ITIMER_PROF for 60-second intervals */
  struct itimerval value;
  value.it_interval.tv_sec = 3;
  value.it_interval.tv_usec = 0;
  value.it_value = value.it_interval;
  return (setitimer(ITIMER_PROF, &value, NULL));  
}

void myHandler(int dummy) {
    shmctl( shmid1, IPC_RMID, NULL ); // Free shared memory segment shm_id
    shmctl( shmid2, IPC_RMID, NULL );
    if (msgctl(msqid, IPC_RMID, NULL) == -1) { //Free memory queue
      perror("msgctl");
      exit(1);
   } 
    cout << "Ctrl-C detected! Freeing shared memory before exiting" << endl;
    cout << "Shared memory detached" << endl;
    kill(0, SIGKILL);
    exit(1);
}

void initClock(int check) {

  
  if (check == 1) {
    shmid1 = shmget ( SHMKEY1, BUFF_SZ, 0777 | IPC_CREAT );
    if ( shmid1 == -1 ) {
      fprintf(stderr,"Error in shmget ...\n");
      exit (1);
    }
    shmid2 = shmget ( SHMKEY2, BUFF_SZ, 0777 | IPC_CREAT );
    if ( shmid2 == -1 ) {
      fprintf(stderr,"Error in shmget ...\n");
      exit (1);
    }
    cout << "Shared memory created" << endl;
    // Get the pointer to shared block
    sec = ( int * )( shmat ( shmid1, 0, 0 ) );
    nano = ( int * )( shmat ( shmid2, 0, 0 ) );
    *sec = 0;
    *nano = 0;
    return;
  }
//detaches shared memory
  else {
    shmdt(sec);    // Detach from the shared memory segment
    shmdt(nano);
    shmctl( shmid1, IPC_RMID, NULL ); // Free shared memory segment shm_id
    shmctl( shmid2, IPC_RMID, NULL ); 
    cout << "Shared memory detached" << endl;
    return;
  }
  
}

void incrementClock(int incNano, int incSec) {

  int * clockSec = ( int * )( shmat ( shmid1, 0, 0 ) );
  int * clockNano = ( int * )( shmat ( shmid2, 0, 0 ) );
  
  *clockNano = *clockNano + incNano;
  if (*clockNano >= 1000000000) {
    *clockNano = *clockNano - 1000000000;
    *clockSec = *clockSec + 1;
  }
  *clockSec = *clockSec + incSec;
  shmdt(clockSec);
  shmdt(clockNano);
  return;
  
}  
  
int main(int argc, char *argv[])  {

//Ctrl-C handler
  signal(SIGINT, myHandler);
  if (setupinterrupt() == -1) {
    perror("Failed to set up handler for SIGPROF");
    return 1;  
  }
  if (setupitimer() == -1) {
    perror("Failed to set up the ITIMER_PROF interval timer");
    return 1;
  }


//Had issues with default selection in switch decided to have an argc catch at the beginning to insure that more than one option is given
  if (argc == 1) {
  
    cout << "Error! No parameters given, enter ./oss -h for how to operate this program" << endl;
    exit(1);

  }
  
  int opt, optCounter = 0;
  int status;
  string fValue;

//opt function to collect command line params  
  while ((opt = getopt ( argc, argv, "hr:f:" ) ) != -1) {
    
    optCounter++;
    
    switch(opt) {
    
      case 'h':
        cout << "Usage: ./oss -f logFileName" << endl;
        cout << "-f: The name of the file the program will write to for logging." << endl;
        exit(1);
        
        case 'f':
          fValue = optarg;
          while (fValue == "") {
            cout << "Error! No log file name given! Please provide a log file!" << endl;
            cin >> fValue;
          }
          break;
    } 
  }
    
//setup logfile
  ofstream logFile(fValue.c_str());
    
//Creates seed for random gen
    int randSec = 0, randNano = 0;
    srand((unsigned) time(NULL));
    initClock(1);
    
//create Message queue
  struct msgbuffer buf;
  key_t key;
  
  if ((key = ftok("oss.cpp", 'B')) == -1) {
      perror("ftok");
      exit(1);
   }
   
   if ((msqid = msgget(key, PERMS | IPC_CREAT)) == -1) {
      perror("msgget");
      exit(1);
   }
   
  //creates and inits the frame tables 
  struct frameTable frameTable[256];
  for(int i = 0; i < 256; i++) {
    frameTable[i].occupied = 0;
    frameTable[i].dirtyBit = 0;
    frameTable[i].pid = 0;
    frameTable[i].head = "";
  }
  
  int totalProcess = 0, numProcess = 0, headLoc = 0, pageFault = 0;
  int clockCheck = 0, checkTime = 0;
  int * clockSec = ( int * )( shmat ( shmid1, 0, 0 ) );
  int * clockNano = ( int * )( shmat ( shmid2, 0, 0 ) );
  pid_t pid;
  msgbuffer rcvbuf;
  
  
  while(totalProcess < 100) {
    
    if(numProcess < 18) {
      if (clockCheck <= *clockNano + rand() % 500) {
        char *args[]={"./worker", NULL};
        pid = fork();
        if(pid == 0) {
          execvp(args[0],args);
          cout << "Exec failed! Terminating" << endl;
          logFile << "Exec failed! Terminating" << endl;   
          exit(1);   
        }
        else {
          logFile << "Creating worker: " << pid << endl;
          incrementClock(5000, 0);
          numProcess++;
          totalProcess++;
          clockCheck = *clockNano + 500;
        }
      }
    }
    
    //message receieve with no wait
    if(msgrcv(msqid, &rcvbuf, 20, 0, IPC_NOWAIT)==-1) {
      if(errno == ENOMSG) {
        //cout << "no message" << endl;
      }
      else {
        printf("Got an error from msgrcv\n");
        perror("msgrcv");
        exit(1); 
      }
    }
    else { 
      //if dirtyBit < 0 means worker is terminating will free up all associated frames
      if(rcvbuf.dirtyBit < 0) {
        logFile << "PID: " << rcvbuf.pid << " is terminating releasing all associated memory" << endl;
        numProcess--;
        for(int i = 0; i < 256; i++) {
          if(frameTable[i].pid == rcvbuf.pid) {
            frameTable[i].pid = 0;
            frameTable[i].occupied = 0;
            frameTable[i].dirtyBit = 0;
            frameTable[i].head = "";
          }
        }
      }
      else {
        buf.mtype = rcvbuf.pid;
        if(rcvbuf.dirtyBit == 0) {
          logFile << "PID: " << rcvbuf.pid << " is requesting read of address " << rcvbuf.memAddress << " at time " << *clockSec << ":" << *clockNano << endl;
          incrementClock(100, 0);
          if(frameTable[headLoc].occupied == 0) {
            logFile << "Address " << rcvbuf.memAddress << " in frame " << headLoc << ", giving data to PID: " << rcvbuf.pid << " at time " << *clockSec << ":" << *clockNano << endl;
            frameTable[headLoc].occupied = 1;
            frameTable[headLoc].dirtyBit = 0;
            frameTable[headLoc].pid = rcvbuf.pid;
            incrementClock(100, 0);
          }
          else {
            logFile << "Address " << rcvbuf.memAddress <<" not in a frame, page fault" << endl;
            pageFault++;
            logFile << "Clearing frame " << headLoc << " and swapping in PID: " << rcvbuf.pid << " page " << rcvbuf.page << endl;
            incrementClock(500, 0);
            frameTable[headLoc].occupied = 1;
            frameTable[headLoc].dirtyBit = 0;
            frameTable[headLoc].pid = rcvbuf.pid;
            incrementClock(100, 0);

          }
        }
        else {
          logFile << "PID: " << rcvbuf.pid << " is requesting write of address " << rcvbuf.memAddress << " at time " << *clockSec << ":" << *clockNano << endl;
          incrementClock(100, 0);
          if(frameTable[headLoc].occupied == 0) {
            logFile << "Address " << rcvbuf.memAddress << " in frame " << headLoc << ", writing data to frame at time " << *clockSec << ":" << *clockNano << endl;
            frameTable[headLoc].occupied = 1;
            frameTable[headLoc].dirtyBit = 1;
            frameTable[headLoc].pid = rcvbuf.pid;
            logFile << "Dirty bit of frame " << headLoc << " set, adding additional time to clock" << endl;
            incrementClock(14000, 0);
          }
          else {
            logFile << "Address " << rcvbuf.memAddress <<" not in a frame, page fault" << endl;
            pageFault++;
            logFile << "Clearing frame " << headLoc << " and swapping in PID: " << rcvbuf.pid << " page " << rcvbuf.page << endl;
            incrementClock(500, 0);
            frameTable[headLoc].occupied = 1;
            frameTable[headLoc].dirtyBit = 1;
            frameTable[headLoc].pid = rcvbuf.pid;
            logFile << "Dirty bit of frame " << headLoc << " set, adding additional time to clock" << endl;
            incrementClock(14000, 0);
          }
        }
      }
    }
    
    //set send to any value cause it dont matter 
    //sends msg back the worker allowing to continue or close
    if (msgsnd(msqid, &buf, 20, 0) == -1){
        perror("msgsnd");
    }
    //increments the clock by 50000ns
    incrementClock(50000, 0);
    
    if(checkTime == *clockSec) {
      frameTable[headLoc].head = "*";
      logFile << "One second has passed printing memory layout" << endl;
      checkTime++;
      logFile << "         Occupied    Dirtybit    HeadOfFIFO" << endl; 
      for(int i = 0; i < 256; i++){
        logFile <<  "Frame " << i << ":      " << frameTable[i].occupied << "           " << frameTable[i].dirtyBit << "             " << frameTable[i].head << endl;
      }
      
    }
    
    for(int i = 0; i < 256; i++){
      if(frameTable[i].head == "*") {
        frameTable[i].head = "";
      }
    }
    headLoc++;
    if(headLoc >= 256) {
      headLoc = 0;
    }
  }
   
  cout << "Oss finished" << endl;
  logFile << "Oss finished" << endl;
  shmdt(clockSec);
  shmdt(clockNano);
  if (msgctl(msqid, IPC_RMID, NULL) == -1) {
      perror("msgctl");
      exit(1);
   }
  initClock(0);
  return 0;
  
}