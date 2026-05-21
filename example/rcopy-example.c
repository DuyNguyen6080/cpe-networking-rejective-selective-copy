enum State
{
    DONE,
    FILENAME,
    RECV_DATA,
    FILE_OK,
    START_STATE
};

void processFile(char argv[]);
STATE start_state(char **argv, Connection *server, uint32_t *clientSeqNum);
STATE recv_data(int32_t output_file, Connection *server, uint32_t *clientSeqNum);
STATE file_ok(int *outputFiled, char *outputFileName);
void check_args(int arg, char **argv);

int main(int argc, char *argv[])
{
    check_args(argc, argv);

    sendtoErr_init(
        atof(argv[4]),
        DROP_ON,
        FLIP_ON,
        DEBUG_ON,
        RSEED_ON
    );

    processFile(argv);

    return 0;
}

void processFile(char *argv[])
{
    // argv needed to get file names, server name and server port number

    Connection *server = (Connection *)calloc(1, sizeof(Connection));

    int32_t clientSeqNum = 0;
    int32_t output_file_fd = 0;

    STATE state = START_STATE;

    while (state != DONE)
    {
        switch (state)
        {
            case START_STATE:
                state = start_state(argv, server, &clientSeqNum);
                break;

            case FILENAME:
                state = filename(argv[1], atol(argv[3]), server);
                break;

            case FILE_OK:
                state = file_ok(&output_file_fd, argv[2]);
                break;

            case RECV_DATA:
                state = recv_data(
                    output_file_fd,
                    server,
                    &clientSeqNum
                );
                break;

            case DONE:
                break;

            default:
                printf("ERROR - in default state\n");
                break;
        }
    }
}
