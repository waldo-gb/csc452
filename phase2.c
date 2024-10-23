#include <usyscall.h>
#include <stdio.h>
#include <string.h>
#include <usloss.h>

// Maximum line length. Used by terminal read and write.
#define MAXLINE         80
#define MAXMBOX         2000
#define MAXSLOTS        2500
#define MAX_MESSAGE     150
#define MAX_PROC        100
#define MAX_SYSCALLS     100

void (*systemCallVec[MAX_SYSCALLS])(USLOSS_Sysargs *args);

typedef struct proc{
    int pid;
    int blocked;
    int mailID;
    void *msg_ptr;
    int msg_size; 
    int status;
} proc;

typedef struct mailbox {
    int inUse;
    int slots;
    int slot_size;
    int destroy;
    int msg_count;  
    void *messages[MAXSLOTS];
    int blockedSenders[10];
    int blockedSenderCount;
    int blockedReceivers[10];
    int blockedReceiverCount;
} mailbox;

typedef struct mail_slot{
    int inUse;
    char message[MAX_MESSAGE];
} mail_slot;

int clockMailbox;
int diskMailboxes[2];
int terminalMailboxes[4];


int interruptMailboxes[7];
mailbox mail[MAXMBOX];
mail_slot slots[MAXSLOTS];
proc table[MAX_PROC];
int total_used=0;
int lastClockTick=0;

void nullsys(USLOSS_Sysargs *args) {
    USLOSS_Console("Error: Invalid system call\n");
    USLOSS_Halt(1);
}
void clockHandler(int type, void *arg) {
    int status;
    int curr;
    USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &curr);
    if (curr-lastClockTick>=100) {
        lastClockTick=curr; 
        status=curr;
        MboxCondSend(clockMailbox, &status, sizeof(int));
    }
    dispatcher();
}
void diskHandler(int type, void *arg) {
    int unit=(int)arg;
    int status;
    if (unit >= 0 && unit < 2) {
        USLOSS_DeviceInput(USLOSS_DISK_DEV, unit, &status);
        MboxSend(diskMailboxes[unit], &status, sizeof(int));
    }
}
void terminalHandler(int type, void *arg) {
    int unit =(int)arg;
    int status;
    if (unit>=0 && unit<2) {
        USLOSS_DeviceInput(USLOSS_TERM_DEV, unit, &status);
        MboxSend(terminalMailboxes[unit], &status, sizeof(int));
    }
}
void syscallHandler(int type, void *arg) {
    USLOSS_Sysargs *sysargs=(USLOSS_Sysargs *)arg;
    if (sysargs->number < 0 || sysargs->number >= MAX_SYSCALLS) {
        USLOSS_Console("Error: Invalid syscall number %d\n", sysargs->number);
        USLOSS_Halt(1);
    }
    (*systemCallVec[sysargs->number])(sysargs);
}
void phase2_init(void) {
    for (int i=0; i<MAX_SYSCALLS; i++) {
        systemCallVec[i]=nullsys;
    }
    memset(mail,0,sizeof(mail));
    memset(slots,0,sizeof(slots));
    memset(table,0,sizeof(table));
    clockMailbox=MboxCreate(1, sizeof(int));
    diskMailboxes[0]=MboxCreate(1, sizeof(int));
    diskMailboxes[1]=MboxCreate(1, sizeof(int));
    terminalMailboxes[0]=MboxCreate(1, sizeof(int));
    terminalMailboxes[1]=MboxCreate(1, sizeof(int));
    terminalMailboxes[2]=MboxCreate(1, sizeof(int));
    terminalMailboxes[3]=MboxCreate(1, sizeof(int));
    USLOSS_IntVec[USLOSS_CLOCK_INT]=clockHandler;
    USLOSS_IntVec[USLOSS_DISK_INT]=diskHandler;
    USLOSS_IntVec[USLOSS_TERM_INT]=terminalHandler;
    USLOSS_IntVec[USLOSS_SYSCALL_INT]=syscallHandler;
}

void phase2_start_service_processes(void){
    
}

// returns id of mailbox, or -1 if no more mailboxes, or -1 if invalid args
int MboxCreate(int slots, int slot_size){
    if (slots<0 || slot_size<0 || slot_size>MAXSLOTS) {
        return -1;
    }
    int mboxID=-1;
    for (int i=0; i<MAXMBOX; i++) {
        if (mail[i].inUse==0) {
            mboxID=i;
            break;
        }
    }
    if (mboxID==-1) {
        return -1;
    }
    mail[mboxID].inUse=1;
    mail[mboxID].slots=slots;
    mail[mboxID].slot_size=slot_size;
    mail[mboxID].destroy=0;
    mail[mboxID].msg_count=0;
    return mboxID;
}

