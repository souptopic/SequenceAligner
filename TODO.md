### TODO LIST
- [ ] Create a user specification script 
	- [ ] Module for finding the optimal values for BATCH_SIZE and WRITE_BUFFER_SIZE
	- [x] Module for choosing the local dataset location, or add an open file dialog to existing code
	- [ ] Module for choosing the format of input and output csv

- [ ] Perform a Similarity Analysis on the Results

- [ ] Add more benchmark options and standardize benchmarking to make it fair and consistent across multiple runs

- [ ] Optimize align_sequences() or replace with better algorithm

- [ ] Buffered writing instead of write(), or just exclude writing time from benchmarking altogether

- [ ] Proper multithreading; right now the results could be orders of magnitude faster. Probably needs things like work stealing and / or better thread management.

- [x] Script for first time creation of _mega datasets
