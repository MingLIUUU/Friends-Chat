#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "friends.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#ifndef PORT
  #define PORT 56403
#endif
#define MAX_BACKLOG 5
#define BUF_SIZE 128
#define INPUT_BUFFER_SIZE 256
#define INPUT_ARG_MAX_NUM 12
#define DELIM " \n"

struct sockname {
    int sock_fd;
    char *username; 
    char buf[BUF_SIZE]; // buffer string used for receive message from client
    int inbuf;    // How many bytes currently in buffer?
    int room;     // How many bytes remaining in buffer?
    char *after;  // Pointer to position after the data in buf
    int line_rcv; // number of lines that received from client
    struct sockname *next;
};


/* 
 * Send a message msg to client with client_fd.
 * Did not check for network newline.
 */
void msg_send(char *msg, int client_fd) {
    if (send(client_fd, msg, strlen(msg), 0) < 0) {
        perror("send");
    }
}


/* 
 * Read and process commands
 * Return:  -1 for quit command
 *          0 otherwise
 */
int process_args(int cmd_argc, char **cmd_argv, User **user_list_ptr, 
struct sockname *client, struct sockname **conn_list) {
    User *user_list = *user_list_ptr;
    int client_fd = client->sock_fd;
    // no command
    if (cmd_argc <= 0) {
        return 0;
    // "quit" command
    } else if (strcmp(cmd_argv[0], "quit") == 0 && cmd_argc == 1) {
        return -1;
    // "list_users" command
    } else if (strcmp(cmd_argv[0], "list_users") == 0 && cmd_argc == 1) {
        char *buf = list_users(user_list);
        if (send(client_fd, buf, strlen(buf), 0) < 0) {
            perror("send");
        }
        free(buf);
    // "make_friends [target]" command
    } else if (strcmp(cmd_argv[0], "make_friends") == 0 && cmd_argc == 2) {
        char *result = NULL;
        int ret;
        switch (make_friends(client->username, cmd_argv[1], user_list)) {
            case 0:
                ret = asprintf(&result, "You are now friends with %s\r\n", cmd_argv[1]);
                if (ret == -1) {
                    perror("asprintf");
                } 
                msg_send(result, client_fd);
                break;
            case 1:
                msg_send("You are already friends\r\n", client_fd);
                break;
            case 2:
                msg_send("At least one of you entered has the max number of friends\r\n", client_fd);
                break;
            case 3:
                msg_send("You can't friend yourself\r\n", client_fd);
                break;
            case 4:
                msg_send("The user you entered does not exist\r\n", client_fd);
                break;
        }
    // "post [target] [msgpiece ...]" command
    } else if (strcmp(cmd_argv[0], "post") == 0 && cmd_argc >= 3) {
        // first determine how long a string we need
        int space_needed = 0;
        for (int i = 2; i < cmd_argc; i++) {
            space_needed += strlen(cmd_argv[i]) + 1;
        }
        // allocate the space
        char *contents = malloc(space_needed * sizeof(char));
        if (contents == NULL) {
            perror("malloc");
            exit(1);
        }
        // copy in the bits to make a single string
        strcpy(contents, cmd_argv[2]);
        for (int i = 3; i < cmd_argc; i++) {
            strcat(contents, " ");
            strcat(contents, cmd_argv[i]);
        }
        User *author = find_user(client->username, user_list);
        User *target = find_user(cmd_argv[1], user_list);
        switch (make_post(author, target, contents)) {
            case 0: {
                char *result = NULL;
                int ret = asprintf(&result, "From %s: %s\r\n", client->username, contents);
                if (ret == -1) {
                    perror("asprintf");
                } 
                struct sockname *curr = *conn_list;
                while (curr != NULL) {
                    if (curr->username != NULL 
                    && strcmp(curr->username, cmd_argv[1]) == 0) {
                        msg_send(result, curr->sock_fd);
                    }
                    curr = curr->next;                
                }
            }
                break;
            case 1:
                msg_send("You can only post to your friends\r\n", client_fd);
                break;
            case 2:
                msg_send("The user you want to post to does not exist\r\n", client_fd);
                break;
        }
    // "profile [username]" command
    } else if (strcmp(cmd_argv[0], "profile") == 0 && cmd_argc == 2) {
        User *user = find_user(cmd_argv[1], user_list);
        if (print_user(user) == NULL) {
            msg_send("user not found\r\n", client_fd);
        } else {
            char *result = print_user(user);
            if (send(client_fd, result, strlen(result), 0) < 0) {
                perror("send");
            }
            free(result);
        }
    // incorrect syntax
    } else {
        msg_send("Incorrect syntax\r\n", client_fd);
    }
    return 0;
}


