#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gsasl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct {
    const char* user;
    const char* pass;
} users_rec;

static users_rec users[] = {
    {"ivan", "12345"},
    {NULL, NULL}
};

static int server_callback(Gsasl* ctx, Gsasl_session* sess, Gsasl_property prop) {
    const char* authid = gsasl_property_fast(sess, GSASL_AUTHID);

    if (!authid) {
        return GSASL_NO_CALLBACK;
    }
    
    
    for (int i = 0; users[i].user != NULL; i++) {
        if (strcmp(authid, users[i].user) == 0) {
            switch (prop) {
                case GSASL_PASSWORD:
                    gsasl_property_set(sess, GSASL_PASSWORD, users[i].pass);
                    return GSASL_OK;
                default:
                    return GSASL_NO_CALLBACK;
            }
        }
    }
    
    return GSASL_NO_CALLBACK;
}

static int send_to_client(int client_fd, const char* data) {
    int n = send(client_fd, data, strlen(data), 0);
    if (n < 0) {
        perror("send error");
        return -1;
    }
    return n;
}

static int recv_from_client(int client_fd, char* buffer, size_t buf_size) {
    memset(buffer, 0, buf_size);
    int n = recv(client_fd, buffer, buf_size - 1, 0);
    if (n < 0) {
        perror("recv error");
        return -1;
    }
    if (n == 0) {
        return 0;
    }
    if (buffer[n - 1] == '\n') {
        buffer[n - 1] = '\0';
        n--;
    }
    return n;
}

static void handle_client(int client_fd) {
    Gsasl* ctx = NULL;
    Gsasl_session* session = NULL;
    char buf[4096] = "";
    char* p = NULL;
    int rc;

    if ((rc = gsasl_init(&ctx)) != GSASL_OK) {
        printf("GSASL init error: %s\n", gsasl_strerror(rc));
        close(client_fd);
        return;
    }
    
    gsasl_callback_set(ctx, server_callback);

    if ((rc = gsasl_server_start(ctx, "PLAIN", &session)) != GSASL_OK) {
        gsasl_done(ctx);
        close(client_fd);
        return;
    }

    do {
        int n = recv_from_client(client_fd, buf, sizeof(buf));
        if (n <= 0) {
            break;
        }
 printf("Got from client: %s\n",buf);

 // 64
        rc = gsasl_step64(session, buf, &p);
 printf("Server will send: %s\n",p);

        if (rc == GSASL_NEEDS_MORE || rc == GSASL_OK) {
            if (p && strlen(p) > 0) {
                if (send_to_client(client_fd, p) < 0) {
                    gsasl_free(p);
                    break;
                }
            }
            if (p) {
                gsasl_free(p);
                p = NULL;
            }
        }

    } while (rc == GSASL_NEEDS_MORE);

    if (rc == GSASL_OK) {
        printf("AUTH GOOD!\n");
        
    } else {
        printf("AUTH FAIL: %s\n", gsasl_strerror(rc));
    }

    if (session) gsasl_finish(session);
    if (ctx) gsasl_done(ctx);
    close(client_fd);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(9999);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 4) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }


    client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
        perror("accept");
        close(server_fd);
        return 1;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    handle_client(client_fd);

    close(server_fd);
    return 0;
}
