#include <usyscall.h>
#include <stdio.h>
#include <string.h>

// Maximum line length. Used by terminal read and write.
#define MAXLINE         80
#define MAXMBOX         2000
#define MAXSLOTS        2500
#define MAX_MESSAGE     150
#define MAX_PROC        100

void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);

typedef struct proc{
    int pid;
    int blocked;
    int mailID;
} proc;

typedef struct mailbox {
    int inUse;
    int slots;
    int slot_size;
    int destory;
    int msg_count;
    void *messages[MAXSLOTS];
} mailbox;

typedef struct mail_slot{
    int inUse;
    char message[MAX_MESSAGE];
} mail_slot;

mailbox mail[MAXMBOX];
mail_slot slots[MAXSLOTS];
proc table[MAX_PROC];

void phase2_init(void) {
    for (int i = 0; i < MAXSYSCALLS; i++) {
        systemCallVec[i]=nullsys;
    }
    memset(mail,0,sizeof(mail));
    memset(slots,0,sizeof(slots));
    memset(table,0,sizeof(table));
}

void nullsys(USLOSS_Sysargs *args) {
    printf("Error: Invalid system call\n");
    USLOSS_Halt(1);
}

// returns id of mailbox, or -1 if no more mailboxes, or -1 if invalid args
int MboxCreate(int slots, int slot_size){
    if (slots<=0 || slot_size<=0 || slot_size>MAXSLOTS) {
        return -1;
    }
    int mboxID=-1;
    for (int i=0; i<MAXMBOX; i++) {
        if (mail[i].inUse==0) {
            return mboxID=i;
        }
    }
    if (mboxID==-1) {
        return -1;
    }
    mail[mboxID].inUse=1;
    mail[mboxID].slots=slots;
    mail[mboxID].slot_size=slot_size;
    mail[mboxID].destroyed=0;
    mail[mboxID].current_slots=0;
    return mboxID;
}

// returns 0 if successful, -1 if invalid arg
int MboxRelease(int mbox_id){
    if(mbox_id<0||mbox_id>=MAXMBOX||!mail[mbox_id].inUse){
        return -1;
    }
    mail[mailboxID].destroyed=1;
    //unblock processes
    mail[mailboxID].inUse=0;
    return 0;
}       

// returns 0 if successful, -1 if invalid args
int MboxSend(int mbox_id, void *msg_ptr, int msg_size){
    if (mbox_id < 0 || mbox_id >= MAXMBOX || !mail[mbox_id].inUse || msg_size > mail[mbox_id].slot_size || (msg_size > 0 && msg_ptr == NULL)) {
        return -1;
    }
    if (mail[mbox_id].destroyed) {
        return -1;
    }
    if (mail[mbox_id].msg_count>=mail[mbox_id].slots) {
        //block the sender
        return -2;
    }
    int slotID=-1;
    for (int i=0; i<MAXSLOTS; i++) {
        if (slots[i].inUse == 0) {
            slotID=i;
        }
    }
    if (slotID==-1) {
        return -2;
    }
    slots[slotID].inUse=1;
    memcpy(slots[slotID].message, msg_ptr, msg_size);
    mail[mbox_id].messages[mail[mbox_id].current_slots++]=slots[slotID].message;
    return 0;
}

// returns size of received msg if successful, -1 if invalid args
int MboxRecv(int mbox_id, void *msg_ptr, int msg_max_size){
    if (mbox_id<0 || mbox_id>=MAXMBOX || !mail[mbox_id].inUse || msg_ptr==NULL) {
        return -1;
    }
    if (mail[mbox_id].destroy) {
        return -1;
    }
    if (mail[mbox_id].current_slots == 0) {
        //block the reviever
        return -2;
    }
    char *msg=(char *) mail[mbox_id].messages[0];
    int msgSize=strlen(msg);
    if (msg_max_size<msgSize) {
        return -1;
    }
    memcpy(msg_ptr, msg, msgSize);
    for (int i=1; i<mail[mbox_id].current_slots; i++) {
        mail[mbox_id].messages[i-1]=mail[mbox_id].messages[i];
    }
    mail[mbox_id].current_slots--;
    return msgSize;
}