/*
 * Tokenize the string stored in cmd.
 * Return the number of tokens, and store the tokens in cmd_argv.
 */
int tokenize(char *cmd, char **cmd_argv, int client_fd) {
    int cmd_argc = 0;
    char *next_token = strtok(cmd, DELIM);    
    while (next_token != NULL) {
        if (cmd_argc >= INPUT_ARG_MAX_NUM - 1) {
            msg_send("Too many arguments!\r\n", client_fd);
            cmd_argc = 0;
            break;
        }
        cmd_argv[cmd_argc] = next_token;
        cmd_argc++;
        next_token = strtok(NULL, DELIM);
    }
    return cmd_argc;
}


/*
 * Search the first n characters of buf for a network newline (\r\n).
 * Return one plus the index of the '\n' of the first network newline,
 * or -1 if no network newline is found. The return value is the index into buf
 * where the current line ends.
 */
int find_network_newline(const char *buf, int n) {
    int i = 0;
    if (n < 1) {return -1;}
    while (i < n && buf[i] != '\r') {
        i++;
    }
    if (i + 1 < n && buf[i] == '\r' && buf[i+1] == '\n') {
        return i+2;
    }
    return -1;
}

/*
 * Add the username in buf to the connection of client.
 * Create a new user if username not in user_list.
 * Send diffrent greeting messages for new and old user.
 * truncate username if longer than MAX_NAME.
 */
void regist_name(int where, char *buf, struct sockname *client, User **user_list_ptr) {
    char *username = buf;
    int limit = MAX_NAME -1;
    // try create new user
    int i = create_user((const char *)buf, user_list_ptr);
    // username too long
    if (i == 2) {
        char *result;
        int ret = asprintf(&result, 
        "Username too long, truncated to %d chars.\r\n", limit);
        if (ret == -1) {
            perror("asprintf");
        } 
        msg_send(result, client->sock_fd);
        strncpy(username, buf, limit);
        username[MAX_NAME] = '\0';
        i = create_user((const char *)username, user_list_ptr);
    // valid length username
    } else {
        int len = where -2;
        strncpy(username, buf, len);
        username[len] = '\0';
    }
    // different greeting messages for new and old user
    if (i == 0) {
        msg_send("Welcome.\r\n", client->sock_fd);
    } else if (i == 1) {
        msg_send("Welcome back.\r\n", client->sock_fd);
    }
    // allocate space for username on client
    client->username = malloc(sizeof(char) * (strlen(username) + 1));
    if (client->username == NULL) {
        perror("malloc");
        exit(1);
    }
    // set username on client connection
    strncpy(client->username, username, strlen(username) + 1);
    // send greenting message
    msg_send("Go ahead and enter user commands>\r\n", client->sock_fd);
}


/*
 * Read a message from client_index and returns it
 * Return -1 if it has been closed or 0 otherwise.
 */
int read_from(struct sockname *client, struct sockname **conn_list_ptr, User **user_list_ptr) {
        int client_fd = client->sock_fd;
        // Receive messages use buf, after, room, inbuf in client

        int nbytes;
        if ((nbytes = read(client_fd, client->after, client->room)) > 0) {
            // update inbuf to bytes were just added
            if (nbytes == -1) {
                perror("server: read");
                exit(1);
            } else if (nbytes > 0) {
                client->inbuf += nbytes;
                client->room = BUF_SIZE - client->inbuf;
            }
            int where;
            // determine if a full line has been read from the client
            // use a loop here because a single read might result in
            // more than one full line.
            if ((where = find_network_newline(client->buf, client->inbuf)) > 0) {
				// where is the index into buf immediately after the first network newline
                // excluding the "\r\n"
                client->buf[where-2] = '\0';
                // regist username of connection
                if (client->line_rcv == 0) {
                    regist_name(where, client->buf, client, user_list_ptr);
                } else {
                    // pass the full line to process_arg() using tokenize()
                    char *msg = client->buf;
                    char *cmd_argv[INPUT_ARG_MAX_NUM];
                    int cmd_argc = tokenize(msg, cmd_argv, client_fd);
                    int res = process_args(cmd_argc, cmd_argv, user_list_ptr, client, conn_list_ptr);
                    // if read "quit" command 
                    if (cmd_argc == 1 && res == -1) {return -1;}
                }
                client->line_rcv += 1;
                // update inbuf and remove the full line from the buffer
                client->inbuf -= where;
                // move the stuff after the full line to the beginning of the buffer. 
                memmove(client->buf, &(client->buf[where]), client->inbuf);
            }
            // update after and room, in preparation for the next read.
            client->after = &(client->buf[client->inbuf]);
            client->room = BUF_SIZE - client->inbuf;
        }
    return 0;
}


