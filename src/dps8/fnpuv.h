struct uvClientData
  {
    bool assoc;
    uint fnpno;
    uint lineno;
    /* telnet_t */ void * telnetp;
    // Work buffet for processLineInput
    char buffer [1024];
    size_t nPos;
  };

typedef struct uvClientData uvClientData;

void fnpuvInit (int telnet_port);
void fnpuvProcessEvent (void);
void fnpuv_start_write (uv_tcp_t * client, char * data, ssize_t len);
void fnpuv_start_writestr (uv_tcp_t * client, char * data);
void fnpuv_start_write_actual (uv_tcp_t * client, char * data, ssize_t datalen);
void fnpuv_unassociated_readcb (uv_tcp_t * client, ssize_t nread, unsigned char * buf);
void fnpuv_associated_readcb (uv_tcp_t * client, ssize_t nread, unsigned char * buf);
void fnpuv_read_start (uv_tcp_t * client);
void fnpuv_read_stop (uv_tcp_t * client);
void fnpuv_dial_out (uint fnpno, uint lineno, word36 d1, word36 d2, word36 d3);
void fnpuv_open_slave (uint fnpno, uint lineno);
void close_connection (uv_stream_t* stream);
#ifdef TUN
void fnpTUNProcessEvent (void);
#endif
