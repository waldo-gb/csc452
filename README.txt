Testcase 8: Our output differs slightly from the expected output with each process terminating
and join be called directly after instead of XXp2a and XXp2b completeing their sends and then the
two joins being called. The difference between the expected output and our own is due to
the way in which we decided to implement MboxRelease(). Rather than having this
function wake up only the head of the producer and consumer queues, we have this
function be responsible for waking up each blocked process in the queue.
This leads to a difference in the order that the processes terminate as one 
process is not responsible for waking another and thus will send its message,
join, and then proceed to the next process. This is not an error as the order of
all the messages is kept and the priority is not violated. Our output matches all
of the non implementation dependent aspects of the testcase.

Testcase 30: Our output differs slighly from the expected output in a similar way to Testcase 8 as it 
is involved with how the XXp2 processes terminate. Those instances receive messages from the zero slot
and they block until XXp3 releases the mailbox. Here again, the difference is due to the implementation for 
MboxRelease(). Since we have Mbox release wake up each individual process, 
one process will not context switch to another and thus will send the message
and terminate, rather than sending the message, context switching, and terminating
later. Again, this is not an error as the order of all the messages is kept and the 
priority is not violated, it is just a difference in implementation.

Testcase 31: Similar to the ones above, the difference we have in this testcase is regarding the XXp2 processes
and how they terminate and how the joins in the testcase are called. Instead of XXp2a and XXp2b terminating and then
their two joins being called, ours does it in order. This difference is not forbidden by the spec and all of the other
spec requirments and testcases goals are met by this output.

Testcase 46: Similar to the ones above, the difference we have in the testcase is with the joins being called after the 
results of Child1a and Child1b are reported. Our implementation has the report happen and the the respective join be called
while the offical output has a and b report and then the respective joins happen. This difference is not specified in the 
spec and our output matches all other parts of this testcase.
