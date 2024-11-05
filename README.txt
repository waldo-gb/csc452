Testcase 7: Our blocked processes by the semaphore are unblocked before all of the V's have been 
completed. Thus child 2 ends right before child 1c ends and child 1a and 1b have already ended. This differs
from the spec because child 2 ends before child 1a and 1b have ended. This difference does not violate any of
the requirements in the spec and is a small differnce in the blocking and unblocking of the semaphores.

Testcase 10: Our time numbers are slightly less than the ones in the expected testcase,
thus we are within the bounds of the spec as it says that we just need to be close, not
have the exact same numbers.
