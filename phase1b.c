/*
 * These are the definitions for phase1 of the project (the kernel).
 */

#ifndef _PHASE1_H
#define _PHASE1_H
#include <stdio.h>
#include <string.h>
#include <phase1.h>
#include <usloss.h>
#include <stdlib.h>
#include <time.h>

/*
 * Maximum number of processes. 
 */

#define MAXPROC      50

/*
 * Maximum length of a process name
 */

#define MAXNAME      50

/*
 * Maximum length of string argument passed to a newly created process
 */

#define MAXARG       100

/*
 * Maximum number of syscalls.
 */

#define MAXSYSCALLS  50
#define DEAD -2
#define BLOCKED -1
#define READY 0
#define RUNNING 1

/* 
 * These functions must be provided by Phase 1.
 */
typedef struct proc{
    struct proc *older;
    struct proc *younger; 
    struct proc *child;   
    struct proc *parent;
    USLOSS_Context context;
    void *stack;
    int pid;
    int prio;
    int status;
    int state;
    char name[32];
    int (*startFunc)(void*);
    void *arg; 
    int exit_status;
} proc;
struct proc table[MAXPROC];
struct proc *queue[6][MAXPROC];
int queue_len[6];
int curr_pid=-1;
struct proc *curr_pcb=NULL;
// #ifndef EXTERNAL_TESTCASE_MAIN
// int testcase_main(){
//     return testcase_main();
// }
// #endif
int testcase_bounce(void*){
    return testcase_main();
}
void trampoline() {
    int curr=getpid();
    for(int i = curr; (i<=MAXPROC && table[i].pid!=0); i++){
        curr = table[i].pid;
    }
    int psr = USLOSS_PsrGet();
    if ((psr & USLOSS_PSR_CURRENT_INT)==0) {
        USLOSS_PsrSet(psr | USLOSS_PSR_CURRENT_INT);
    }
    table[curr-1].startFunc(table[curr-1].arg);
    quit(table[curr_pid - 1]->status);
}
void dispatcher(void) {
    int end=0;
    for (int j=0;j<6;j++) {
        for (int i=0;i<queue_len[p];i++) {
            if (queue[j][i]->state==0) {
                TEMP_switchTo(queue[j][i]->pid);
                end=1;
                break;
            }
        }
        if (end==1){
            break;
        }
    }
    if (end==0) {
        USLOSS_Console("Error\n");
        USLOSS_Halt(1);
    }
}
int currentTime(void) {
    time_t curr=time(NULL);
    return (int)curr;
}
int spork(char *name, int (*startFunc)(void*), void *arg, int stackSize, int priority) {
    int psr=USLOSS_PsrGet();
    if (!(psr & USLOSS_PSR_CURRENT_MODE)) {
        USLOSS_Console("ERROR: Someone attempted to call spork while in user mode!\n");
        USLOSS_Halt(1);
    }
    if (!(psr & USLOSS_PSR_CURRENT_INT)) {
        USLOSS_Console("ERROR: It looks like you are running testcase_main() with interrupts disabled, instead of enabled. Fix that!\n");
        USLOSS_Halt(1);
    }
    if(stackSize<USLOSS_MIN_STACK){
        USLOSS_PsrSet(psr);
        return -2;
    }
    for(int i=1; i<=MAXPROC; i++){
        if(table[i].pid==0){
            struct proc *new=&table[i];
            new->pid=i+1;
            new->prio=priority;
            new->status=0;
            new->startFunc=startFunc;
            new->arg=arg;
            strcpy(new->name,name);
            proc *par=curr_pcb;
            new->parent=par;
            new->child=NULL;
            new->older=par->child;
            if (par->child!=NULL) par->child->younger=new;
            par->child=new;
            new->younger=NULL;
            new->stack=malloc(USLOSS_MIN_STACK);
            USLOSS_ContextInit(&new->context,new->stack,USLOSS_MIN_STACK,NULL,trampoline);
            int p=new->prio-1;
            queue[p][queue_len[p]++]=new;
            if (new->prio<curr_pcb->prio) {
                dispatcher();
            }
            USLOSS_PsrSet(psr);
            return new->pid;
        }
    }
    USLOSS_PsrSet(psr);
    return -1;
}

