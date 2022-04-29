# MultiThreadedWordWrap

This wordwrap program provides functionality for wrapping text files to a certain line width, all lines will be within that line width, if any word is longer than the line width this program will return EXIT_FAILURE. To utilize this program pass in the width and file as command line arguments. Ex: ./ww 20 foo.
If the provided file is a directory it will wrap all the files within that directory. This program also provides recursive support, if you pass in -r as the second command line argument it will recursively wrap all the files in the provided directory and subdirectory. Multithreaded support is also provided if you pass in -rM,N this program will wrap all files within the directory provided and subdirectories using M directory threads and N file threads.

For this wordwrap multithreading project we divided the threads into directory and file threads each with their corresponding queue. The directory queue would take in the initial
queue given as a command line argument. Then the directory threads would dequeue from the directory queue, and enqueue the file found to either the directory or file queue depending
on the file type. The file threads would solely dequeue from the file queue and wrap each file. With a counter for active directory threads, once the counter hit 0 and there were no more
to dequeue directory threads would begin to terminate while sending a broadcast to wake up any directory threads waiting to dequeue, then send a broadcast to any file threads waiting to dequeue.

We tested this program with multiple directory threads and multiple file threads, 1 directory thread and multiple file threads, 1 directory thread and 1 file thread, and multiple directory threads and 1 file thread. We tested this on sample input with multiple subdirectories and multiple files in each subdirectory.
