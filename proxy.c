#include <stdio.h>
#include "csapp.h"
//////////////////////////////////////전역변수 선언부/////////////////////////////////////
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_header =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

//////////////////////////////////////함수 선언부/////////////////////////////////////
void doit(int connfd);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void make_http_header(char *http_header, char *hostname, char *path, char *port, rio_t *client_rio);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void *thread(void *vargp);

//////////////////////////////////////코드 시작/////////////////////////////////////
int main(int argc, char **argv)
{
  int listenfd, *connfdp;
  socklen_t clientlen;
  char hostname[MAXLINE], port[MAXLINE];
  struct sockaddr_storage clientaddr;
  pthread_t tid;
  printf("%s", user_agent_header);

  // argc는 인자의 갯수를 나타냄. argc!=2면 (ip,port)가 들어온게 아니므로 exit
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // Open_listenfd는 getaddrinfo, socket, bind, listen 기능을 순차적으로 실행하는 함수
  listenfd = Open_listenfd(argv[1]);

  // 무한 while문으로 서버 항상 열어놓음.(무한 서버 루프)
  while (1)
  {
    clientlen = sizeof(clientaddr);

    // 클라이언트 연결 수락 하고 클라 정보 얻어옴
    connfdp = Malloc(sizeof(int));
    *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    // clientaddr의 구조체에 대응되는 hostname, port를 작성한다.=
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    Pthread_create(&tid, NULL, thread, connfdp);
  }
  return 0;
}

void *thread(void *vargp)
{
  int connfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  doit(connfd);
  Close(connfd);
  return NULL;
}

void doit(int connfd)
{
  rio_t cli_rio, svr_rio;
  int svr_connfd, port;
  // 이게 다 maxline이면 메모리 낭빈데 짧은 숫자는 int로 해도?
  // port를 int로 바꿔보자 나중에
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE],
      hostname[MAXLINE], buf[MAXLINE], path[MAXLINE], http_header[MAXLINE];

  // 식별자 fd를 rio_t타입의 empty한 읽기버퍼 rio와 연결
  rio_readinitb(&cli_rio, connfd); // CSAPP pic10.8 참조

  // 파일 디스크립터에서 한 줄 읽음.
  // fd에 아무것도 없으면 return
  if (!rio_readlineb(&cli_rio, buf, MAXLINE))
    return;

  printf("REQUESTED HEADERS: \n");
  printf("%s", buf);

  // buf의 내용을 각각 method, uri, ver에 저장함.
  // http://%s << 이렇게하면 %s에 http:// 뒤부터 들어감
  sscanf(buf, "%s http://%s %s", method, uri, version);

  // error 검증시간~
  // err code는 https://datatracker.ietf.org/doc/html/rfc1945 참고함.
  if ((strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))) // method로 GET말고 다른거 받으면 에러 반환
  {
    clienterror(connfd, method, "501", "not implemented", "Tiny couldn't implement this method");
    return;
  }

  if (strlen(uri) == 0 || (strchr(uri, '/') == NULL)) // uri 잘못됐으면
  {
    clienterror(connfd, method, "400", "Bad Request", "The request could not be understood by the server due to malformed syntax");
    return;
  }

  if (strlen(version) == 0 || (strchr(version, '/') == NULL)) // version 잘못됐으면
  {
    clienterror(connfd, method, "400", "Bad Request", "The request could not be understood by the server due to malformed syntax");
    return;
  }

  // uri파싱해서 server에 보내야함
  parse_uri(uri, hostname, path, &port);

  // int port를 str로 바꿔서 Open_clientfd함
  char port_to_str[100];
  sprintf(port_to_str, "%d", port);
  svr_connfd = Open_clientfd(hostname, port_to_str);

  // Open_clientfd가 실패하면
  if (svr_connfd < 0)
  {
    printf("connection failed\n");
    return;
  }

  // server에 header보냄
  make_http_header(http_header, hostname, path, port, &cli_rio);
  Rio_readinitb(&svr_rio, svr_connfd);
  Rio_writen(svr_connfd, http_header, strlen(http_header));

  //  \r\n\r\n 나올때까지 read함
  int n;
  while ((n = Rio_readlineb(&svr_rio, buf, MAXLINE)) != 0)
  {
    printf(" %d bytes send\n", n); // echo
    Rio_writen(connfd, buf, n);
  }

  Close(svr_connfd);
}

// 헤더 만들기
void make_http_header(char *http_header, char *hostname, char *path, char *port, rio_t *client_rio)
{
  char buf[MAXLINE], request_header[MAXLINE], other_header[MAXLINE], host_header[MAXLINE];

  // 인자 합쳐줌
  sprintf(request_header, "GET %s HTTP/1.0\r\n", path);

  while (Rio_readlineb(client_rio, buf, MAXLINE) > 0)
  {
    if (!strcmp(buf, "\r\n")) /// EOF판별
      break;

    if (!strncasecmp(buf, "HOST", strlen("HOST")))
    {
      strcpy(host_header, buf);
      continue;
    }
  }

  if (strlen(host_header) == 0)
    sprintf(host_header, "Host: %s\r\n", hostname);

  sprintf(http_header, "%s%s%s%s%s%s",
          request_header,
          host_header,
          "Connection: close\r\n",
          "Proxy-Connection: close\r\n",
          user_agent_header,
          "\r\n");

  return;
}

// 에러 메시지를 클라이언트에게 전송하는 함수
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF]; // HTTP 헤더와 HTML 본문을 저장할 버퍼

  // HTML의 body버퍼에 내용 추가
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  // 클라이언트에게 HTTP 응답 헤더 작성 시작
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);

  // 헤더 정보를 소켓을 통해 클라이언트에 전송
  Rio_writen(fd, buf, strlen(buf));

  // 컨텐츠 타입 HTML임
  sprintf(buf, "Content-type: text/html\r\n");

  // 헤더 정보를 소켓을 통해 클라이언트에 전송
  Rio_writen(fd, buf, strlen(buf));

  // HTML 본문의 길이.. 하고 끝냄
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));

  // 헤더 정보와 HTML 본문을 클라이언트에 전송
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void parse_uri(char *uri, char *hostname, char *path, int *port)
{
  *port = 80; // 기본 HTTP 포트 설정
  char *p;

  // doit_line:88 참조
  p = uri; // "http://"이 없으면 처음부터 시작

  // '/'는 int다...
  char *p2 = strstr(p, ":");
  if (p2 != NULL)
  {
    *p2 = '\0';                         // 포트 번호 시작 지점에서 문자열 분리
    sscanf(p, "%s", hostname);          // 호스트네임 파싱
    sscanf(p2 + 1, "%d%s", port, path); // 포트 번호와 나머지 경로(path) 파싱
    // strcat(hostname, path);               // 이게아닌갑네
  }
  else
  {
    p2 = strstr(p, "/");

    if (p2 != NULL)
    {
      *p2 = '\0';                // 경로 시작 지점에서 문자열 분리
      sscanf(p, "%s", hostname); // 호스트네임 파싱
      *p2 = '/';                 // 원래 문자열 복원
      sscanf(p2, "%s", path);    // 경로 파싱
    }

    else
      sscanf(p, "%s", hostname); // 호스트네임만 있을 경우 파싱
  }
  return;
}
