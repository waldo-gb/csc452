Testcase 00: The difference between our output and the expected output is the time values for the clock.
Our values are close to the expected values but can be attributed to the difference in devices thus making it
within the bounds of the spec.

Testcase 01: The difference between our output and the expected output is the time values for the clock.
Our values are close to the expected values but can be attributed to the difference in devices thus making it
within the bounds of the spec.

Testcase 02: The difference between our output and the expected output is the time values for the clock.
Our values are close to the expected values but can be attributed to the difference in devices thus making it
within the bounds of the spec. The other difference is in the pid values being returned from wait.
Our values are one below the expected value which can be attributed to one less process being created in our solution
which does not violate anything in the spec.

Testcase 03: The difference between our output and the expected output is the pid and kid pid values. Our values
are still equal to eachother, however they are 12 when the expected values are ten. This does not violate the spec
because our values are still equal and this just means that two more processes were created in our solution than in
the expected solution.

Testcase 06: Our ouput when run on our machine is identicial to the expected one, however it differes on gradescope 
with child1 terminating very early in the process. This does not violate the spec as child1 still writes
to the terminal, it just terminates sooner than the expected output.

Testcase 20:The difference in our output comes from differences in process schdueleing between devices. All of the readers
read the intended number of lines from the writers and read from the correct terminal. The only difference 
is the order in which this happens which is normal for a system such as USLOSS.
