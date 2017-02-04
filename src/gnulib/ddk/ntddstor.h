// to compile on mingw 4.8.0
#ifndef STORAGE_READ_CAPACITY
typedef struct _STORAGE_READ_CAPACITY {
  ULONG Version;
  ULONG Size;
  ULONG BlockLength;
  LARGE_INTEGER NumberOfBlocks;
  LARGE_INTEGER DiskLength;
} STORAGE_READ_CAPACITY, *PSTORAGE_READ_CAPACITY; 
#endif
