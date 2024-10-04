/*
 * These are the definitions for phase1 of the project (the kernel).
 */
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
    int time;
    struct proc *zapped[MAXPROC];
    int zap_count;
} proc;

struct proc table[MAXPROC];
struct proc *queue[5][MAXPROC];
int queue_len[5];
int curr_pid=-1;
struct proc *curr_pcb=NULL;

int testcase_bounce(void*){
    return testcase_main();
}

void trampoline(){
    int psr = USLOSS_PsrGet();
    if ((psr & USLOSS_PSR_CURRENT_INT)==0) {
        USLOSS_PsrSet(psr | USLOSS_PSR_CURRENT_INT);
    }
    table[curr_pid-1].startFunc(table[curr_pid-1].arg);
    quit(table[curr_pid-1].status);
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
    if ((psr & USLOSS_PSR_CURRENT_INT)!=0) {
        USLOSS_PsrSet(psr & ~USLOSS_PSR_CURRENT_INT);
    }
    if(stackSize<USLOSS_MIN_STACK){
        USLOSS_PsrSet(psr);
        return -2;
    }
    if (priority<1 || priority>5) {
        USLOSS_PsrSet(psr);
        return -1;
    }
    for(int i=1; i<MAXPROC; i++){
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
            new->time=0;
            new->state=0;
            int p=new->prio-1;
            queue[p][queue_len[p]++]=new;
            if (new->prio<table[curr_pid-1].prio) {
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
        int id=dead->pid;
        memset(&table[dead->pid - 1], 0, sizeof(proc));
        USLOSS_PsrSet(psr);
        return id;
    }
    if(curr->child==NULL){
        USLOSS_PsrSet(psr);
        return -2;
    }
    curr->state=-1;
    dispatcher();
    child=curr->child;
    while (child!=NULL) {
        if (child->state==DEAD) {
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
        int id=dead->pid;
        memset(&table[dead->pid - 1], 0, sizeof(proc));
        USLOSS_PsrSet(psr);
        return id;
    }
    USLOSS_PsrSet(psr);
    return -1;
}

__attribute__((noreturn)) void quit(int status) {
    int psr=USLOSS_PsrGet();
    proc *curr=&table[curr_pid-1];
    if (!(psr & USLOSS_PSR_CURRENT_MODE)) {
        USLOSS_Console("ERROR: Someone attempted to call quit while in user mode!\n");
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
        int p_prio=curr->parent->prio - 1;
        queue[p_prio][queue_len[p_prio]++]=curr->parent;
    }
    for (int i=0;i<MAXPROC;i++) {
        if (table[i].state==-1 && table[i].status==-2) {
            table[i].state=0;
            int zapper_prio=table[i].prio-1;
            queue[zapper_prio][queue_len[zapper_prio]++]=&table[i];
        }
    }
    for (int i = 0; i < curr->zap_count; i++) {
        table[curr->zapped[i]->pid-1].state = READY;
    }
    USLOSS_PsrSet(psr);
    dispatcher();
}

void zap(int pid) {
    if (pid<=0 || pid>MAXPROC || table[pid-1].pid==0) {
        USLOSS_Console("ERROR: Attempt to zap() a non-existent process.\n");
        USLOSS_Halt(1);
    }
    if (pid==curr_pid){
        USLOSS_Console("ERROR: Attempt to zap() itself.\n");
        USLOSS_Halt(1);
    }
    if(table[pid-1].state==DEAD){
        USLOSS_Console("ERROR:  Attempt to zap() a process that is already in the process of dying.\n");
        USLOSS_Halt(1);
    }
    if(pid==1) {
        USLOSS_Console("ERROR: Attempt to zap() init.\n");
        USLOSS_Halt(1);
    }

    struct proc *target=&table[pid-1];
    target->zapped[target->zap_count++]=&table[curr_pid-1];
    curr_pcb->state=BLOCKED;
    while (target->state!=DEAD) {
        dispatcher();
    }
    curr_pcb->state=READY;
}

int  getpid(void){
    return curr_pid;
}

void blockMe(void) {
    int psr=USLOSS_PsrGet();
    if ((psr & USLOSS_PSR_CURRENT_INT)!=0) {
        USLOSS_PsrSet(psr & ~USLOSS_PSR_CURRENT_INT);
    }
    table[curr_pid-1].state=BLOCKED;
    dispatcher();
    USLOSS_PsrSet(psr);
}

int unblockProc(int pid) {
    int psr=USLOSS_PsrGet();
    if ((psr & USLOSS_PSR_CURRENT_INT)!=0) {
        USLOSS_PsrSet(psr & ~USLOSS_PSR_CURRENT_INT);
    }
    if (pid<1 || pid>MAXPROC || table[pid-1].state!=BLOCKED || table[pid-1].status<=10) {
        return -2;
    }
    table[pid-1].state=0;
    int prio=table[pid-1].prio-1;
    queue[prio][queue_len[prio]++]=&table[pid-1];
    dispatcher();
    USLOSS_PsrSet(psr);
    return 0;
}

void dumpProcesses() {
    USLOSS_Console(" PID  PPID  NAME              PRIORITY  STATE\n");
    for (int i=0; i<MAXPROC;i++) {
        if (table[i].pid==0) continue;
        char *str;
        int num=table[i].parent!=NULL ? table[i].parent->pid:0;
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
        USLOSS_Console(" %3d  %4d  %-15s  %8d  %s\n", 
            table[i].pid, 
            table[i].parent!=NULL ? table[i].parent->pid:0,
            table[i].name, 
            table[i].prio, 
            str);
    }
}

void swapTo(int pid){
    //USLOSS_Console("Name: %s, PID: %d\n", &table[curr_pid-1].name, table[curr_pid-1].pid);
    //USLOSS_Console("Name: %s, PID: %d\n", &table[pid-1].name, table[pid-1].pid);
    int psr=USLOSS_PsrGet();
    if ((psr & USLOSS_PSR_CURRENT_INT)!=0) {
        USLOSS_PsrSet(psr & ~USLOSS_PSR_CURRENT_INT);
    }
    if(curr_pid!=-1 && table[curr_pid - 1].state!=-2) {
        table[curr_pid-1].state=0;
        table[curr_pid-1].time=0;
    }
    int old=curr_pid;
    curr_pid=pid;
    curr_pcb=&table[pid-1];
    curr_pcb->state=1;
    USLOSS_ContextSwitch(&table[old-1].context,&table[pid-1].context);
    USLOSS_PsrSet(psr | USLOSS_PSR_CURRENT_INT);
}

void dispatcher(void) {
    int end=0;
    for (int j=0;j<5;j++) {
        if (queue_len[j]>0) {
            struct proc *curr=queue[j][0];
            if (curr_pid!=-1 && table[curr_pid-1].prio-1==j && table[curr_pid-1].time>=80) {
                table[curr_pid-1].time=0;
                struct proc *temp=queue[j][0];
                for(int i=0;i<queue_len[j]-1;i++) { 
                    queue[j][i]=queue[j][i+1];
                }
                queue[j][queue_len[j]-1]=temp;
                curr=queue[j][0];
            }
            while(curr!=NULL && curr->state!=READY) {
                struct proc *blocked_proc=queue[j][0];
                for (int i=0;i<queue_len[j]-1;i++) {
                    queue[j][i]=queue[j][i+1];
                }
                queue[j][queue_len[j]-1]=blocked_proc;
                curr=queue[j][0];
                if (curr==blocked_proc) {
                    curr=NULL;
                    break;
                }
            }
            if (curr!=NULL && curr->state==READY) {
                swapTo(curr->pid);
                end=1;
                break; 
            }
        }
    }
    if (end==0) {
        USLOSS_Halt(1);
    }
}

void phase1_init(void) {
    memset(table,0,sizeof(table));
    memset(queue_len,0,sizeof(queue_len));
    for(int i=0; i<MAXPROC; i++){
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
    USLOSS_ContextInit(&table[0].context,table[0].stack, USLOSS_MIN_STACK, NULL, trampoline);
    swapTo(1);
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
    int i=1;
    int status;
    while(1){
        int join_val=join(&status);
        if(join_val==-2){
            USLOSS_Halt(0);
            break;
        }
        i++;
    }
}
