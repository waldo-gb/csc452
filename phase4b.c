/*
* Authors: Connor O'Neill and Waldo Guzman
* File: phase4b.c
* This is used to simulate the clock and terminal devices
* and how reading and writing to the terminal works
*/
#include "usloss.h"
#include "usyscall.h"
#include "phase4.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#define MAXPROC         50
#define MAX_SYSCALLS    50

extern void (*systemCallVec[MAX_SYSCALLS])(USLOSS_Sysargs *args);

//struct for a process that is asleep
typedef struct sleepingProcess{
      int pid; 
      int wakeTime; 
      struct sleepingProcess *next; 
}sleeper;

//struct for a disk request
typedef struct DiskReq {
    USLOSS_DeviceRequest req;
    int diskUnit;
    int numSectors;
    int startTrackNum;
    int startBlockNum;
    char *buffer;
    struct DiskReq *next;
} DiskReq;


//struct for the state of the disk
typedef struct DiskState{
      USLOSS_DeviceRequest req;
      int currentTrack;
      char *buffer;
} DiskState;
 
//global variables
DiskState disks[USLOSS_DISK_UNITS];
DiskReq *requests[USLOSS_DISK_UNITS];
int diskDaemonMBoxIDs[USLOSS_DISK_UNITS];
int diskLocks[USLOSS_DISK_UNITS];

int terminalLocks[USLOSS_MAX_UNITS];
int termReadMBoxes[USLOSS_MAX_UNITS];
int termWriteMBoxes[USLOSS_MAX_UNITS];
int termBuffers[USLOSS_MAX_UNITS][MAXLINE+1];
int termBufferLengths[USLOSS_MAX_UNITS];

int currentTicks; 
sleeper sleepers[MAXPROC]; 
sleeper *queue = NULL; 

DiskReq *dequeueDiskRequest(int diskUnit);

//adds a sleeping process to the queue
void addToQueue(sleeper* process){
    if(queue == NULL || process->wakeTime <= queue->wakeTime){
        process->next = queue;
        queue = process;
    } else {
        sleeper *current = queue;
        while(current->next != NULL && current->next->wakeTime < process->wakeTime){
            current = current->next;
        }
        process->next = current->next;
        current->next = process;
    }
}

//finds an open index for a sleeping process
int findOpenSleeper(int pid){
    int tableIndex =  pid % MAXPROC;
    if(sleepers[tableIndex].pid == -1){
        return tableIndex;
    }else{
        return -1;
    }
    
}

//handles the kernel syscall for sleep
void kernSleep(USLOSS_Sysargs *sysargs){
      int *seconds  = (int *)sysargs->arg1;
      if(seconds < 0){
            sysargs->arg4 = (void*)(long)-1;
            return;
      }
      int time = seconds;
      time  = time * 10;
      int wakeupTime = currentTicks + time;
      int pid = getpid();
      int sleeperIndex = findOpenSleeper(pid);
      sleeper *sleepingProcess = &sleepers[sleeperIndex];
      sleepingProcess->pid = pid;
      sleepingProcess->wakeTime = wakeupTime;
      sleepingProcess->next = NULL;
      addToQueue(sleepingProcess);
      blockMe(13);
      

}

//helper to find a free slot
void freeSlot(sleeper *process){
      process->next = NULL;
      process->pid = -1;
      process->wakeTime = -1;
}

//daemon for the sleep function
void sleepDaemon(){
    int waitStatus;
    while (1){
        waitDevice(USLOSS_CLOCK_DEV, 0, &waitStatus);
        currentTicks++;
        while(queue != NULL && queue->wakeTime <= currentTicks){
            sleeper *tempPointer = queue;
            int wakePID = tempPointer->pid;
            queue = queue->next;
            unblockProc(wakePID);
            freeSlot(tempPointer);
        }
    }
}

//gives the lock to a process for the terminal
void lockTerminal(int termUnit){
      MboxSend(terminalLocks[termUnit], NULL, 0);
}

//returns the lock
void unlockTerminal(int termUnit){
      MboxRecv(terminalLocks[termUnit], NULL, 0);
}

//handles the term read call in kernel
void kernTermRead(USLOSS_Sysargs *args){
      char *buffer = (char*)(long) args->arg1;
      int bufferSize = (int)(long) args->arg2;
      int termUnit = (int)(long) args->arg3;

      if (bufferSize <= 0 || bufferSize > MAXLINE || termUnit < 0 || termUnit >= USLOSS_MAX_UNITS){
            args->arg2 = 0;
            args->arg4 = (void*)(long)-1;
            return;
      }

      lockTerminal(termUnit);
      char buff[MAXLINE+1];
      memset(buff, '\0', MAXLINE+1); 

      int size = MboxRecv(termReadMBoxes[termUnit], buff, MAXLINE);
      strcpy(buffer, buff);
      unlockTerminal(termUnit);

      if (strlen(buff) > bufferSize){
            args->arg2 = bufferSize;
            args->arg4 = 0;
      } else {
            args->arg2 = strlen(buff);
            args->arg4 = 0;
      }
}

