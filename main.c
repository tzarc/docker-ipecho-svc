#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BACKLOG 32
#define MAX_PORTS 64

void sigchld_handler(int s)
{
    if(s == SIGINT)
    {
        exit(-1);
    }
    else if(s == SIGCHLD)
    {
        while(waitpid(-1, NULL, WNOHANG) > 0)
            ;
    }
}

int bind_to_port(const char* port)
{
    struct addrinfo hints, *res = NULL;
    int sockfd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if(getaddrinfo(NULL, port, &hints, &res) < 0)
    {
        fprintf(stderr, "getaddrinfo(...) failed\n");
        exit(8);
    }

    if((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
    {
        fprintf(stderr, "socket(...) failed\n");
        exit(9);
    }

    if(bind(sockfd, res->ai_addr, res->ai_addrlen) < 0)
    {
        fprintf(stderr, "bind(...) failed\n");
        exit(10);
    }

    if(listen(sockfd, BACKLOG) < 0)
    {
        fprintf(stderr, "listen(...) failed\n");
        exit(11);
    }

    return sockfd;
}

void process_connection(int client_socket, struct sockaddr_in6* client_sa)
{
    char outBuf[1024] = {0};
    char name[256] = {0};
    char port[256] = {0};
    int retLen = 0, maxLen = sizeof(name);
    const char* ret;
    socklen_t addrLen = sizeof(struct sockaddr_in6);
    if(getnameinfo((struct sockaddr*)client_sa, addrLen, name, sizeof(name) - 1, port, sizeof(port) - 1, NI_NUMERICHOST | NI_NUMERICSERV) < 0)
    {
        fprintf(stderr, "getnameinfo(...) failed\n");
        exit(12);
    }

    ret = name;
    if(strncmp(name, "::ffff:", 7) == 0)
    {
        ret = name + 7;
        maxLen -= 7;
    }

    retLen = strnlen(ret, maxLen);

    snprintf(outBuf,
             sizeof(outBuf) - 1,
             "HTTP/1.1 200 OK\r\n"
             "Server: ipecho-svc 1.0\r\n"
             "Connection: close\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Content-Length: %d\r\n"
             "Content-Type: text/plain; charset=UTF-8\r\n"
             "Echo-Remote-Address: %s\r\n"
             "Echo-Remote-Port: %s\r\n"
             "\r\n"
             "%s",
             retLen,
             ret,
             port,
             ret);

    if(write(client_socket, outBuf, strnlen(outBuf, sizeof(outBuf))) < 0)
    {
        fprintf(stderr, "write(...) failed\n");
        exit(13);
    }
}

int main(int argc, char* argv[])
{
    char *p, *e;
    char buf[128] = {0};
    int num_ports = 0;

    char *echo_uid, *echo_gid, *echo_ports;
    int server_sockets[MAX_PORTS] = {0};
    int fd_last;

    struct sigaction sig_action;
    sig_action.sa_handler = sigchld_handler;
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_flags = SA_RESTART;
    if(sigaction(SIGINT, &sig_action, NULL) < 0)
    {
        fprintf(stderr, "sigaction(SIGINT, ...) failed\n");
        exit(1);
    }
    if(sigaction(SIGCHLD, &sig_action, NULL) < 0)
    {
        fprintf(stderr, "sigaction(SIGCHLD, ...) failed\n");
        exit(1);
    }

    echo_gid = getenv("ECHO_GID");
    if(!echo_gid)
    {
        fprintf(stderr, "$ECHO_GID missing\n");
        exit(2);
    }

    echo_uid = getenv("ECHO_UID");
    if(!echo_uid)
    {
        fprintf(stderr, "$ECHO_UID missing\n");
        exit(3);
    }

    echo_ports = getenv("ECHO_PORTS");
    if(!echo_ports)
    {
        fprintf(stderr, "$ECHO_PORTS missing\n");
        exit(4);
    }

    gid_t gid = strtol(echo_gid, NULL, 10);
    if(setgid(gid) != 0)
    {
        fprintf(stderr, "setgid(...) failed\n");
        exit(5);
    }

    uid_t uid = strtol(echo_uid, NULL, 10);
    if(setuid(uid) != 0)
    {
        fprintf(stderr, "setuid(...) failed\n");
        exit(6);
    }

    strncpy(buf, echo_ports, sizeof(buf) - 1);
    p = e = buf;
    while(e && e < (buf + sizeof(buf)) && *e)
    {
        while(e && e < (buf + sizeof(buf)) && *e && isdigit(*e))
            ++e;
        *e = 0;
        server_sockets[num_ports] = bind_to_port(p);
        ++num_ports;
        ++e;
        p = e;
    }

    if(num_ports == 0)
        exit(14);

    while(1)
    {
        fd_set read_fds;
        int res;
        do
        {
            FD_ZERO(&read_fds);
            for(int i = 0; i < num_ports; ++i)
                FD_SET(server_sockets[i], &read_fds);
            fd_last = server_sockets[num_ports - 1] + 1;
            res = select(fd_last, &read_fds, NULL, NULL, NULL);
        } while(res < 0 && errno == EINTR);

        if(res < 0)
        {
            fprintf(stderr, "select(...) failed\n");
            exit(7);
        }

        for(int i = 0; i < num_ports; ++i)
        {
            if(FD_ISSET(server_sockets[i], &read_fds))
            {
                int client_socket = -1;
                struct sockaddr_in6 client_addr;
                memset(&client_addr, 0, sizeof(client_addr));
                socklen_t client_addr_size = sizeof(client_addr);
                if((client_socket = accept(server_sockets[i], (struct sockaddr*)&client_addr, &client_addr_size)) < 0)
                {
                    fprintf(stderr, "accept(...) failed\n");
                    continue;
                }

                if(!fork())
                {
                    close(server_sockets[i]);
                    process_connection(client_socket, &client_addr);
                    close(client_socket);
                    exit(0);
                }

                close(client_socket);
            }
        }
    }
    return 0;
}
