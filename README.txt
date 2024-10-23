Testcase 8: Our output differs slightly from the expected output with each process terminating
and join be called directly after instead of XXp2a and XXp2b completeing their sends and then the
two joins being called. This difference does not violate anything in the spec as they are all the same
priority and XXp3 completes afterwards since it is a different priority. Our output matches all of the 
not implementation dependent aspects of this testcase since the only difference is how the XXp2 processes 
terminate.

Testcase 30: Our output differs slighly from the expected output in a similar way to Testcase 8 as it 
is involved with how the XXp2 processes terminate. Those instances receive messages from the zero slot
and they block until XXp3 releases the mailbox. Then these three processes terminate and the joins are called 
which causes the difference as the order of these events are slightly different. Since this is not in the spec
or an aim of the testcase our output and the official output are aligned in the important ways. In addition, XXp3
terminates after these three processes as aligns with the given priorities.

Testcase 31: Similar to the ones above, the difference we have in this testcase is regarding the XXp2 processes
and how they terminate and how the joins in the testcase are called. Instead of XXp2a and XXp2b terminating and then
their two joins being called, ours does it in order. This difference is not forbidden by the spec and all of the other
spec requirments and testcases goals are met by this output.

Testcase 46: Similar to the ones above, the difference we have in the testcase is with the joins being called after the 
results of Child1a and Child1b are reported. Our implementation has the report happen and the the respective join be called
while the offical output has a and b report and then the respective joins happen. This difference is not specified in the 
spec and our output matches all other parts of this testcase.
