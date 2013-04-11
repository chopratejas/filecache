filecache
=========

A basic cache of files

Memory comes in hierarchy. The faster ones are costlier, and so are less in quantity. Different levels of caching are
present in today's architectures, namely: L1, L2, L3, DRAM memory and finally disk. Retrieving data from the cache
and writing data to a cache is faster as compared to seeking data from the disk.
We rely on the principles of temporal and spatial locality to ensure that we take advantage of caching. The code
written in this project5 is a simple file-cache. Accessing files would be faster if they lie in this cache.
A write done to a file would mark it as dirty and at some point it needs to be flushed to the next higher hierarchy
i.e. memory and disk. Basic principles of eviction need to be followed as well. This is an attempt to get some 
caching going and is no way reflective of how it is handled in real-world systems.

The project can be extended to have multi-threading support although now its minimalistic. Condition variables, mutexes
and semphores can be used at several points to lock data structures so that multiple threads dont change them.

