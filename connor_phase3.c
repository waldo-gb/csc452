#include "phase3.h"
#include "usloss.h"
#include "usyscall.h"
#include <stdio.h>
#include <stdlib.h>

#define MAXSEMS         200
#define MAX_PROCESSES   100
#define MAX_SYSCALLS    50

extern void (*systemCallVec[MAX_SYSCALLS])(USLOSS_Sysargs *args);

typedef struct Semaphore{
    int value;
    int is_valid;
    int mutex;
    int cond;
} Semaphore;

Semaphore semaphore_table[MAXSEMS];

void bounce(void *arg);
void syscall_spawn_handler(USLOSS_Sysargs *args);
void syscall_wait_handler(USLOSS_Sysargs *args);
void syscall_terminate_handler(USLOSS_Sysargs *args);
void syscall_semcreate_handler(USLOSS_Sysargs *args);
void syscall_semp_handler(USLOSS_Sysargs *args);
void syscall_semv_handler(USLOSS_Sysargs *args);
void syscall_gettimeofday_handler(USLOSS_Sysargs *args);
void syscall_getpid_handler(USLOSS_Sysargs *args);

void phase3_start_service_processes() {
    
}

void phase3_init(void) {
    systemCallVec[SYS_SPAWN]=syscall_spawn_handler;
    systemCallVec[SYS_WAIT]=syscall_wait_handler;
    systemCallVec[SYS_TERMINATE]=syscall_terminate_handler;
    systemCallVec[SYS_SEMCREATE]=syscall_semcreate_handler;
    systemCallVec[SYS_SEMP]=syscall_semp_handler;
    systemCallVec[SYS_SEMV]=syscall_semv_handler;
    systemCallVec[SYS_GETTIMEOFDAY]=syscall_gettimeofday_handler;
    systemCallVec[SYS_GETPID]=syscall_getpid_handler;
}


void syscall_spawn_handler(USLOSS_Sysargs *args) {
    char *name=(char *) args->arg5;
    int (*func)(void *)=(int (*)(void *)) args->arg1;
    void *arg=args->arg2;
    int stack_size=(int) args->arg3;
    int priority=(int) args->arg4;
    int pid;
    int status=kernel_Spawn(name, func, arg, stack_size, priority, &pid);
    args->arg1=(void *) (long) pid;
    int result;
    if (status<0) {
        result = -1;
    } else {
        result = 0;
    }
    args->arg4 = (void *) (long) result;
}

void syscall_wait_handler(USLOSS_Sysargs *args) {
    int pid, status;
    int result=kernel_Wait(&pid, &status);

    if (result==-2) {
        args->arg1=(void *)(long) -1;
        args->arg4=(void *)(long) -2;
    } else if (result==-3) {
        args->arg1=(void *)(long) -1;
        args->arg4=(void *)(long) -1;
    } else {
        args->arg1=(void *)(long) pid;
        args->arg2=(void *)(long) status;
        args->arg4=(void *)(long) 0;
    }
}

void syscall_terminate_handler(USLOSS_Sysargs *args) {
    int status=(int) args->arg1;
    kernel_Terminate(status);
}

void syscall_semcreate_handler(USLOSS_Sysargs *args) {
    int initial_value=(int) args->arg1;
    int sem_id;
    int status=kernel_SemCreate(initial_value, &sem_id);
    args->arg1=(void *) (long) sem_id;
    int result;
    if (status < 0) {
        result = -1;
    } else {
        result = 0;
    }
    args->arg4 = (void *) (long) result;
}

void syscall_semp_handler(USLOSS_Sysargs *args) {
    int sem_id=(int) args->arg1;
    int status=kernel_SemP(sem_id);
    int result;
    if (status < 0) {
        result = -1;
    } else {
        result = 0;
    }
    args->arg4 = (void *) (long) result;
    }

void syscall_semv_handler(USLOSS_Sysargs *args) {
    int sem_id=(int) args->arg1;
    int status=kernel_SemV(sem_id);
    int result;
    if (status < 0) {
        result = -1;
    } else {
        result = 0;
    }
    args->arg4 = (void *) (long) result;
}