// returns 0 if successful, 1 if mailbox full, -1 if illegal args
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size){
    if (mbox_id < 0 || mbox_id >= MAXMBOX || !mail[mbox_id].inUse || msg_size > mail[mbox_id].slot_size || (msg_size > 0 && msg_ptr == NULL)) {
        return -1;
    }
    if (mail[mbox_id].destroyed) {
        return -1;
    }
    if (mail[mbox_id].msg_count>=mail[mbox_id].slots) {
        return -2;
    }
    int slotID=-1;
    for (int i=0; i<MAXSLOTS; i++) {
        if (slots[i].inUse == 0) {
            slotID=i;
        }
    }
    if (slotID==-1) {
        return -2;
    }
    slots[slotID].inUse=1;
    memcpy(slots[slotID].message, msg_ptr, msg_size);
    mail[mbox_id].messages[mail[mbox_id].current_slots++]=slots[slotID].message;
    return 0;
}

// returns 0 if successful, 1 if no msg available, -1 if illegal args
int MboxCondRecv(int mbox_id, void *msg_ptr, int msg_max_size){
    if (mbox_id<0 || mbox_id>=MAXMBOX || !mail[mbox_id].inUse || msg_ptr==NULL) {
        return -1;
    }
    if (mail[mbox_id].destroy) {
        return -1;
    }
    if (mail[mbox_id].current_slots == 0) {
        return -2;
    }
    char *msg=(char *) mail[mbox_id].messages[0];
    int msgSize=strlen(msg);
    if (msg_max_size<msgSize) {
        return -1;
    }
    memcpy(msg_ptr, msg, msgSize);
    for (int i=1; i<mail[mbox_id].current_slots; i++) {
        mail[mbox_id].messages[i-1]=mail[mbox_id].messages[i];
    }
    mail[mbox_id].current_slots--;
    return msgSize;
}

// type = interrupt device type, unit = # of device (when more than one),
// status = where interrupt handler puts device's status register.
extern void waitDevice(int type, int unit, int *status) {
    int mailboxID=-1;
    switch (type) {
        case 0:
            if (unit!=0) {
                mailboxID=-1;
            }
            mailboxID=0;
        case 1:
            if (unit==0) {
                mailboxID=1;
            } else if (unit==1) {
                mailboxID=2;
            } else {
                mailboxID=-1;
            }
        case 2:
            if (unit>=0 && unit<=3) {
                mailboxID=3 + unit;
            } else {
                mailboxID=-1;
            }
        default:
            mailboxID=-1;  
    }
    if (mailboxID==-1) {
        printf("Error: Invalid device type or unit\n");
        USLOSS_Halt(1);
    }
    int rec_status=MboxRecv(mailboxID, status, sizeof(int));
    if (rec_status<0) {
        printf("Error: Failed to receive device status from mailbox %d\n", mailboxID);
        USLOSS_Halt(1);
    }
    *status=rec_status;
}
extern void wakeupByDevice(int type, int unit, int status){
    int mailboxID=-1;
    switch (type) {
        case 0:
            if (unit!=0) {
                mailboxID=-1;
            }
            mailboxID=0;
        case 1:
            if (unit==0) {
                mailboxID=1;
            } else if (unit==1) {
                mailboxID=2;
            } else {
                mailboxID=-1;
            }
        case 2:
            if (unit>=0 && unit<=3) {
                mailboxID=3 + unit;
            } else {
                mailboxID=-1;
            }
        default:
            mailboxID=-1;  
    }
    if (mailboxID==-1) {
        printf("Error: Invalid device type or unit\n");
        USLOSS_Halt(1);
    }
    int send_status=MboxCondSend(mailboxID, status, sizeof(int));
    if (send_status<0) {
        printf("Error: Failed to receive device status from mailbox %d\n", mailboxID);
        USLOSS_Halt(1);
    }
    *status=send_status;
}
// 
extern void (*systemCallVec[])(USLOSS_Sysargs *args);