//writes a character
void writeChar(int termUnit, char character){
      int cr_val = 0x1;
      cr_val |= 0x2;
      cr_val |= 0x4;
      cr_val |= (character << 8);
      USLOSS_DeviceOutput(USLOSS_TERM_DEV, termUnit, (void*)(long)cr_val);
}

//handles writing to a terminal in kernel
void kernTermWrite(USLOSS_Sysargs *args){
      char *buffer = (char*)(long) args->arg1;
      int bufferSize = (int)(long) args->arg2;
      int termUnit = (int)(long) args->arg3;

      if (bufferSize <= 0 || bufferSize > MAXLINE || termUnit < 0 || termUnit >= USLOSS_MAX_UNITS){
            args->arg2 = 0; 
            args->arg4 = (void*)(long)-1;
            return;
      }

      lockTerminal(termUnit);
      for (int i = 0; i < (strlen(buffer)); i++){
            MboxRecv(termWriteMBoxes[termUnit], NULL, 0);
            writeChar(termUnit, buffer[i]); 
      }
      unlockTerminal(termUnit);

      args->arg2 = strlen(buffer);
      args->arg4 = (void*)(long) 0;
}

//handles the various terminal interrupts
void handleTermInterrupt(int termUnit, int status){
      int recv = USLOSS_TERM_STAT_RECV(status);
      if (recv == USLOSS_DEV_BUSY){
            char character = USLOSS_TERM_STAT_CHAR(status);
            int len = termBufferLengths[termUnit];
            char *buff = termBuffers[termUnit];

            if (len < MAXLINE){
                  buff[len+1] = character;
                  len++;
            }
            if (character == '\n' || len == MAXLINE-1){ 
                  MboxCondSend(termReadMBoxes[termUnit], buff, len+1);
                  len = -1;
                  memset(termBuffers[termUnit], '\0', MAXLINE+1);
            }
            termBufferLengths[termUnit] = len;
      }

      int xmit = USLOSS_TERM_STAT_XMIT(status);
      if (xmit == USLOSS_DEV_READY){
            MboxCondSend(termWriteMBoxes[termUnit], NULL, 0);
      }

      if (recv == USLOSS_DEV_ERROR || xmit == USLOSS_DEV_ERROR){
            USLOSS_Console("Error in terminal %d", termUnit);
            USLOSS_Halt(1);
      }
}

//daemon for the terminal
void termDaemon(int termUnit){
      int status;
      while (1){
            waitDevice(USLOSS_TERM_DEV, termUnit, &status);
            handleTermInterrupt(termUnit, status);
      }
}

//lock for the disk
void lockDisk(int diskUnit){
      MboxSend(diskLocks[diskUnit], NULL, 0);
}

//returns the lock for the disk
void unlockDisk(int diskUnit){
      MboxRecv(diskLocks[diskUnit], NULL, 0);
}

//daemon for the disk
void diskDaemon(int diskUnit) {
    int status;

    while (1) {
        waitDevice(USLOSS_DISK_DEV, diskUnit, &status);

        if (status == USLOSS_DEV_READY) {
            DiskReq *request = dequeueDiskRequest(diskUnit);

            // Process the request if available
            if (request != NULL) {
                if (request->req.opr == USLOSS_DISK_WRITE) {
                    writeToDisk(request);
                } else if (request->req.opr == USLOSS_DISK_READ) {
                    readFromDisk(request);
                }
                free(request); // Free the request after processing
            }
        }
    }
}




void enqueueDiskRequest(int diskUnit, DiskReq *request) {
    request->next = NULL;

    // If the queue is empty, add the request directly
    if (requests[diskUnit] == NULL) {
        requests[diskUnit] = request;
        return;
    }

    DiskReq *current = requests[diskUnit];
    DiskReq *previous = NULL;

    // Find the correct position to insert based on C-SCAN
    while (current != NULL && current->startTrackNum < request->startTrackNum) {
        previous = current;
        current = current->next;
    }

    // Insert the request into the correct position
    if (previous == NULL) {
        request->next = requests[diskUnit];
        requests[diskUnit] = request;
    } else {
        request->next = current;
        previous->next = request;
    }
}


DiskReq *dequeueDiskRequest(int diskUnit) {
    DiskReq *request = requests[diskUnit];
    if (request != NULL) {
        requests[diskUnit] = request->next;
    }
    return request;
}

