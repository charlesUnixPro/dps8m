struct uv_access_s
  {
    uv_loop_t * loop;
    int port;
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
