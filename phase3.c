
#include "usloss.h"
#include "phase3_usermode.h"
#include "phase3.h"
#include "phase2.h"
#include "phase1.h"
#include "stddef.h"

typedef struct PCB{
    int pid;
    int (*userFunc)(char*);
    char* userArgs;
} PCB;

typedef struct semaphore{
    int semID;
    int count;
    int MboxID;
    int blockedProcs;
} semaphore;

PCB table[MAXPROC];
semaphore sems[MAXSEMS];
int MboxLockID;
int numSems = 0;

int MODE_BIT = 0x1;
int INTERRUPT_BIT = 0x2;

void lock(){
    MboxSend(MboxLockID, NULL, 0);    
}

void unlock(){
    MboxRecv(MboxLockID, NULL, 0);

}

void checkPSR(char *func){
    int val = USLOSS_PsrGet();
    int modeRead = (val & MODE_BIT); 
    if (modeRead != 0x1){
        USLOSS_Console("ERROR: Someone attempted to call %s while in user mode!\n", func);
        USLOSS_Halt(1);
    }    
}

void switchToUserMode(){
    int val = USLOSS_PsrGet();
    int modeRead = (val & MODE_BIT);
    if (modeRead == 0x1){
        int retVal = USLOSS_PsrSet(val & ~MODE_BIT);
        if (retVal != USLOSS_DEV_OK){
            USLOSS_Console("PSR Set: Setting to user mode unsuccesful!\n");
            USLOSS_Halt(1);
        }
    }
}

void userTrampoline(){
    lock();

    int pid = getpid(); 
    PCB proc = table[pid%MAXPROC];
    int (*userFunc)(char*) = proc.userFunc;
    char *userArgs = (char*)proc.userArgs;
    unlock();
    switchToUserMode();
    int returnVal = userFunc(userArgs); // calling user func
    Terminate(returnVal);


}


void Spawn(USLOSS_Sysargs *sysargs){
    lock();
    char *name = (char *)sysargs->arg5;
    int (*func)(char *) = sysargs->arg1;
    char *arg = (char *)sysargs->arg2;
    long stackSize = (long)sysargs->arg3;
    long priority = (long)sysargs->arg4;
    int pid = spork(name, userTrampoline, arg, stackSize, priority); 

    PCB *proc = &table[pid%MAXPROC];
    proc->pid = pid;
    proc->userFunc = func;
    proc->userArgs = arg;  

    unlock();  

    if(pid > 0){
        sysargs->arg1 = pid;
        sysargs->arg4 = 0;
    }else{
        sysargs->arg1 = -1;
        sysargs->arg4 = -1; 
    }


}

void Wait(USLOSS_Sysargs *sysargs){
    int retVal = join(&sysargs->arg2);
    if (retVal == -2){ 
        sysargs->arg4 = -2;
    }
    else{
        sysargs->arg4 = 0;
        sysargs->arg1 = retVal;
    }
}

void Terminate(USLOSS_Sysargs *sysargs){
    long status = (long) sysargs->arg1;
    int intStatus = (int) status;
    int retVal = join(&intStatus);
    while (retVal != -2 ){
        retVal = join(&intStatus);
    }
    quit(status);
}

int findAvailableSemID(){
    for(int i = 0; i < MAXSEMS; i++){
        if (sems[i].semID == -1){
            return i;
        }
    }
    return -1;
}

void SemCreate(USLOSS_Sysargs *sysargs){
    long initVal = sysargs->arg1;
    if ((initVal < 0) || (numSems >= MAXSEMS)){ 
        sysargs->arg4 = -1;
    }
    else{
        numSems++;
        int MboxID = MboxCreate(1,0); 
        int SemID = findAvailableSemID();

        semaphore *sem = &sems[SemID];
        sem->count = sysargs->arg1;
        sem->MboxID = MboxID;
        sem->semID = SemID;

        sysargs->arg4 = 0;
        sysargs->arg1 = SemID;
    }
}

int isValidSemID(int SemID){
    if (SemID < 0){
        return 0;
    }
    if (sems[SemID].semID == -1){
        return 0;
    }
    return 1;

}

void SemP(USLOSS_Sysargs *sysargs){
    long SemID = sysargs->arg1;
    if (!isValidSemID(SemID)){
        sysargs->arg4 = -1;
    }
    else{
        semaphore *sem = &sems[SemID];
        lock();
        if (sem->count == 0){ 
            sem->blockedProcs++;
            unlock();
            MboxRecv(sem->MboxID, NULL, 0);
            lock();
        }
        sem->count--;

        sysargs->arg4 = 0;
        unlock();
    }
}

void SemV(USLOSS_Sysargs *sysargs){
    long SemID = sysargs->arg1;
    if (!isValidSemID(SemID)){
        sysargs->arg4 = -1;
    }
    else{
        semaphore *sem = &sems[SemID];
        lock();
        if (sem->blockedProcs > 0){
            sem->count++;
            sem->blockedProcs--;
            unlock();
            MboxSend(sem->MboxID, NULL, 0);
            lock();

        }
        else{
            sem->count++;
        }
        sysargs->arg4 = 0;
        unlock();
    }
}

void GetTimeofDay(USLOSS_Sysargs *sysargs){
    sysargs->arg1 = currentTime();
}

void GetPID(USLOSS_Sysargs *sysargs){
    sysargs->arg1 = getpid();
}


void phase3_init(){
    for(int i = 0; i < MAXSEMS; i++){
        semaphore *sem = &sems[i];
        sem->semID = -1;
        sem->blockedProcs = 0;
    }

    MboxLockID = MboxCreate(1,0); 

    // setting SYSCALL opcodes
    systemCallVec[SYS_SPAWN] = Spawn;
    systemCallVec[SYS_WAIT] = Wait;
    systemCallVec[SYS_TERMINATE] = Terminate;
    systemCallVec[SYS_SEMCREATE] = SemCreate;
    systemCallVec[SYS_SEMP] = SemP;
    systemCallVec[SYS_SEMV] = SemV;
    systemCallVec[SYS_GETTIMEOFDAY] = GetTimeofDay;
    systemCallVec[SYS_GETPID] = GetPID;
}

void phase3_start_service_processes(){
    
}