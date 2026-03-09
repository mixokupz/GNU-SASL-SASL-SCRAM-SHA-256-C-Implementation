#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gsasl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

static int send_data(int sockfd, const char* data){
 int n = send(sockfd,data,strlen(data),0);
 if(n<0){
  printf("error");
  return -1;
 }
 return n;
}
static int recv_data(int sockfd, char* buffer, size_t buf_size){
 int n = recv(sockfd,buffer,buf_size-1,0);
 if(n<0){
  printf("error");
  return -1; 
 }
 if(buffer[n-1]=='\n') buffer[n-1]='\0';
 return n;
}

static void client_auth(Gsasl_session* session, int sockfd){

 char buf[10000] = "";
 char* p;
 int rc;

 do{
  rc = gsasl_step64(session,buf,&p);
  if(rc == GSASL_NEEDS_MORE || rc==GSASL_OK){
   printf("Output: %s\n",p);
  }
  printf("Client send: %s\n",p);
  if(send_data(sockfd,p)<0){
   gsasl_free(p);
   return;
  }
  gsasl_free(p);

  if(rc == GSASL_NEEDS_MORE){
   if(recv_data(sockfd,buf,sizeof(buf))<=0){
    return;
   } 
  }
 
 }while(rc==GSASL_NEEDS_MORE);
 p = buf;


}

static void client(Gsasl *ctx, int sockfd){
 Gsasl_session* session;
 const char* mech = "PLAIN";
 int rc;

 if((rc = gsasl_client_start(ctx,mech,&session)) != GSASL_OK){
  printf("error\n");
  return;
 }

 rc = gsasl_property_set(session,GSASL_AUTHID,"ivan");
 if(rc!=GSASL_OK){
  printf("error\n");
  return;
 }
 rc = gsasl_property_set(session,GSASL_PASSWORD,"12345");
 if(rc!=GSASL_OK){
  printf("error\n");
  return;
 }

 client_auth(session,sockfd);
 gsasl_finish(session);

}

int main(){

 int sock_fd;
 struct sockaddr_in server_addr;
 char buffer[10000];

 sock_fd = socket(AF_INET, SOCK_STREAM, 0);
 memset(&server_addr, 0,sizeof(server_addr));
 server_addr.sin_family = AF_INET;
 server_addr.sin_port = htons(9999);

 if(inet_pton(AF_INET,"127.0.0.1",&server_addr.sin_addr)<=0){
  printf("error\n");
  close(sock_fd);
  exit(1);
 }
 if(connect(sock_fd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0){
  printf("aaaerror\n");
  close(sock_fd);
  exit(1);
 }

 Gsasl* ctx = NULL;
 int rc;

 //init library
 if(rc = gsasl_init(&ctx) != GSASL_OK){
  printf("Init error: %s\n",gsasl_strerror(rc));
  return 1;
 }

 client(ctx,sock_fd);

 gsasl_done(ctx);
 close(sock_fd);

 return 0;
}