// returns 0 if successful, -1 if invalid arg
int MboxRelease(int mbox_id){
    if(mbox_id<0||mbox_id>=MAXMBOX||!mail[mbox_id].inUse){
        return -1;
    }
    mail[mbox_id].destroy=1;
    while (mail[mbox_id].blockedSenderCount>0) {
        int sender_pid=mail[mbox_id].blockedSenders[0];
        mail[mbox_id].blockedSenderCount--;
        int i=1;
        while(i<=mail[mbox_id].blockedSenderCount){
            mail[mbox_id].blockedSenders[i-1]=mail[mbox_id].blockedSenders[i];
            i++;
        }
        mail[mbox_id].blockedSenders[i]=NULL;
        unblockProc(sender_pid);
    }
    while (mail[mbox_id].blockedReceiverCount>0) {
        int receiver_pid=mail[mbox_id].blockedReceivers[0];
        mail[mbox_id].blockedReceiverCount--;
        int i=1;
        while(i<=mail[mbox_id].blockedReceiverCount){
            mail[mbox_id].blockedReceivers[i-1]=mail[mbox_id].blockedReceivers[i];
            i++;
        }
        mail[mbox_id].blockedReceivers[i]=NULL;
        unblockProc(receiver_pid);
    }
    dispatcher();
    mail[mbox_id].inUse=0;
    return 0;
}       

// returns 0 if successful, -1 if invalid args
int MboxSend(int mbox_id, void *msg_ptr, int msg_size){
    if (mbox_id<0 || mbox_id >= MAXMBOX || !mail[mbox_id].inUse || msg_size>mail[mbox_id].slot_size || (msg_size>0 && msg_ptr==NULL)) {
        return -1;
    }
    if (mail[mbox_id].destroy) {
        return -1;
    }
     if (mail[mbox_id].slots==0) {
        if (mail[mbox_id].blockedReceiverCount>0) {
            int receiver_pid=mail[mbox_id].blockedReceivers[0];
            proc *receiver_proc=&table[receiver_pid];
            for (int i=1; i<mail[mbox_id].blockedReceiverCount; i++) {
                mail[mbox_id].blockedReceivers[i-1]=mail[mbox_id].blockedReceivers[i];
            }
            mail[mbox_id].blockedReceiverCount--;
            memcpy(receiver_proc->msg_ptr, msg_ptr, msg_size);
            receiver_proc->msg_size=msg_size;
            receiver_proc->blocked=0;
            unblockProc(receiver_pid);
            return 0;
        } else {
            int current_pid=getpid();
            mail[mbox_id].blockedSenders[mail[mbox_id].blockedSenderCount++]=current_pid;
            table[current_pid].msg_ptr=msg_ptr;
            table[current_pid].msg_size=msg_size;
            table[current_pid].blocked=1;
            blockMe();
            if (mail[mbox_id].destroy) {
                return -1;
            }
            return 0;
        }
    }
    if (mail[mbox_id].blockedReceiverCount>0) {
        int receiver_pid=mail[mbox_id].blockedReceivers[--mail[mbox_id].blockedReceiverCount];
        proc *receiver_proc=&table[receiver_pid];
        if (receiver_proc->mailID==mbox_id && receiver_proc->blocked) {
            memcpy(receiver_proc->msg_ptr, msg_ptr, msg_size);
            receiver_proc->msg_size=msg_size;
            receiver_proc->blocked=0;
            unblockProc(receiver_pid);
            return 0;
        }
    }
    if (total_used>= MAXSLOTS) {
        return -2;
    } 
    while (mail[mbox_id].msg_count>=mail[mbox_id].slots) {
        int current_pid=getpid();
        if (mail[mbox_id].blockedSenderCount<10) {
            mail[mbox_id].blockedSenders[mail[mbox_id].blockedSenderCount++]=current_pid;
        }
        blockMe();
        if (mail[mbox_id].destroy) {
            return -1;
        }
    }
    int slotID=-1;
    for (int i=0; i<MAXSLOTS; i++) {
        if (slots[i].inUse==0) {
            slotID=i;
            break;
        }
    }
    if (slotID==-1) {
        return -2;
    }
    slots[slotID].inUse=1;
    memcpy(slots[slotID].message, msg_ptr, msg_size);
    mail[mbox_id].messages[mail[mbox_id].msg_count++]=slots[slotID].message;
    total_used++;
    if (mail[mbox_id].blockedReceiverCount>0) {
        int receiver_pid=mail[mbox_id].blockedReceivers[--mail[mbox_id].blockedReceiverCount];
        unblockProc(receiver_pid);
    }
    return 0;
}

