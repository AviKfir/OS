#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <cstring>

#define MAXHOSTNAME 256
#define BUFSIZE 256
#define FAILURE 1
#define MAX_OF_CONNECTS 5

#define MSG_ACCEPT "system error: accept failed\n"
#define MSG_READ "system error: read failed\n"
#define MSG_GETHOSTNAME "system error: gethostname failed\n"
#define MSG_GETHOSTBYNAME "system error: gethostbyname failed\n"
#define MSG_SOCKET "system error: socket failed\n"
#define MSG_BIND "system error: bind failed\n"
#define MSG_CONNECT "system error: connect failed\n"
#define MSG_WRITE "system error: write failed\n"
#define MSG_SYSTEM "system error: system failed\n"

/**
 * this function accept request from client to connect to the server.
 */
int get_connection(int s) {
    int t; /* socket of connection */
    if ((t = accept(s, nullptr, nullptr)) < 0) {
        fprintf (stderr, MSG_ACCEPT);
        exit (FAILURE);
    }
    return t;
}

/**
 * this function read into the buf data from file (up to n bytes).
 */
void read_data(int s, char *buf, int n) {
    int bcount; // counts bytes read
    int br; // bytes read this pass
    bcount= 0; br= 0;

    while (bcount < n) { // loop until full buffer
        br = read(s, buf, n - bcount);
        if (br > 0)  {
            bcount += br;
            buf += br;
        }
        if (br == 0) {
            break;
        }
        if (br == -1) {
            fprintf (stderr, MSG_READ);
            exit(FAILURE);
        }
    }
}

/**
 * this function establish a server- creates a socket, bind it and initialize the max client number to listen to.
 * @param port_num port number for the connection between server and clients
 * @return number of socket on success, exit(-1) if fails
 */
int server(long port_num) {
    char myname[MAXHOSTNAME + 1];
    int s;
    struct sockaddr_in sa;
    struct hostent *hp;

    //hostnet initialization
    if (gethostname(myname, MAXHOSTNAME) == -1) {
        fprintf (stderr, MSG_GETHOSTNAME);
        exit (FAILURE);
    }
    hp = gethostbyname(myname);
    if (hp == nullptr) {
        fprintf (stderr, MSG_GETHOSTBYNAME);
        exit (FAILURE);
    }
    //sockaddrr_in initlization
    memset(&sa, 0, sizeof(struct sockaddr_in));
    sa.sin_family = hp->h_addrtype;
    /* this is our host address */
    memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
    /* this is our port number */
    sa.sin_port= htons(port_num);

    /* create socket */
    if ((s= socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf (stderr, MSG_SOCKET);
        exit (FAILURE);
    }

    if (bind(s , (struct sockaddr *)&sa , sizeof(struct sockaddr_in)) < 0) {
        close(s);
        fprintf (stderr, MSG_BIND);
        exit (FAILURE);
    }
    
    listen(s, MAX_OF_CONNECTS); /* max num of queued connects */

    return(s);
}

/**
 * this function establish a client- creates sockets and connect between it to a server.
 * @param hostname the host name of the client.
 * @param portnum port number for the connection between server and clients.
 * @return number of socket on success, exit(-1) if fails.
 */
int client(char *hostname, unsigned short portnum) {
    struct sockaddr_in sa;
    struct hostent *hp;
    int s;

    if ((hp= gethostbyname (hostname)) == nullptr) {
        fprintf (stderr, MSG_GETHOSTBYNAME);
        exit (FAILURE);
    }
    memset(&sa,0,sizeof(sa));
    memcpy((char *)&sa.sin_addr , hp->h_addr ,
           hp->h_length);
    sa.sin_family = hp->h_addrtype;
    sa.sin_port = htons((u_short)portnum);

    if ((s = socket(hp->h_addrtype,
                    SOCK_STREAM,0)) < 0) {
        fprintf (stderr, MSG_SOCKET);
        exit (FAILURE);
    }

    if (connect(s, (struct sockaddr *)&sa , sizeof(sa)) < 0) {
        close(s);
        fprintf (stderr, MSG_CONNECT);
        exit (FAILURE);
    }

    return(s);
}

/**
 * this function creates server/ client.
 * if server- run received terminal commands from port.
 * if client - send terminal command to server.
 * @param argc number of args in command line.
 * @param argv args from command line.
 * @return 0 on success, exit(-1) on failure.
 */
int main(int argc, char* argv[]) {
    std::string arg1 = argv[1];
    long num_port = strtol(argv[2], nullptr, 10);
    if (arg1 == "client") {
        //client
        char host[MAXHOSTNAME + 1];
        if (gethostname(host, MAXHOSTNAME) == -1) {
            fprintf (stderr, MSG_GETHOSTNAME);
            exit (FAILURE);
        }
        int t = client(host, num_port);
        if (write(t, argv[3], BUFSIZE) == -1) {
            fprintf (stderr, MSG_WRITE);
            exit (FAILURE);
        }
    }
    else {
        //server
        char buf[BUFSIZE];
        int s = server(num_port);
        while (true)
        {
            int t = get_connection(s);
            read_data(t, buf, BUFSIZE);
            close(t);
            if (system(buf) == -1) {
                fprintf (stderr, MSG_SYSTEM);
                exit (FAILURE);
            }
            bzero(buf, sizeof (buf));
        }
    }
    return 0;
}
