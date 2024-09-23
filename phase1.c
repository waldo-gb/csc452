#ifndef _PHASE1_H
#define _PHASE1_H

#include <stdio.h>
#include <string.h>
#include <phase1.h>
#include <usloss.h>

list of dead and alive childern
child status get collected by parent in join
need 6 different run queues
need a pointer or  something for the table
a process has a list of people waiting for it to die
write init as if there are other phases running

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
struct proc{
    proc *older;
    proc *younger; 
    proc *child;   
    proc *parent;
    proc dead_children[MAXPROC];
    int dead_count;
    proc alive_children[MAXPROC];
    int alive_count;
    USLOSS_Context context;
    int pid;
    int prio;
    int status;
    int state;
    int run_status;
    char name[32];
    int (*startFunc)(char*);
    char *arg; 
    int pid_parent;
    int pid_child;
    int exit_status;
} proc;
USLOSS_ContextInit(&table[i].context)
table[1].pid=1;
table[1].......
proc table[MAXPROC];
proc *queue[6][MAXPROC];
int queue_len[6];
int curr_pid=-1;
proc *curr_pcb=NULL;
***CONSTRUCTOR***kinda
void phase1_init(void) {
    memset(table,0,sizeof(table));
    memset(queue_len,0,sizeof(table))
    int val;
    for(int i=0; i<=MAXPROC; i++){
        table[i].pid=0;
        table[i].status=0;
    }
    curr_pid=1;
    strcpy(table[1].name, "init");
    table[1].pid=1;
    table[1].status=0;
    table[1].prio=6;
    table[1].pid_parent=0;
    table[1].state=RUNNING;
    curr_pcd=&table[1];
    int testcase=spork("testcase_main",testcas_main, NULL, USLOSS_MIN_STACK, 3);
    ULOSS_ContextSwitch(testcase);
    printf("testcase_main returned...haulting");
    USLOSS_HAULT(0);
    while(1){
        int join_val=join(&status);
        if(join_val==-2){
            printf("testcase_main returned...haulting");
            USLOSS_HAULT(0);
            break;
        }
    }
}
int testcase_main(void){
    return testcase_main();
}
int spork(char *name, int (*startFunc)(char*), char *arg, int stackSize, int priority) {
    if(stackSize<USLOSS_MIN_STACK){
        return -2
    }
    for(int i=1; i<=MAXPROC; i++){
        if(table[i].pid==0){
            proc *new=&table[i];
            new->pid=i+1;
            new->prio=priority;
            new->status=0;
            new->startFunc=startFunc;
            new->arg=arg;
            new->pid_parent=curr_pid;
            strcpy(new->name,name);
            curr_pid=new->pid;
            proc *par=&table[curr_pid-1];
            new->parent=par;
            if(par->child==NULL) {
                par->child= new;
            } else {
                proc*old=par->child;
                proc*curr=old;
                while(curr->younger!=NULL) {
                    curr=curr->younger;
                }
                curr->younger=new;
                new->older=curr;
            }
            int p=new->prio-1;
            queue[p][queue_len[p]++]=new;
        }
        return curr_pid;
    }
    return -1;
}

int  join(int *status){
    if(status==NULL){
        return -3;
    }
    proc *curr=&table[curr_pid-1];
    proc *dead=NULL;
    proc *child = current_proc->oldest;
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
            curr->oldest=dead->younger;
        }
        if (dead->younger!=NULL) {
            dead->younger->older=dead->older;
        }
        return dead->pid;
    }
    if(curr->oldest!=0){
        return -1;
    }
    return -2;
}

void quit_phase_1a(int status, int switchToPid) {
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
    ULOSS_ContextSwitch(switchToPid);
}

int  getpid(void){
    return curr_pid;
}

extern void dumpProcesses(void){
    printf("Dump Process");
    for(int i=0; i<MAXPROC; i++){
        if(table[i].pid!=-1){
            printf("%5d %5s %5d %5d %5d %5s\n", 
            table[i].pid, 
            table[i].name, 
            table[i].pid_parent, 
            table[i].prio, 
            table[i].status, 
            table[i].state);
        }
    }
}

void TEMP_switchTo(int pid){
    curr_pid=pid;
    curr_pcb=&table[pid-1]
}
