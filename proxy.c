#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
/* You won't lose style points for including this long line in your code */

static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void doit(int fd);
void read_requesthdrs(rio_t *rp);
void do_req(int svr_connfd, char *method, char *uri_p, char *hostname);
void do_resp(int fd, int svr_connfd);

// 1. http1.0 GET
int main(int argc, char **argv)
{
  int cli_listenfd;
  int cli_connfd;

  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  printf("%s", user_agent_hdr);

  // cli의 호스트, 포트 저장용
  char hostname[MAXLINE], port[MAXLINE];

  // argc는 인자의 갯수를 나타냄. argc!=2면 (ip,port)가 들어온게 아니므로 exit
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // Open_listenfd는 getaddrinfo, socket, bind, listen 기능을 순차적으로 실행하는 함수
  cli_listenfd = Open_listenfd(argv[1]); // listen 소켓 오픈

  // 무한 while문으로 서버 항상 열어놓음.(무한 서버 루프)
  while (1)
  {
    clientlen = sizeof(clientaddr);

    // 클라이언트 연결 수락 하고 클라 정보 얻어옴
    cli_connfd = Accept(cli_listenfd, (SA *)&clientaddr, &clientlen);

    // clientaddr의 구조체에 대응되는 hostname, port를 작성한다.=
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    doit(cli_connfd);
    Close(cli_connfd);
  }
  return 0;
}

void doit(int fd)
{
  rio_t cli_rio, svr_rio;
  int is_static;
  int svr_connfd;
  // 이게 다 maxline이면 메모리 낭빈데 짧은 숫자는 int로 해도?
  // port를 int로 바꿔보자 나중에
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE],
      uri_p[MAXLINE], hostname[MAXLINE], port[MAXLINE], buf[MAXLINE], body[MAXBUF];

  // 식별자 fd를 rio_t타입의 empty한 읽기버퍼 rio와 연결
  rio_readinitb(&cli_rio, fd); // CSAPP pic10.8 참조

  // 파일 디스크립터에서 한 줄씩 읽는 동작을 함.
  // fd에 아무것도 없으면 return
  if (!rio_readlineb(&cli_rio, buf, MAXLINE))
    return;

  printf("REQUESTED HEADERS: \n");
  printf("%s", buf);

  // buf의 내용을 각각 method, uri, ver에 저장함.
  sscanf(buf, "%s %s %s", method, uri, version);

  // error 검증시간~
  // err code는 https://datatracker.ietf.org/doc/html/rfc1945 참고함.
  if ((strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))) // method로 GET말고 다른거 받으면 에러 반환
  {
    clienterror(fd, method, "501", "not implemented", "Tiny couldn't implement this method");
    return;
  }

  if (strlen(uri) == 0 || (strchr(uri, '/') == NULL)) // uri 잘못됐으면
  {
    clienterror(fd, method, "400", "Bad Request", "The request could not be understood by the server due to malformed syntax");
    return;
  }

  if (strlen(version) == 0 || (strchr(version, '/') == NULL)) // version 잘못됐으면
  {
    clienterror(fd, method, "400", "Bad Request", "The request could not be understood by the server due to malformed syntax");
    return;
  }

  read_requesthdrs(&cli_rio);

  // uri파싱해서 server에 보내야함
  parse_uri(uri, uri_p, hostname, port);

  // server에 전송
  svr_connfd = open_clientfd(hostname, port);

  // cli에서 받은 데이터를 server에 전송
  do_req(svr_connfd, method, uri_p, hostname);

  // server에서 받은 데이터를 cli에 전송
  do_resp(fd, svr_connfd);
  close(svr_connfd);
}

// 여러줄 요구한거 무시용
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
}

void do_req(int svr_connfd, char *method, char *uri_p, char *hostname)
{
  char proxy_header[MAXLINE];

  printf("Request headers to server: \n");
  printf("%s %s %s\n", method, uri_p, "HTTP/1.0");

  sprintf(proxy_header, "GET %s HTTP/1.0\r\n", uri_p);
  sprintf(proxy_header, "%sHost: %s\r\n", proxy_header, hostname);          // Host: www.google.com
  sprintf(proxy_header, "%s%s", proxy_header, user_agent_hdr);              // User-Agent: Mozilla/5.0 (X11;...
  sprintf(proxy_header, "%sConnections: close\r\n", proxy_header);          // Connections: close
  sprintf(proxy_header, "%sProxy-Connection: close\r\n\r\n", proxy_header); // Proxy-Connection: close

  Rio_writen(svr_connfd, proxy_header, (size_t)strlen(proxy_header));
}

void do_resp(int fd, int svr_connfd)
{
}