// returns size of received msg if successful, -1 if invalid args
int MboxRecv(int mbox_id, void *msg_ptr, int msg_max_size){
    if (mbox_id<0 || mbox_id>=MAXMBOX || !mail[mbox_id].inUse) {
        return -1;
    }
    if (mail[mbox_id].destroy) {
        return -1;
    }
    if (mail[mbox_id].slots==0) {
        if (mail[mbox_id].blockedSenderCount>0) {
            int sender_pid=mail[mbox_id].blockedSenders[0];
            proc *sender_proc=&table[sender_pid];
            for (int i=1; i<mail[mbox_id].blockedSenderCount; i++) {
                mail[mbox_id].blockedSenders[i-1]=mail[mbox_id].blockedSenders[i];
            }
            mail[mbox_id].blockedSenderCount--;
            int msg_size=sender_proc->msg_size;
            if (msg_max_size<msg_size) {
                return -1;
            }
            memcpy(msg_ptr, sender_proc->msg_ptr, msg_size);
            sender_proc->blocked=0;
            unblockProc(sender_pid);
            return msg_size;
        } else {
            int current_pid=getpid();
            mail[mbox_id].blockedReceivers[mail[mbox_id].blockedReceiverCount++]=current_pid;
            table[current_pid].msg_ptr=msg_ptr;
            table[current_pid].msg_size=msg_max_size;
            table[current_pid].blocked=1;
            blockMe();
            if (mail[mbox_id].destroy) {
                return -1;
            }
            return table[current_pid].msg_size;
        }
    }
    if(mail[mbox_id].msg_count>0) {
        char *msg=(char *)mail[mbox_id].messages[0];
        int msgSize=strlen(msg)+1;
        if(msg_max_size<msgSize) {
            return -1;
        }
        memcpy(msg_ptr, msg, msgSize);
        for(int i=1; i<mail[mbox_id].msg_count; i++) {
            mail[mbox_id].messages[i-1]=mail[mbox_id].messages[i];
        }
        mail[mbox_id].msg_count--;
        total_used--;
        if(mail[mbox_id].blockedSenderCount>0) {
            int sender_pid=mail[mbox_id].blockedSenders[0];
            mail[mbox_id].blockedSenderCount--;
            int i=1;
            while(i<=mail[mbox_id].blockedSenderCount){
                mail[mbox_id].blockedSenders[i-1]=mail[mbox_id].blockedSenders[i];
                i++;
            }
            mail[mbox_id].blockedSenders[i]=NULL;
            unblockProc(sender_pid);
        }
        return msgSize;
    }
    int current_pid=getpid();
    if(mail[mbox_id].blockedReceiverCount<10) {
        mail[mbox_id].blockedReceivers[mail[mbox_id].blockedReceiverCount++]=current_pid;
    }
    table[current_pid].mailID=mbox_id;
    table[current_pid].msg_ptr=msg_ptr;
    table[current_pid].msg_size=msg_max_size;
    table[current_pid].blocked=1;
    blockMe();
    if (mail[mbox_id].destroy) {
        return -1;
    }
    return table[current_pid].msg_size;
}

// returns 0 if successful, 1 if mailbox full, -1 if illegal args
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size){
    if (mbox_id<0 || mbox_id >= MAXMBOX || !mail[mbox_id].inUse || msg_size>mail[mbox_id].slot_size || (msg_size>0 && msg_ptr==NULL)) {
        return -1;
    }
    if (mail[mbox_id].destroy) {
        return -1;
    }
     if (mail[mbox_id].slots==0) {
        if (mail[mbox_id].blockedReceiverCount>0) {
            int receiver_pid=mail[mbox_id].blockedReceivers[0];
            proc *receiver_proc=&table[receiver_pid];
            for (int i=1; i<mail[mbox_id].blockedReceiverCount; i++) {
                mail[mbox_id].blockedReceivers[i-1]=mail[mbox_id].blockedReceivers[i];
            }
            mail[mbox_id].blockedReceiverCount--;
            memcpy(receiver_proc->msg_ptr, msg_ptr, msg_size);
            receiver_proc->msg_size=msg_size;
            receiver_proc->blocked=0;
            unblockProc(receiver_pid);
            return 0;
        } else {
            return -2;
        }
    }
    if (total_used>= MAXSLOTS) {
        return -2;
    } 
    while (mail[mbox_id].msg_count>=mail[mbox_id].slots) {
        return -2;
    }
    int slotID=-1;
    for (int i=0; i<MAXSLOTS; i++) {
        if (slots[i].inUse==0) {
            slotID=i;
            break;
        }
    }
    if (slotID==-1) {
        return -2;
    }
    slots[slotID].inUse=1;
    memcpy(slots[slotID].message, msg_ptr, msg_size);
    mail[mbox_id].messages[mail[mbox_id].msg_count++]=slots[slotID].message;
    if (mail[mbox_id].blockedReceiverCount>0) {
        int receiver_pid=mail[mbox_id].blockedReceivers[--mail[mbox_id].blockedReceiverCount];
        unblockProc(receiver_pid);
    }
    total_used++;
    return 0;
}