int join(int *status){
    //USLOSS_Console("Join Name: %s, PID: %d\n", &table[curr_pid-1].name, table[curr_pid-1].pid);
     int psr=USLOSS_PsrGet();
    if ((psr & USLOSS_PSR_CURRENT_INT)!=0) {
        USLOSS_PsrSet(psr & ~USLOSS_PSR_CURRENT_INT);
    }
    if(status==NULL){
        USLOSS_PsrSet(psr);
        return -3;
    }
    proc *curr=&table[curr_pid-1];
    proc *dead=NULL;
    proc *child = curr->child;
    while(child!=NULL) {
        if(child->state==-2) {
            dead=child;
            break;
        }
        child=child->older;
    }
    if(dead!=NULL) {
        *status=dead->exit_status;
        if (dead->younger!=NULL) {
            dead->younger->older=dead->older;
        } else {
            curr->child=dead->older;
        }
        if (dead->older!=NULL) {
            dead->older->younger=dead->younger;
        }
        free(dead->stack);
        int id = dead->pid;
        memset(&table[dead->pid - 1], 0, sizeof(proc));
        USLOSS_PsrSet(psr);
        return id;
    }
    if(curr->child==NULL){
        USLOSS_PsrSet(psr);
        return -2;
    }
    curr->state=-2;
    dispatcher();
    child=curr->child;
    while (child!=NULL) {
        if (child->state==DEAD) {
            dead=child;
            break;
        }
        child=child->older;
    }
    USLOSS_PsrSet(psr);
    return -1;
}

