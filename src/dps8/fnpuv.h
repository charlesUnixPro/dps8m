struct uvClientData
  {
    bool assoc;
    uint fnpno;
    uint lineno;
    /* telnet_t */ void * telnetp;
  };

typedef struct uvClientData uvClientData;

void fnpuvInit (int telnet_port);
void fnpuvProcessEvent (void);
void fnpuv_start_write (void * client, char * data, ssize_t len);
void fnpuv_start_writestr (void * client, char * data);
void fnpuv_start_write_actual (/* uv_tcp_t */ void  * client, char * data, ssize_t datalen);
void fnpuv_unassociated_readcb (uv_stream_t* stream, ssize_t nread, char * buf);
void fnpuv_associated_readcb (uv_stream_t* stream, ssize_t nread, char * buf);
void fnpuv_read_start (void * client);
void fnpuv_read_stop (void * client);
void fnpuv_dial_out (uint fnpno, uint lineno, word36 d1, word36 d2, word36 d3);
void fnpuv_open_slave (uint fnpno, uint lineno);

