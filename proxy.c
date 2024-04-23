//////////////////////////////////////주의사항//////////////////////////////////////
/* 실행 전 반드시 proxy.c, proxy_cache.h 두 파일을 전부 받아주세요.*/
#include "proxy_cache.h"
#include <stdio.h>

//////////////////////////////////////전역변수 선언부//////////////////////////////////////
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_header =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

//////////////////////////////////////함수 선언부//////////////////////////////////////
void *thread(void *vargp);
void doit(int connfd);
void parse_uri(char *uri, char *hostname, char *path, char *port);
void make_http_header(char *http_header, char *hostname, char *path, rio_t *client_rio);
void error_find(char *method, char *uri, char *version, int fd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

//////////////////////////////////////코드 시작//////////////////////////////////////

int main(int argc, char **argv)
{
  int listenfd, *cli_connfd;
  char hostname[MAXLINE], port[MAXLINE];
  struct sockaddr_storage clientaddr;
  socklen_t clientlen = sizeof(clientaddr);
  pthread_t tid;
  printf("%s", user_agent_header);

  // argc는 인자의 갯수를 나타냄. argc!=2면 (ip,port)가 제대로 들어온게 아니므로 exit
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // Open_listenfd는 getaddrinfo, socket, bind, listen 기능을 순차적으로 실행하는 함수
  // listen 소켓 엶
  listenfd = Open_listenfd(argv[1]);
  
  //캐시 리스트 초기화
  init_cache();
  
  // 무한 while문으로 서버 항상 열어놓음.(무한 서버 루프)
  while (1)
  {
    // 클라이언트와 통신하는 소켓 만들고 정보 얻음
    cli_connfd = Malloc(sizeof(int));
    *cli_connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    // clientaddr의 구조체에 대응되는 hostname, port를 작성
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    // 쓰레드 생성해서 tid와 연결
    // tid에 연결된 쓰레드로 thread함수를 실행 + arg는 connfd
    Pthread_create(&tid, NULL, thread, cli_connfd);
  }
  return 0;
}

// 동시 작업 위해 쓰레드 사용, CS:APP_pic_12.14 참조.
void *thread(void *vargp)
{
  // connfd 가져옴
  int cli_connfd = *((int *)vargp);

  // 현재 thread detach함.
  /* 디태치 상태의 쓰레드는 종료 시 자원을 자동으로 해제하고 쓰레드가 종료되어도 다른 쓰레드가 join을 호출할 필요가 없습니다.*/
  Pthread_detach(pthread_self());

  // fd 가져왔으니 vargp free.
  Free(vargp);

  // 쓰레드에서 doit 실행
  doit(cli_connfd);

  // 통신 한번 끝났으니 소켓 닫음
  Close(cli_connfd);

  return NULL;
}

// 클라이언트에게서 받은 요청을 서버로 보내고
// 서버에서 온 응답을 클라이언트에게 보냄
void doit(int cli_connfd)
{
  rio_t cli_rio, svr_rio;
  int svr_connfd;
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE],
      hostname[MAXLINE], buf[MAXLINE], path[MAXLINE], port[MAXLINE], http_header[MAXLINE];

  // cli요청 송신
  rio_readinitb(&cli_rio, cli_connfd);
  if (!rio_readlineb(&cli_rio, buf, MAXLINE))
    return;

  // 요청헤더 method, uri, version에 나눠담음
  sscanf(buf, "%s %s %s", method, uri, version);

  // 파비콘 ^^
  if (strstr(uri, "favicon.ico"))
    return;

  // 에러 처리
  error_find(method, uri, version, cli_connfd);

  // uri 파싱
  parse_uri(uri, hostname, port, path);

  // 서버fd open하고 요청 보냄
  svr_connfd = Open_clientfd(hostname, port);
  make_http_header(http_header, hostname, path, &cli_rio);
  Rio_writen(svr_connfd, http_header, strlen(http_header));

  // 서버의 응답 받음
  int n;
  Rio_readinitb(&svr_rio, svr_connfd);
  while ((n = Rio_readlineb(&svr_rio, buf, MAXLINE)) != 0)
    Rio_writen(cli_connfd, buf, n);

  // 서버fd 닫음
  Close(svr_connfd);
}

