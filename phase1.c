#include <stdio.h>
#include <string.h>
#include <phase1.h>
#include <usloss.h>
#include <stdlib.h>
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
#ifndef EXTERNAL_TESTCASE_MAIN
int testcase_main(){
    return testcase_main();
}
#endif
int testcase_bounce(void*){
    return testcase_main();
}
void trampoline() {
    int curr=getpid();
    table[curr-1].startFunc(table[curr-1].arg);
}
int spork(char *name, int (*startFunc)(void*), void *arg, int stackSize, int priority) {
    if(stackSize<USLOSS_MIN_STACK){
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
            curr_pid=new->pid;
            proc *par=&table[curr_pid-1];
            new->parent=par;
            if(par->child==NULL) {
                par->child= new;
            } else {
                struct proc*old=par->child;
                struct proc*curr=old;
                while(curr->younger!=NULL) {
                    curr=curr->younger;
                }
                curr->younger=new;
                new->older=curr;
            }
            new->stack=malloc(USLOSS_MIN_STACK);
            USLOSS_ContextInit(&new->context,new->stack,USLOSS_MIN_STACK,NULL,trampoline);
            int p=new->prio-1;
            queue[p][queue_len[p]++]=new;
            return curr_pid;
        }
    }
    return -1;
}

int join(int *status){
    if(status==NULL){
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
        child=child->younger;
    }
    if(dead!=NULL) {
        *status=dead->exit_status;
        if (dead->older!=NULL) {
            dead->older->younger=dead->younger;
        } else {
            curr->child=dead->younger;
        }
        if (dead->younger!=NULL) {
            dead->younger->older=dead->older;
        }
        return dead->pid;
    }
    if(curr->child!=0){
        return -1;
    }
    return -2;
}

__attribute__((noreturn)) void quit_phase_1a(int status, int switchToPid) {
    proc *curr=&table[curr_pid-1];
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
    USLOSS_ContextSwitch(&table[curr_pid-1].context,&table[switchToPid-1].context);
}

int  getpid(void){
    return curr_pid;
}

void dumpProcesses(void){
    printf("Dump Process");
    for(int i=0; i<MAXPROC; i++){
        if(table[i].pid!=-1){
            printf("%5d %5s %5d %5d %5d\n", 
            table[i].pid, 
            table[i].name,
            table[i].prio, 
            table[i].status, 
            table[i].state);
        }
    }
}
void TEMP_switchTo(int pid){
    curr_pid=pid;
    curr_pcb=&table[pid-1];
}
void phase1_init(void) {
    memset(table,0,sizeof(table));
    memset(queue_len,0,sizeof(table));
    for(int i=0; i<=MAXPROC; i++){
        table[i].pid=0;
        table[i].status=0;
    }
    curr_pid=1;
    strcpy(table[1].name, "init");
    table[1].pid=1;
    table[1].status=0;  
    table[1].prio=6;
    table[1].state=RUNNING;
    curr_pcb=&table[1];
    int testcase=spork("testcase_main",testcase_bounce, NULL, USLOSS_MIN_STACK, 3);
    USLOSS_ContextSwitch(&table[1].context,&table[testcase-1].context);
    printf("testcase_main returned...haulting");
    USLOSS_Halt(0);
    int i=2;
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