// returns 0 if successful, 1 if no msg available, -1 if illegal args
int MboxCondRecv(int mbox_id, void *msg_ptr, int msg_max_size){
    if (mbox_id<0 || mbox_id>=MAXMBOX || !mail[mbox_id].inUse) {
        return -1;
    }
    if (mail[mbox_id].destroy) {
        return -1;
    }
    if (mail[mbox_id].slots==0) {
        if (mail[mbox_id].blockedSenderCount>0) {
            int sender_pid=mail[mbox_id].blockedSenders[0];
            proc *sender_proc=&table[sender_pid];
            for (int i=1; i<mail[mbox_id].blockedSenderCount; i++) {
                mail[mbox_id].blockedSenders[i-1]=mail[mbox_id].blockedSenders[i];
            }
            mail[mbox_id].blockedSenderCount--;
            int msg_size=sender_proc->msg_size;
            if (msg_max_size<msg_size) {
                return -1;
            }
            memcpy(msg_ptr, sender_proc->msg_ptr, msg_size);
            sender_proc->blocked=0;
            unblockProc(sender_pid);
            return msg_size;
        } else {
            return -2;
        }
    }
    if (mail[mbox_id].msg_count==0) {
        return -2;
    }
    char *msg=(char *)mail[mbox_id].messages[0];
    int msgSize=strlen(msg) + 1;
    if (msg_max_size<msgSize) {
        return -1;
    }
    memcpy(msg_ptr, msg, msgSize);
    for (int i=1; i<mail[mbox_id].msg_count; i++) {
        mail[mbox_id].messages[i-1]=mail[mbox_id].messages[i];
    }
    mail[mbox_id].msg_count--;
    if (mail[mbox_id].blockedSenderCount>0) {
        int sender_pid=mail[mbox_id].blockedSenders[--mail[mbox_id].blockedSenderCount];
        unblockProc(sender_pid);
    }
    return msgSize;
}

// type = interrupt device type, unit = # of device (when more than one),
// status = where interrupt handler puts device's status register.
extern void waitDevice(int type, int unit, int *status) {
    int mailboxID=-1;
    switch (type) {
        case USLOSS_CLOCK_DEV:
            mailboxID=clockMailbox;
            break;
        case USLOSS_DISK_DEV:
            if (unit>=0 && unit<2) {
                mailboxID=diskMailboxes[unit];
            }
            break;
        case USLOSS_TERM_DEV:
            if (unit>=0 && unit<4) {
                mailboxID=terminalMailboxes[unit];
            }
            break;
        default:
            mailboxID=-1;
            break;
    }
    if (mailboxID==-1) {
        USLOSS_Console("Error: Invalid device type or unit\n");
        USLOSS_Halt(1);
    }
    int rec_status=MboxRecv(mailboxID, status, sizeof(int));
    if (rec_status<0) {
        USLOSS_Console("Error: Failed to receive device status from mailbox %d\n", mailboxID);
        USLOSS_Halt(1);
    }
    *status=rec_status;
}
void wakeupByDevice(int type, int unit, int status){
    int mailboxID=-1;
    switch (type) {
        case USLOSS_CLOCK_DEV:
            mailboxID=clockMailbox;
            break;
        case USLOSS_DISK_DEV:
            if (unit>=0 && unit<2) {
                mailboxID=diskMailboxes[unit];
            }
            break;
        case USLOSS_TERM_DEV:
            if (unit>=0 && unit<4) {
                mailboxID=terminalMailboxes[unit];
            }
            break;
        default:
            mailboxID=-1;  
            break;
    }
    if (mailboxID==-1) {
        USLOSS_Console("Error: Invalid device type or unit\n");
        USLOSS_Halt(1);
    }
    int send_status=MboxCondSend(mailboxID, &status, sizeof(int));
    if (send_status<0) {
        USLOSS_Console("Error: Failed to receive device status from mailbox %d\n", mailboxID);
        USLOSS_Halt(1);
    }
    status=send_status;
}
// 
extern void (*systemCallVec[])(USLOSS_Sysargs *args);