// 서버에 보낼 요청 헤더 만듦
void make_http_header(char *http_header, char *hostname, char *path, rio_t *client_rio)
{
  char buf[MAXLINE], request_header[MAXLINE], other_header[MAXLINE], host_header[MAXLINE];

  // 인자 합침
  sprintf(request_header, "GET %s HTTP/1.0\r\n", path);

  // host 찾아서 host_header에 쓰기
  Rio_readlineb(client_rio, buf, MAXLINE);
  if (!strncasecmp(buf, "HOST", strlen("HOST")))
    strcpy(host_header, buf);

  // 만약 host 못찾았으면 hostname에서 들고옴
  if (strlen(host_header) == 0)
    sprintf(host_header, "Host: %s\r\n", hostname);

  // 더 좋은 방법없나  ...
  sprintf(http_header, "%s%s%s%s%s%s",
          request_header,
          host_header,
          "Connection: close\r\n",
          "Proxy-Connection: close\r\n",
          user_agent_header,
          "\r\n");

  return;
}

void parse_uri(char *uri, char *hostname, char *port, char *path)
{
  /*
  uri는 다음과 같은 구조라 가정함
  scheme://   사용자  @hostname        :port   /path(경로)         ?query            #fragment
  https://    sili    @www.naver.com   :80     /forum/questions    ?search=jungle    #search
  */

  strcpy(port, "8080"); // 포트가 지정되어 있지 않을 때를 대비해 기본 HTTP 포트 설정

  // http://가 있을 때와 없을 때 구분
  if (strstr(uri, "http://"))

    // http:// 부터 :까지, /까지, 끝까지 읽음
    sscanf(uri, "http:// %[^:]: %[^/] %[^\n]", hostname, port, path);

  else
    sscanf(uri, "/ %[^:]: %[^/] %[^\n]", hostname, port, path);

  return;
}

// 에러의 종류 식별
void error_find(char *method, char *uri, char *version, int fd)
{
  // err code는 https://datatracker.ietf.org/doc/html/rfc1945 참고함.

  if ((strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))) // method로 GET말고 다른거 받으면 에러 반환
  {
    clienterror(fd, method, "501", "not implemented", "Tiny couldn't implement this method");
    return;
  }

  if (strlen(uri) == 0) // uri 잘못됐으면 || (strchr(uri, '/') == NULL 붙이면 왜 ㅏㅇㄴ될까
  {
    clienterror(fd, method, "400", "Bad Request", "The request could not be understood by the server due to malformed uri syntax");
    return;
  }

  if (strlen(version) == 0) // version 잘못됐으면 || (strchr(version, '/') == NULL
  {
    clienterror(fd, method, "400", "Bad Request", "The request could not be understood by the server due to malformed version syntax");
    return;
  }

  // URI 길이 검사(길이 10kB넘으면 뇌절)
  if (strlen(uri) > 10240)
  {
    clienterror(fd, method, "414", "URI Too Long", "The URI requested is too long for the server to process.");
    return;
  }

  // 버전 검사
  if (strcmp(version, "HTTP/1.0") && strcmp(version, "HTTP/1.1"))
  {
    clienterror(fd, method, "505", "HTTP Version Not Supported", "The server does not support, or refuses to support, the HTTP protocol version that was used in the request message.");
    return;
  }
}

// 에러 메시지를 클라이언트에게 전송
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
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

  // 클라이언트에게 HTTP 응답 헤더 작성, 전송
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));

  // 컨텐츠 타입 HTML
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // HTML 본문의 길이.. 하고 끝냄
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));

  // 헤더 정보와 HTML 본문을 클라이언트에 전송
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}