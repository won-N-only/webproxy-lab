/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */

/*
1. sprintf 는 printf와 유사하지만 문자열을 출력X, 특정 포맷에 맞춰 버퍼에 저장함.
  sprintf(buffer, "Age: %d", age) {buffer에 age:어쩌고 저장}

3. Rio_readlineb 함수는 text line을 \n을 만날 때까지 한줄씩 읽음
  Rio_readlineb(&rio, buffer, MAXLINE) {&rio에 MAXLINE 길이만큼 읽어서 buffer에 저장}

2. Rio_writen 함수는 파일디스크립터 fd에 n바이트만큼의 데이터를 data로 씀.
  Rio_writen(fd, data, strlen(data)) {fd에 strlen(data)만큼 data를 씀 }

4. Rio_readn
  Rio_readn(fd, buffer, 100)
  바이트 수만큼 데이터를 읽음. 요청된 바이트 수를 모두 읽을 때까지 호출...근데 버퍼없어

5. rio_readinitb
   rio_readinitb(rio_t *rp, int fd)
   식별자 fd를 주소 rp에 위치한 rio_t 타입의 읽기 버퍼와 연결한다.
*/

/*
uri의 기본 구조

scheme:[//[user:password@]host[:port]][/]path[?query][#fragment]
여기서 path 부분이 웹 서버의 리소스 경로
예를 들어, URI가 http: // example.com :80 /path/to/resource ?345&345인 경우, /path/to/resource가 리소스의 경로

헤더의 구성요소
Content-Type: 응답 본문의 미디어 타입. 예: text/html, application/json.
Content-Length: 응답 본문의 길이를 바이트 단위로 나타냄
Set-Cookie: 클라이언트에 쿠키를 설정하기 위한 헤더.
Cache-Control: 캐싱 메커니즘을 제어하기 위한 지시어.
Location: 3xx 상태 코드의 경우, 리디렉션할 URL을 지정
Connection: 해당 연결에 대한 옵션을 명시 (예: keep-alive, close).
ETag: 특정 버전의 리소스를 식별하는 태그.
Expires: 리소스가 만료되는 시간.
Server: 응답을 보내는 서버의 소프트웨어 정보.
*/

#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void get_filetype(char *filename, char *filetype);
void serve_static(int fd, char *filename, int filesize, char *method);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);

int main(int argc, char **argv)
{
  // listenfd: 소켓 bind후 cli의 연결요청 대기
  // connfd: cli와 통신용,클라이언트의 연결 요청을 accept() 함수로 받아들인 후 할당됨
  int listenfd;
  int connfd;

  // ci의 호스트, 포트 저장용
  char hostname[MAXLINE], port[MAXLINE];

  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  // 명령행 인자 검사 (프로그램은 반드시 포트 번호를 인자로 받아야 함)
  // argc는 인자의 갯수를 나타냄, argc!=2면 (ip,port)가 들어온게 아니므로 exit
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // Open_listenfd는 getaddrinfo, socket, bind, listen 기능을 순차적으로 실행하는 함수
  listenfd = Open_listenfd(argv[1]); // listen 소켓 오픈

  // 무한 while문으로 서버 항상 열어놓음.(무한 서버 루프)
  // close 있을 때 까지 서버 안닫힘

  /*
  **getaddrinfo()**는 주소 정보를 조회하여 네트워크 프로그래밍에 사용할 수 있는 주소 구조체를 제공하는 데 사용되며, 주로 연결 설정 초기 단계에서 사용됩니다.

  **getnameinfo()**는 주어진 네트워크 주소로부터 호스트 이름과 서비스 이름을 추출하는 데 사용되며, 연결 후 클라이언트 정보를 인간이 이해할 수 있는 형태로 변환하는 데 사용됩니다.
  */

  while (1)
  {
    clientlen = sizeof(clientaddr);

    /*서버는 accept() 호출해서 CL로부터의 연결요청 기다림
    CLI 소켓은 SVR소켓 주소 알고있으니까
    CLI에서 SVR로 넘어올 때 주소정보(addr)를 가지고 올거라고 가정하고
    accept()는 연결되면 식별자 connfd를 리턴함.*/

    // 클라이언트 연결 수락 하고 클라 정보 얻어옴
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    // clientaddr의 구조체에 대응되는 hostname, port를 작성한다.=
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);

    printf("Accepted connection from (%s, %s)\n", hostname, port);

    doit(connfd); // 트랜젝션 수행

    // 비연결 트랜젝션이라 doit하고 바로 닫아주는거여
    Close(connfd); // 서버 연결 닫음
  }
}

// CLI의 요청 보고 정적, 동적인지 확인
void doit(int fd) // 여기서 fd는 위의 connfd임(listen아님)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  // 요청구문, 헤더 읽음
  // 식별자 fd를 rio_t타입의 empty한 읽기버퍼 rio와 연결함(init)
  rio_readinitb(&rio, fd); // CSAPP pic10.8 참조

  // 파일 디스크립터에서 한 줄씩 읽는 동작을 함. HTTP 요청의 경우, 요청 라인이나 헤더 필드를 처리
  // 헤더 읽어오는거임
  rio_readlineb(&rio, buf, MAXLINE);

  printf("REQUESTED HEADERS: \n");
  printf("%s", buf);

  // buf의 내용을 각각 method, uri, ver에 저장함.
  sscanf(buf, "%s %s %s", method, uri, version);

  if ((strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))) // method로 GET말고 다른거 받으면 에러 반환
  {
    clienterror(fd, method, "501", "not implemented", "Tiny couldn't implement this method");
    return;
  }

  // method가 GET일 시
  read_requesthdrs(&rio); // 나머지 요청 헤더 읽기 (현재는 무시됨)

  // 요청이 정적인지 동적인지 판단
  // uri 내용 바탕으로 filename, cgiargs 채워짐
  is_static = parse_uri(uri, filename, cgiargs);

  if (stat(filename, &sbuf) < 0)
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static) // 요청이 정적 콘텐츠라면
  {
    // is_regular, is_user : 정규 파일인지, 읽을 권한 있는지 검사
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, method);
  }
  else // 요청이 동적 컨텐츠이면
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) // 정규 파일인지, 실행권한 있는지
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs, method);
  }
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
  // 상태 라인 설정, 예: "HTTP/1.0 404 Not Found"
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);

  // 헤더 정보를 소켓을 통해 클라이언트에 전송
  Rio_writen(fd, buf, strlen(buf));

  // 컨텐츠 타입 헤더 설정, HTML 문서임을 명시
  sprintf(buf, "Content-type: text/html\r\n");

  // 헤더 정보를 소켓을 통해 클라이언트에 전송
  Rio_writen(fd, buf, strlen(buf));

  // 컨텐츠 길이 헤더 설정, HTML 본문의 