//finds the disk
void seekDisk(int diskUnit, int track){
      DiskState *disk = &disks[diskUnit];
      disk->req.opr = USLOSS_DISK_SEEK;
      disk->req.reg1 = track;
      USLOSS_DeviceOutput(USLOSS_DISK_DEV, diskUnit, &disk->req);
      MboxRecv(diskDaemonMBoxIDs[diskUnit], NULL, 0);
}

//reads from the disk
void readFromDisk(DiskReq *request) {
    lockDisk(request->diskUnit);

    int currentBlock = request->startBlockNum;
    int currentTrack = request->startTrackNum;
    int numSectors = request->numSectors;
    int diskUnit = request->diskUnit;
    char *buffer = request->buffer;

    DiskState *disk = &disks[diskUnit];
    seekDisk(diskUnit, currentTrack);

    for (int i = 0; i < numSectors; i++) {
        int absoluteBlock = currentBlock + i;
        int newTrack = currentTrack + (absoluteBlock / USLOSS_DISK_TRACK_SIZE);
        int newBlock = absoluteBlock % USLOSS_DISK_TRACK_SIZE;

        if (disk->currentTrack != newTrack) {
            disk->currentTrack = newTrack;
            seekDisk(diskUnit, disk->currentTrack);
        }

        disk->req.opr = USLOSS_DISK_READ;
        disk->req.reg1 = newBlock;
        disk->req.reg2 = disk->buffer;

        USLOSS_DeviceOutput(USLOSS_DISK_DEV, diskUnit, &disk->req);
        MboxRecv(diskDaemonMBoxIDs[diskUnit], NULL, 0);
        memcpy(buffer + (USLOSS_DISK_SECTOR_SIZE * i), disk->buffer, USLOSS_DISK_SECTOR_SIZE);
    }

    unlockDisk(diskUnit);
}


//handles the read syscall for the disk
void kernDiskRead(USLOSS_Sysargs *args){
      char *buffer = args->arg1;
      int numSectors = args->arg2;
      int startTrackNum = args->arg3;
      int startBlockNum = args->arg4;
      int diskUnit = args->arg5;

    int trackSize = USLOSS_DISK_TRACK_SIZE;
      if (diskUnit < 0 || diskUnit >= USLOSS_DISK_UNITS || startBlockNum >= trackSize || startBlockNum < 0) {
        args->arg1 = USLOSS_DEV_ERROR;
        args->arg4 = -1;
        return;
    }

    if (startTrackNum >= 31 || startTrackNum < 0) {
        args->arg4 = 0;
        args->arg1 = USLOSS_DEV_ERROR;
        return;
    }


      struct DiskReq *request = &requests[diskUnit];
      request->startBlockNum = startBlockNum;
      request->startTrackNum = startTrackNum;
      request->numSectors = numSectors;
      request->diskUnit = diskUnit;
      request->buffer = buffer;

      readFromDisk(request);

      args->arg1 = 0;
      args->arg4 = 0;
}

//writes to the disk
void writeToDisk(DiskReq *request) {
    lockDisk(request->diskUnit);

    int currentBlock = request->startBlockNum;
    int currentTrack = request->startTrackNum;
    int numSectors = request->numSectors;
    int diskUnit = request->diskUnit;
    char *buffer = request->buffer;

    DiskState *disk = &disks[diskUnit];
    seekDisk(diskUnit, currentTrack);

    for (int i = 0; i < numSectors; i++) {
        int absoluteBlock = currentBlock + i;
        int newTrack = currentTrack + (absoluteBlock / USLOSS_DISK_TRACK_SIZE);
        int newBlock = absoluteBlock % USLOSS_DISK_TRACK_SIZE;

        if (disk->currentTrack != newTrack) {
            disk->currentTrack = newTrack;
            seekDisk(diskUnit, disk->currentTrack);
        }

        memcpy(disk->buffer, buffer + (USLOSS_DISK_SECTOR_SIZE * i), USLOSS_DISK_SECTOR_SIZE);
        disk->req.opr = USLOSS_DISK_WRITE;
        disk->req.reg1 = newBlock;
        disk->req.reg2 = disk->buffer;

        USLOSS_DeviceOutput(USLOSS_DISK_DEV, diskUnit, &disk->req);
        MboxRecv(diskDaemonMBoxIDs[diskUnit], NULL, 0);
    }

    unlockDisk(diskUnit);
}