void syscall_gettimeofday_handler(USLOSS_Sysargs *args) {
    int tod;
    kernel_GetTimeofDay(&tod);
    args->arg1=(void *) (long) tod;
}

void syscall_getpid_handler(USLOSS_Sysargs *args) {
    int pid=kernel_GetPID();
    args->arg1=(void *) (long) pid;
}

typedef struct trampoline{
    int (*func)(void *);
    void *arg;
} trampoline;

int kernel_Spawn(char *name, int (*func)(void *), void *arg, int stack_size, int priority, int *pid) {
    if (name==NULL || func==NULL || stack_size<USLOSS_MIN_STACK || priority<1 || priority>5) {
        return -1;
    }
    trampoline *tr=malloc(sizeof(trampoline));
    if (tr==NULL) {
        return -1;
    }
    tr->func=func;
    tr->arg=arg;
    int kidPid=spork(name, bounce, tr, stack_size, priority);
    if (kidPid<0) {
        free(tr);
        return -1;
    }
    *pid=kidPid;
    return 0;
}
void bounce(void *arg) {
    trampoline *trampo=(trampoline *) arg;
    int psr=USLOSS_PsrGet();
    int status=USLOSS_PsrSet(psr & ~USLOSS_PSR_CURRENT_MODE);
    if (status != USLOSS_DEV_OK) {
        USLOSS_Console("launch(): Failed to switch to user mode\n");
        USLOSS_Halt(1);
    }
    int ret=trampo->func(trampo->arg);
    free(trampo);
    Terminate(ret);
}

int kernel_Wait(int *pid, int *status) {
    if (status==NULL) {
        return -3;
    }
    int child_pid=join(status);
    if (child_pid==-2) {
        return -2;
    }
    if (child_pid==-3) {
        return -1;
    }
    *pid=child_pid;
    return 0;
}

void kernel_Terminate(int status) {
    int child_status;
    while (join(&child_status) != -2);
    quit(status);
}

int kernel_SemCreate(int value, int *semaphore) {
    if (value<0) return -1;
    int slot=-1;
    for (int i=0; i<MAXSEMS; i++) {
        if (!semaphore_table[i].is_valid){
            slot=i;
            break;
        }
    }
    if (slot==-1) {
        *semaphore=0;
        return -1;
    }
    semaphore_table[slot].value=value;
    semaphore_table[slot].is_valid=1;
    semaphore_table[slot].mutex=MboxCreate(1, 0);
    if (semaphore_table[slot].mutex<0) {
        semaphore_table[slot].is_valid=0;
        return -1;
    }
    MboxSend(semaphore_table[slot].mutex, NULL, 0);
    semaphore_table[slot].cond=MboxCreate(0, 0);
    if (semaphore_table[slot].cond<0) {
        MboxRelease(semaphore_table[slot].mutex);
        semaphore_table[slot].is_valid=0;
        return -1;
    }
    *semaphore=slot;
    return 0;
}

int kernel_SemP(int semaphore) {
    if (semaphore<0 || semaphore>=MAXSEMS || !semaphore_table[semaphore].is_valid)
        return -1;
    MboxRecv(semaphore_table[semaphore].mutex, NULL, 0);
    semaphore_table[semaphore].value--;
    if (semaphore_table[semaphore].value<0) {
        MboxSend(semaphore_table[semaphore].mutex, NULL, 0);
        int result=MboxRecv(semaphore_table[semaphore].cond, NULL, 0);
        if (result<0) {
            return -1;
        }
    } else {
        MboxSend(semaphore_table[semaphore].mutex, NULL, 0);
    }
    return 0;
}

int kernel_SemV(int semaphore) {
    if (semaphore<0 || semaphore>=MAXSEMS || !semaphore_table[semaphore].is_valid)
        return -1;
    MboxRecv(semaphore_table[semaphore].mutex, NULL, 0);
    semaphore_table[semaphore].value++;
    if (semaphore_table[semaphore].value<=0) {
        MboxSend(semaphore_table[semaphore].cond, NULL, 0);
    }
    MboxSend(semaphore_table[semaphore].mutex, NULL, 0);
    return 0;
}

void kernel_GetTimeofDay(int *tod) {
    *tod=currentTime();
}

int kernel_GetPID() {
    return getpid();
}