__attribute__((noreturn)) void quit(int status) {
    int psr=USLOSS_PsrGet();
    if (curr_pid<1 || curr_pid>MAXPROC || table[curr_pid-1].pid==0) {
        USLOSS_Console("Error: Invalid process attempting to quit (PID: %d).\n",curr_pid);
        USLOSS_Halt(1);
    }
    proc *curr=&table[curr_pid-1];
    if (!(psr & USLOSS_PSR_CURRENT_MODE)) {
        USLOSS_Console("ERROR: Someone attempted to call quit_phase_1a while in user mode!\n");
        USLOSS_Halt(1);
    }
    if (curr->state==DEAD) {
        USLOSS_Console("Error: Process %d is already terminated.\n",curr->pid);
        USLOSS_Halt(1);
    }
    if (curr->child!=NULL) {
        USLOSS_Console("ERROR: Process pid %d called quit() while it still had children.\n",curr->pid);
        USLOSS_Halt(1);
    }
    if ((psr & USLOSS_PSR_CURRENT_INT)!=0) {
        USLOSS_PsrSet(psr & ~USLOSS_PSR_CURRENT_INT);
    }
    if (curr->child != NULL) {
        proc *child = curr->child;
        while (child != NULL) {
            if (child->state != DEAD) {
                USLOSS_Console("Error: Process %d tried to quit without joining all of its children.\n", curr->pid);
                USLOSS_Halt(1);
            }
            child = child->older;
        }
    }
    curr->exit_status=status;
    curr->state=-2;
    int p=curr->prio-1;
    for(int i=0;i<queue_len[p];i++) {
        if(queue[p][i]==curr) {
            for (int j=i;j<queue_len[p]-1;j++) {
                queue[p][j]=queue[p][j+1];
            }
            queue_len[p]--;
            break;
        }
    }   
    if (curr->parent!=NULL && curr->parent->state==-1) {
        curr->parent->state=0;
        int parent_prio=curr->parent->prio-1;
        queue[parent_prio][queue_len[parent_prio]++]=curr->parent;
    }
    for (int i=0;i<MAXPROC;i++) {
        if (table[i].state==BLOCKED && table[i].status==-2) {
            table[i].state=READY;
            int zapper_prio=table[i].prio-1;
            queue[zapper_prio][queue_len[zapper_prio]++]=&table[i];
        }
    }
    dispatcher();
}
void zap(int pid) {
    if (pid==curr_pid || pid==1 || table[pid-1].pid==0 || table[pid-1].state==DEAD) {
        USLOSS_Console("Error: Cannot zap PID %d.\n", pid);
        USLOSS_Halt(1);
    }
    curr_pcb->state=BLOCKED;
    struct proc *target=&table[pid-1];
    while (target->state!=DEAD) {
        dispatcher();
    }
    curr_pcb->state=READY;
}
int  getpid(void){
    return curr_pid;
}
void blockMe(void) {
    table[curr_pid - 1].state=BLOCKED;
    dispatcher();
}
int unblockProc(int pid) {
    if (pid<1 || pid>MAXPROC || table[pid-1].state!=BLOCKED || table[pid-1].status<=10) {
        return -2;
    }
    table[pid-1].state = READY;
    int prio=table[pid-1].prio-1;
    queue[prio][queue_len[prio]++]=&table[pid-1];
    dispatcher();
    return 0;
}
void dumpProcesses() {
    USLOSS_Console("**************** Calling dumpProcesses() *******************\n");
    USLOSS_Console("- PID  PPID  NAME              PRIORITY  STATE\n");
    for (int i=0; i<MAXPROC;i++) {
        if (table[i].pid==0) continue;
        char *str;
        switch (table[i].state) {
            case DEAD:
                str="Terminated";
                break;
            case BLOCKED:
                str="Blocked";
                break;
            case READY:
                str="Runnable";
                break;
            case RUNNING:
                str="Running";
                break;
            default:
                str="SHOULDN'T BE HERE";
        }
        USLOSS_Console("- %3d  %4d  %-15s  %8d  %s\n", 
            table[i].pid, 
            table[i].parent!=NULL ? table[i].parent->pid:0,
            table[i].name, 
            table[i].prio, 
            str);
    }
}
void TEMP_switchTo(int pid){
    //USLOSS_Console("Name: %s, PID: %d\n", &table[curr_pid-1].name, table[curr_pid-1].pid);
    //USLOSS_Console("Name: %s, PID: %d\n", &table[pid-1].name, table[pid-1].pid);
    int psr=USLOSS_PsrGet();
    if ((psr & USLOSS_PSR_CURRENT_INT)!=0) {
        USLOSS_PsrSet(psr & ~USLOSS_PSR_CURRENT_INT);
    }
    int old = curr_pid;
    curr_pid=pid;
    curr_pcb=&table[pid-1];
    USLOSS_ContextSwitch(&table[old-1].context,&table[pid-1].context);
    USLOSS_PsrSet(psr | USLOSS_PSR_CURRENT_INT);
}
void phase1_init(void) {
    memset(table,0,sizeof(table));
    memset(queue_len,0,sizeof(queue_len));
    for(int i=0; i<=MAXPROC; i++){
        memset(&table[i],0,sizeof(proc));
    }
    curr_pid=1;
    strcpy(table[0].name, "init");
    table[0].pid = 1;
    table[0].status=0;  
    table[0].prio=6;    
    table[0].state=RUNNING;
    curr_pcb=&table[0];
    table[0].stack=malloc(USLOSS_MIN_STACK);
    table[0].parent=NULL;
    table[0].child=NULL;
    table[0].older=NULL;
    table[0].younger=NULL;
    USLOSS_ContextInit(table[0].context,table[0].stack, USLOSS_MIN_STACK, NULL, trampoline);
    int priority=table[0]->prio-1;
    queue[priority][queue_len[priority]++]=table[0];
    USLOSS_Console("Phase 1A TEMPORARY HACK: init() manually switching to PID 1.\n");
    TEMP_switchTo(1);
    phase2_start_service_processes();
    phase3_start_service_processes();
    phase4_start_service_processes();
    phase5_start_service_processes();
    int psr = USLOSS_PsrGet();
    psr |= USLOSS_PSR_CURRENT_INT;
    int rc = USLOSS_PsrSet(psr);
    if (rc != USLOSS_ERR_OK) {
        USLOSS_Console("ERROR: Failed to enable interrupts!\n");
        USLOSS_Halt(1);
    }
    int testcase=spork("testcase_main",testcase_bounce, NULL, USLOSS_MIN_STACK, 3);
    USLOSS_Console("Phase 1A TEMPORARY HACK: init() manually switching to testcase_main() after using spork() to create it.\n");
    TEMP_switchTo(testcase);
    USLOSS_Console("Phase 1A TEMPORARY HACK: testcase_main() returned, simulation will now halt.\n");
    //printf("testcase_main returned...haulting");
    USLOSS_Halt(0);
    int i=1;
    while(1){
        int join_val=join(&table[i].status);
        if(join_val==-2){
            printf("join_val=-2");
            USLOSS_Halt(0);
            break;
        }
        i++;
    }
}
