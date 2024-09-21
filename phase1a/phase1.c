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


/* 
 * These functions must be provided by Phase 1.
 */
int curr_pid=-1;
proc curr_pcb;

***CONSTRUCTOR***kinda
USLOSS_ContextInit(&table[i].context)

struct proc{
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

proc table[MAXPROC];
memset(table,0,sizeof(table));
table[1].pid=1;
table[1].......
void phase1_init(void) {
    int val;
    for(int i=0; i<=MAXPROC; i++){
        table[i].pid=-1;
        table[i].status=-1;
    }
    curr_pid=1;
    table[1].pid=1;
    table[1].status=0;
    table[1].prio=6;
    int testcase=spork("testcase_main",testcas_main, NULL, USLOSS_MIN_STACK, 3);
    ULOSS_ContextSwitch(testcase);
    printf("testcase_main returned...haulting");
    USLOSS_HAULT(0);
}
int testcase_main(void){
    testcase_main()
}
int spork(char *name, int (*startFunc)(char*), char *arg, int stackSize, int priority) {
    if(stackSize<USLOSS_MIN_STACK){
        return -2
    }
    for(int i=1; i<=MAXPROC; i++){
        if(table[i].status==-1){
            table[i].pid=i+1;
            table[i].prio=priority;
            table[i].status=0;
            table[i].startFunc=startFunc;
            table[i].arg=arg;
            table[i].pid_parent=curr_pid
            curr_pid=table[i].pid;
        }
        return curr_pid;
    }
    return -1;
}

int  join(int *status){
    if(status==NULL){
        return -3;
    }
    int curr=curr_pid;
    for(int i=0; i<MAXPROC; i++){
        if(table[i].pid_parent==curr&&table[i].status=-1) {
            *status=table[i].exit_status;
            int child=table[i].pid;
            table[i].pid=-1;
            return child
        }
    }
    return -2
}

void quit_phase_1a(int status, int switchToPid) {
    int curr=getpid();
    table[curr]=-1
    table[curr]=status
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
}
int status;
int pid=join(&status);

int next_pid_to_try;
while(next_pid_to_try -> is not free)
    next_pid_to_try++;