/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2018 by Charles Anthony
 Copyright 2015 by Eric Swenson

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

#ifndef UVUTIL_H
#define UVUTIL_H

struct uv_access_s
  {
    uv_loop_t * loop;
    int port;
    char * address;
#define PW_SIZE 128
    char pw[PW_SIZE + 1];
    char pwBuffer[PW_SIZE + 1];
    int pwPos;

    void (* connectPrompt) (uv_tcp_t * client);
    void (* connected) (uv_tcp_t * client);
    bool open;
    uv_tcp_t server;
    uv_tcp_t * client;
    bool useTelnet;
    void * telnetp;
    bool loggedOn;
    unsigned char * inBuffer;
    uint inSize;
    uint inUsed;
  };

typedef struct uv_access_s uv_access;
void accessStartWriteStr (uv_tcp_t * client, char * data);
void uv_open_access (uv_access * access);
void accessPutChar (uv_access * access,  char ch);
int accessGetChar (uv_access * access);
void accessPutStr (uv_access * access, char * str);
void accessStartWrite (uv_tcp_t * client, char * data, ssize_t datalen);
void accessCloseConnection (uv_stream_t* stream);
#endif
