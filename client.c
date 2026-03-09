#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gsasl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

static int send_data(int sockfd, const char* data) {
    if (!data) return 0;
    
    int n = send(sockfd, data, strlen(data), 0);
    if (n < 0) {
        perror("send error");
        return -1;
    }
    return n;
}

static int recv_data(int sockfd, char* buffer, size_t buf_size) {
    memset(buffer, 0, buf_size);
    int n = recv(sockfd, buffer, buf_size - 1, 0);
    if (n < 0) {
        perror("recv error");
        return -1;
    }
    if (buffer[n - 1] == '\n') {
        buffer[n - 1] = '\0';
    }
    return n;
}

static void client_auth(Gsasl_session* session, int sockfd) {
    char buf[4096] = "";
    char* p = NULL;
    int rc;
    int step = 1;

    rc = gsasl_step64(session, "", &p);

    if (rc != GSASL_NEEDS_MORE || !p) {
        if (p) gsasl_free(p);
        return;
    }
    
    printf("Sends client-first: %s\n", p);
    if (send_data(sockfd, p) < 0) {
        gsasl_free(p);
        return;
    }
    gsasl_free(p);
    p = NULL;

    int n = recv_data(sockfd, buf, sizeof(buf));
    if (n <= 0) {
        return;
    }
    printf("Got server-first: %s\n", buf);

    rc = gsasl_step64(session, buf, &p);
    if (rc != GSASL_NEEDS_MORE || !p) {
        if (p) gsasl_free(p);
        return;
    }

    printf("Sends client-final: %s\n", p);
    if (send_data(sockfd, p) < 0) {
        gsasl_free(p);
        return;
    }
    gsasl_free(p);
    p = NULL;

    n = recv_data(sockfd, buf, sizeof(buf));
    if (n <= 0) {
        return;
    }

    printf("Got Server-final: %s\n", buf);
    rc = gsasl_step64(session, buf, &p);
    if (rc == GSASL_OK) {
        printf("\nAUTH GOOD!\n");
        
        const char* auth_user = gsasl_property_fast(session, GSASL_AUTHID);
        if (auth_user) {
            printf("GOOD USER: %s\n", auth_user);
        }
    } else {
        printf("\nAUTH FAIL: %s\n", gsasl_strerror(rc));
    }

    if (p) {
        gsasl_free(p);
        p = NULL;
    }
}

static void client(Gsasl *ctx, int sockfd) {
    Gsasl_session* session;
    const char* mech = "SCRAM-SHA-256";
    int rc;


    rc = gsasl_client_start(ctx, mech, &session);
    if (rc != GSASL_OK) {
        return;
    }

    rc = gsasl_property_set(session, GSASL_AUTHID, "ivan");
    if (rc != GSASL_OK) {
        gsasl_finish(session);
        return;
    }

    rc = gsasl_property_set(session, GSASL_PASSWORD, "12345");
    if (rc != GSASL_OK) {
        gsasl_finish(session);
        return;
    }

    client_auth(session, sockfd);

    gsasl_finish(session);
}

int main() {
    int sock_fd;
    struct sockaddr_in server_addr;
    Gsasl* ctx = NULL;
    int rc;


    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9999);

    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        close(sock_fd);
        return 1;
    }

    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock_fd);
        return 1;
    }

    rc = gsasl_init(&ctx);
    if (rc != GSASL_OK) {
        close(sock_fd);
        return 1;
    }

    client(ctx, sock_fd);

    gsasl_done(ctx);
    close(sock_fd);

    return 0;
}