int main(void) {
    struct sockname *conn_list = NULL;
    User *user_list = NULL;

    // Create the socket FD.
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("server: socket");
        exit(1);
    }

    // Set information about the port (and IP) we want to be connected to.
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    // This sets an option on the socket so that its port can be reused right
    // away. Since you are likely to run, stop, edit, compile and rerun your
    // server fairly quickly, this will mean you can reuse the same port.
    int on = 1;
    int status = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR,
                            (const char *) &on, sizeof(on));
    if (status == -1) {
        perror("setsockopt -- REUSEADDR");
    }

    // This should always be zero. On some systems, it won't error if you
    // forget, but on others, you'll get mysterious errors. So zero it.
    memset(&server.sin_zero, 0, 8);

    // Bind the selected port to the socket.
    if (bind(sock_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("server: bind");
        close(sock_fd);
        exit(1);
    }

    // Announce willingness to accept connections on this socket.
    if (listen(sock_fd, MAX_BACKLOG) < 0) {
        perror("server: listen");
        close(sock_fd);
        exit(1);
    }
    
while (1) {
    // The client accept - message accept loop. First, we prepare to listen to multiple
    // file descriptors by initializing a set of file descriptors.
    fd_set all_fds;
    int max_fd = sock_fd;
    FD_ZERO(&all_fds);
    FD_SET(sock_fd, &all_fds);
    struct sockname *curr;

    // Add all existing connections to the all_fds set
    for (curr = conn_list; curr; curr = curr->next) {
        FD_SET(curr->sock_fd, &all_fds);
        if (curr->sock_fd > max_fd) {max_fd = curr->sock_fd;}
    }
    // Call select to wait for events on any of the file descriptors
    if (select(max_fd + 1, &all_fds, NULL, NULL, NULL) < 0) {
        perror("select");
    }

    // Check if there is a new connection waiting on the sock_fd
    if (FD_ISSET(sock_fd, &all_fds)) {
        int fd;
        struct sockaddr_in r;
        socklen_t len = sizeof r;
        if ((fd = accept(sock_fd, (struct sockaddr *)&r, &len)) < 0) {
            perror("server: accept");
        }
        printf("Accepted connection %d\n", fd);
        fflush(stdout);
        struct sockname *client = malloc(sizeof(struct sockname));
        if (!client) {
            perror("server: malloc");
            exit(1);
        }
        // initialize new connection
        client->sock_fd = fd;
        client->username = NULL;
        client->buf[0] = '\0';
        client->inbuf = 0;                    
        client->room = sizeof(client->buf);   
        client->after = client->buf;          
        client->line_rcv = 0;
        client->next = conn_list;
        conn_list = client;

        // send message to ask for username
        char *msg = "What is your user name?\r\n";
        msg_send(msg, client->sock_fd);
    }

    // Check for incoming data on each connection
    for (curr = conn_list; curr; curr = curr->next) {
        if (FD_ISSET(curr->sock_fd, &all_fds)) {break;}
    }
    if (curr) {
        int status = read_from(curr, &conn_list, &user_list);
        // receive "quit"
        // disconnect the connection
        if (status == -1) {
            close(curr->sock_fd);
            printf("Disconnecting connection %d\n", curr->sock_fd);
            fflush(stdout);
            struct sockname **pp = &conn_list;
            // remove disconnected sockename node and free
            while (*pp && *pp != curr) {pp = &(*pp)->next;}
            if (*pp) {
                if ((*pp)->username) {free((*pp)->username);}
                struct sockname *t = (*pp)->next;
                free(*pp);
                *pp = t;
            } else {
                fprintf(stderr, "Trying to remove fd %d unsuccessful\n", curr->sock_fd);
                fflush(stderr);
            }
        }
    }
}
    // Should never get here.
    return 1;
}