/* testcase_main attempts to create two children with invalid priorities. 
 */

#include <stdio.h>
#include <usloss.h>
#include <phase1.h>

int XXp1(void *);

int   tm_pid = -1;

int testcase_main()
{
    int pid1;

    tm_pid = getpid();

    USLOSS_Console("testcase_main(): started\n");
// TODO    USLOSS_Console("EXPECTATION: TBD\n");
    USLOSS_Console("QUICK SUMMARY: Attempt to spork() with invalid priorities.\n");

    pid1 = spork("XXp1", XXp1, "XXp1", USLOSS_MIN_STACK, 9);
    if (pid1 == -1)
        USLOSS_Console("testcase_main(): could not spork a child--invalid priority\n"); 

    pid1 = spork("XXp1", XXp1, "XXp1", USLOSS_MIN_STACK, -10);
    if (pid1 == -1)
        USLOSS_Console("testcase_main(): could not spork a child--invalid priority\n"); 

    return 0; /* so gcc will not complain about its absence... */
}

int XXp1(void *arg)
{
    USLOSS_Console("THIS SHOULD NEVER RUN\n");

    quit_phase_1a(3, tm_pid);
}

