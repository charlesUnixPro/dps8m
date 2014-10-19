#CC = gcc
#LD = gcc
CC = clang
LD = clang

# for Linux (Ubuntu 12.10 64-bit) or Apple OS/X 10.8
#CFLAGS  = -g -O0
CFLAGS  = -g -O3

CFLAGS += -I../decNumber -I../simh 

CFLAGS += -std=c99 -U__STRICT_ANSI__  

# CFLAGS += -finline-functions -fgcse-after-reload -fpredictive-commoning -fipa-cp-clone -fno-unsafe-loop-optimizations -fno-strict-overflow -Wno-unused-result 

CFLAGS += -D_GNU_SOURCE -DUSE_READER_THREAD -DHAVE_DLOPEN=so 
CFLAGS += -DUSE_INT64
#CFLAGS += -DMULTIPASS

LDFLAGS = -g
#CFLAGS += -pg
#LDFLAGS += -pg


