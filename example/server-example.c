enum State
{
    START,
    DONE,
    FILENAME,
    SEND_DATA,
    WAIT_ON_ACK,
    TIMEOUT_ON_ACK,
    WAIT_ON_EOF_ACK,
    TIMEOUT_ON_EOF_ACK
};

void process_server(int serverSocketNumber);

STATE wait_on_ack(Connection *client);

STATE filename(
    Connection *client,
    uint8_t *buf,
    int32_t recv_len,
    int32_t *data_file,
    int32_t *buf_size
);

STATE send_data(
    Connection *client,
    uint8_t *packet,
    int32_t *packet_len,
    int32_t data_file,
    int32_t buf_size,
    uint32_t *seqnum
);

STATE timeout_on_ack(
    Connection *client,
    uint8_t *packet,
    int32_t packet_len
);

STATE timeout_on_eof_ack(
    Connection *client,
    uint8_t *packet,
    int32_t packet_len
);

STATE wait_on_eof_ack(Connection *client);

int processArgs(int argc, char **argv);

void handleZombies(int sig);

int main(int argc, char *argv[])
{
    int32_t serverSocketNumber = 0;
    int portNumber = 0;

    portNumber = processArgs(argc, argv);

    sendtoErr_init(
        atof(argv[1]),
        DROP_ON,
        FLIP_ON,
        DEBUG_ON,
        RSEED_ON
    );

    /* set up the main server port */
    serverSocketNumber = udpServerSetup(portNumber);

    process_server(serverSocketNumber);

    return 0;
}

void process_server(int serverSocketNumber)
{
    pid_t pid = 0;

    uint8_t buf[MAX_LEN];

    Connection *client =
        (Connection *)calloc(1, sizeof(Connection));

    uint8_t flag = 0;
    uint32_t seq_num = 0;

    int32_t recv_len = 0;

    /*
     * We are going to fork,
     * so need to clean up SIGCHLD
     */
    signal(SIGCHLD, handleZombies);

    // get new client connection, fork child
    while (1)
    {
        // block waiting for a new client
        recv_len = recv_buf(
            buf,
            MAX_LEN,
            serverSocketNumber,
            client,
            &flag,
            &seq_num
        );

        if (recv_len != CRC_ERROR)
        {
            if ((pid = fork()) < 0)
            {
                perror("fork");
                exit(-1);
            }

            if (pid == 0)
            {
                /*
                 * child process
                 * a new process for each client
                 */
                printf(
                    "Child fork() - child pid: %d\n",
                    getpid()
                );

                process_client(
                    serverSocketNumber,
                    buf,
                    recv_len,
                    client
                );

                exit(0);
            }
        }
    }
}

void process_client(
    int32_t serverSocketNumber,
    uint8_t *buf,
    int32_t recv_len,
    Connection *client
)
{
    STATE state = START;

    int32_t data_file = 0;
    int32_t packet_len = 0;

    uint8_t packet[MAX_LEN];

    int32_t buf_size = 0;

    uint32_t seq_num = START_SEQ_NUM;

    while (state != DONE)
    {
        switch (state)
        {
            case START:
                state = FILENAME;
                break;

            case FILENAME:
                state = filename(
                    client,
                    buf,
                    recv_len,
                    &data_file,
                    &buf_size
                );
                break;

            case SEND_DATA:
                state = send_data(
                    client,
                    packet,
                    &packet_len,
                    data_file,
                    buf_size,
                    &seq_num
                );
                break;

            case WAIT_ON_ACK:
                state = wait_on_ack(client);
                break;

            case TIMEOUT_ON_ACK:
                state = timeout_on_ack(
                    client,
                    packet,
                    packet_len
                );
                break;

            case WAIT_ON_EOF_ACK:
                state = wait_on_eof_ack(client);
                break;

            case TIMEOUT_ON_EOF_ACK:
                state = timeout_on_eof_ack(
                    client,
                    packet,
                    packet_len
                );
                break;

            case DONE:
                break;

            default:
                printf(
                    "In default and you should not be here!!!!\n"
                );

                state = DONE;
                break;
        }
    }
}