//handles the syscall for the disk write
void kernDiskWrite(USLOSS_Sysargs *args) {
    char *buffer = args->arg1;
    int numSectors = args->arg2;
    int startTrackNum = args->arg3;
    int startBlockNum = args->arg4;
    int diskUnit = args->arg5;

    int trackSize = USLOSS_DISK_TRACK_SIZE;
    if (diskUnit < 0 || diskUnit >= USLOSS_DISK_UNITS || startBlockNum >= trackSize || startBlockNum < 0) {
        args->arg1 = USLOSS_DEV_ERROR;
        args->arg4 = -1;
        return;
    }

    if (startTrackNum >= 31 || startTrackNum < 0) {
        args->arg4 = 0;
        args->arg1 = USLOSS_DEV_ERROR;
        return;
    }

    // Allocate memory for the new request
    DiskReq *request = malloc(sizeof(DiskReq));
    if (request == NULL) {
        USLOSS_Console("ERROR: Memory allocation failed for DiskReq\n");
        args->arg1 = USLOSS_DEV_ERROR;
        args->arg4 = -1;
        return;
    }

    // Populate the request
    request->startBlockNum = startBlockNum;
    request->startTrackNum = startTrackNum;
    request->numSectors = numSectors;
    request->diskUnit = diskUnit;
    request->buffer = buffer;
    request->next = NULL;

    // Enqueue the request
    enqueueDiskRequest(diskUnit, request);

    args->arg1 = 0;
    args->arg4 = 0;
}


//handles the syscall for the disk size
void kernDiskSize(USLOSS_Sysargs *args){
      int diskUnit = args->arg1;

      if (diskUnit > 1 || diskUnit < 0){
            args->arg1 = USLOSS_DEV_ERROR;
            args->arg4 = -1;
            return;
      }

      int numTracks;
      USLOSS_DeviceRequest req;
      req.opr = USLOSS_DISK_TRACKS;
      req.reg1 = &numTracks;
      
      USLOSS_DeviceOutput(USLOSS_DISK_DEV, diskUnit, &req);
      MboxRecv(diskDaemonMBoxIDs[diskUnit], NULL, 0);

      args->arg1 = USLOSS_DISK_SECTOR_SIZE;
      args->arg2 = USLOSS_DISK_TRACK_SIZE;
      args->arg3 = numTracks;
      args->arg4 = 0;

}

//phase 4 initilization for globals 
void phase4_init(){
      currentTicks = 0;

      systemCallVec[SYS_SLEEP] = kernSleep;
      systemCallVec[SYS_TERMREAD] = kernTermRead;
      systemCallVec[SYS_TERMWRITE] = kernTermWrite;
      systemCallVec[SYS_DISKREAD] = kernDiskRead;
      systemCallVec[SYS_DISKWRITE] = kernDiskWrite;
      systemCallVec[SYS_DISKSIZE] = kernDiskSize;

      for (int i = 0; i < 4; i++){
            terminalLocks[i] = MboxCreate(1,0);
            termReadMBoxes[i] = MboxCreate(10, MAXLINE);
            termWriteMBoxes[i] = MboxCreate(1, MAXLINE);
            termBufferLengths[i] = -1;
            memset(termBuffers[i], 0, MAXLINE+1);
      }

      for (int i = 0; i < MAXPROC ;i++){
            sleeper *cur = &sleepers[i];
            cur->pid = -1;
            cur->wakeTime = -1;
            cur->next = NULL;
      }

      for (int i = 0; i < 2; i++){
            diskLocks[i] = MboxCreate(1,0);
      }
    for (int i = 0; i < USLOSS_DISK_UNITS; i++) {
        requests[i] = NULL;
    }

      for (int i = 0; i < 2; i++){
            diskDaemonMBoxIDs[i] = MboxCreate(1,0);
            diskLocks[i] = MboxCreate(1,0);
            memset(&requests[i], 0, sizeof(requests[i]));
            memset(&disks[i], 0, sizeof(disks[i]));
      }
}

//starts the daemon processes
void phase4_start_service_processes(){
      int cr_val = 0x6; 
      USLOSS_DeviceOutput(USLOSS_TERM_DEV, 0, (void*)(long)cr_val);
      USLOSS_DeviceOutput(USLOSS_TERM_DEV, 1, (void*)(long)cr_val);
      USLOSS_DeviceOutput(USLOSS_TERM_DEV, 2, (void*)(long)cr_val);
      USLOSS_DeviceOutput(USLOSS_TERM_DEV, 3, (void*)(long)cr_val);
      
      spork("sleepDaemon", sleepDaemon, NULL, USLOSS_MIN_STACK, 1);
      spork("termDaemon0", termDaemon, 0, USLOSS_MIN_STACK, 1);
      spork("termDaemon1", termDaemon, 1, USLOSS_MIN_STACK, 1);
      spork("termDaemon2", termDaemon, 2, USLOSS_MIN_STACK, 1);
      spork("termDaemon3", termDaemon, 3, USLOSS_MIN_STACK, 1);
      spork("diskDaemon1", diskDaemon, 0, USLOSS_MIN_STACK, 1);
      spork("diskDaemon2", diskDaemon, 1, USLOSS_MIN_STACK, 1);
}