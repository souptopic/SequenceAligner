### TODO LIST
- [x] Script for first time creation of _mega datasets

- [ ] Create a user specification script 
	- [x] Module for choosing the local dataset location, ~~or add an open file dialog to existing code~~ (finished with arguments)
	- [x] Module for finding the optimal values for BATCH_SIZE and WRITE_BUFFER_SIZE
	- [ ] Module for choosing the format of input and output csv

- [ ] Perform a Similarity Analysis on the Results

- [ ] Buffered writing instead of write(), or just exclude writing time from benchmarking altogether

- [ ] Benchmark from first line read until last result written 
	- currently until last line aligned, will finish after write optimizations

- [ ] Optimize align_sequences() or replace with better algorithm

- [ ] Proper multithreading; right now the results could be orders of magnitude faster. Probably needs things like work stealing and / or better thread management.

- [ ] Add cases for AVX-512 and for neither AVX2 and AVX-512

- [ ] Comment unclear code
