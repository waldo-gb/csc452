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
    table[curr-1].startFunc(table[curr-1].arg);
    quit_phase_1a(0, table[curr_pid - 1].parent->pid);
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
            //curr_pid=new->pid;
            proc *par = curr_pcb;
            new->parent=par;
            new->child = NULL;
            new->older = par->child;
            if (par->child != NULL) par->child->younger = new;
            par->child = new;
            new->younger = NULL;
        
            new->stack=malloc(USLOSS_MIN_STACK);
            USLOSS_ContextInit(&new->context,new->stack,USLOSS_MIN_STACK,NULL,trampoline);
            int p=new->prio-1;
            queue[p][queue_len[p]++]=new;
            return new->pid;
        }
    }
    return -1;
}

int join(int *status){
    //USLOSS_Console("Join Name: %s, PID: %d\n", &table[curr_pid-1].name, table[curr_pid-1].pid);
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
        int id = dead->pid;
        memset(&table[dead->pid - 1], 0, sizeof(proc));
        return id;
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
    TEMP_switchTo(switchToPid);
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
    //USLOSS_Console("Name: %s, PID: %d\n", &table[curr_pid-1].name, table[curr_pid-1].pid);
    //USLOSS_Console("Name: %s, PID: %d\n", &table[pid-1].name, table[pid-1].pid);
    int old = curr_pid;
    curr_pid=pid;
    curr_pcb=&table[pid-1];
    USLOSS_ContextSwitch(&table[old-1].context,&table[pid-1].context);
    
    
}
void phase1_init(void) {
    memset(table,0,sizeof(table));
    memset(queue_len,0,sizeof(queue_len));
    for(int i=0; i<=MAXPROC; i++){
        table[i].pid=0;
        table[i].status=0;
    }
    curr_pid=1;
    strcpy(table[0].name, "init");
    table[0].pid = 1;

    table[0].status=0;  
    table[0].prio=6;
    table[0].state=RUNNING;
    curr_pcb=&table[0];
    USLOSS_Console("Phase 1A TEMPORARY HACK: init() manually switching to PID 1.\n");
    TEMP_switchTo(1);
    phase2_start_service_processes();
    phase3_start_service_processes();
    phase4_start_service_processes();
    phase5_start_service_processes();
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
