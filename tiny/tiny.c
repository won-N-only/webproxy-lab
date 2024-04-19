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


2. Rio_writen 함수는 파일디스크립터 fd에 n바이트만큼의 데이터를 data로 씀.
  Rio_writen(fd, data, strlen(data)) {fd에 strlen(data)만큼 data를 씀 }

3. Rio_readlineb 함수는 text line을 \n을 만날 때까지 한줄씩 읽음
  Rio_readlineb(&rio, buffer, MAXLINE) {&rio에 MAXLINE 길이만큼 읽어서 buffer에 저장}

4. Rio_readn
  Rio_readn(fd, buffer, 100)


 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  // 명령행 인자 검사 (프로그램은 반드시 포트 번호를 인자로 받아야 함)
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // listen 소켓 오픈

  // 무한 while문으로 서버 항상 열어놓음.(무한 서버 루프)
  // close 있을 때 까지 서버 안닫힘
  while (1)
  {
    clientlen = sizeof(clientaddr);
    // 클라이언트 연결 수락 하고 클라 정보 얻어옴
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit, 트랜젝션 수행
    Close(connfd); // line:netp:tiny:close, 서버 연결 닫음
  }
}

void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  // 요청구문, 헤더 읽음
  rio_readinitb(&rio, fd); // CSAPP pic10.8 참조
  rio_readlineb(&rio, buf, MAXLINE);
  printf("REQUESTED HEADERS: \n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);

  if (strcasecmp(method, "GET")) // method로 GET말고 다른거 받으면 에러 반환
  {
    clienterror(fd, method, "501", "not implemented", "Tiny couldn't implement this method");
    return;
  }

  // method가 GET일 시
  read_requesthdrs(&rio); // 나머지 요청 헤더 읽기 (현재는 무시됨)

  // get request에서 uri 파싱함

  is_static = parse_uri(uri, filename, cgiargs); // 요청이 정적인지 동적인지 판단

  if (stat(filename, &sbuf) < 0)
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static)                                               // 요청이 정적 콘텐츠라면
  {                                                            /* Serve static content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) // 보통 파일인지, 읽을 권한 있는지 검사
    {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);
  }
  else // 요청이 동적 컨텐츠이면
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) // 보통파일인지, 실행권한 있는지
    {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
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

  // 컨텐츠 길이 헤더 설정, HTML 본문의 길이를 바이트 단위로 명시
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));

  // 헤더 정보와 HTML 본문을 소켓을 통해 클라이언트에 전송
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
  /*Tiny서버에서는 요청의 header에 들어있는 정보를 읽기만 할 뿐 사용하지 않는다->무시한다*/
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);

  while (strcmp(buf, "\r\n")) // 빈 text 들어오면 while 종료
                              //"\r"은 캐리지리턴, "\n"은 라인피드 라고 함..
  {
    Rio_readlineb(rp, buf, MAXLINE); // line:netp:readhdrs:checkterm
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  /*정적 콘텐츠 홈 디렉터리: Tiny 서버는 정적 콘텐츠의 홈 디렉터리로 현재 작업 중인 디렉터리를 가정합니다.
동적 콘텐츠 실행 파일 디렉터리: 동적 콘텐츠를 위한 홈 디렉터리는 ./cgi-bin으로 설정됩니다.

 정적 콘텐츠 요청: URI에서 CGI 인자 문자열을 제거하고 (파싱 함수의 6번 라인), URI를 Linux 상대 경로 이름으로 변환합니다 (예: ./index.html). 이 과정은 7-8번 라인에서 수행됩니다.

디렉터리 기본 파일명: URI가 '/' 문자로 끝날 경우 (9번 라인), 기본 파일명인 ./home.html을 추가합니다 (10번 라인).
동적 콘텐츠 요청: URI가 cgi-bin을 포함하는 경우, 동적 콘텐츠로 간주되며, CGI 인자를 추출합니다 (14-20번 라인). URI의 나머지 부분은 Linux 상대 경로 파일명으로 변환됩니다 (21-22번 라인).

*/

  // strstr는 문자열 안에서 해당 단어 위치 찾음
  // strcpy는 2번째 인자를 1번째 인자 문자열에 덮어씌움
  // strcat은 인자 문자열을 이음
  char *ptr;
  if (!strstr(uri, "cgi-bin")) // uri에 cgi-bin 없으면
  {
    // 정적 콘텐츠
    strcpy(cgiargs, ""); // uri에서 cgi문자열 제거
    strcpy(filename, ".");
    strcat(filename, uri);           // uri 상대경로 이름으로 변환
    if (uri[strlen(uri) - 1] == '/') // uri가 /로 끝나면 /뒤에 home.html 추가해줌
      strcat(filename, "home.html");
    return -1;
  }
  else // uri에 cgi-bin있으면
  {
    // 동적 콘텐츠
    ptr = index(uri, '?');
    if (ptr) // cgi 인자 추출
    {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else // 나머지 uri는 상대경로로 변경
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

// file name에서 확장자 읽어옴.
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else
    strcpy(filetype, "text/plain");
}

// Tiny는 5개의 정적 컨텐츠 지원함
// HTML, TXT, GIF, PNG, JPEG
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  // 클라이언트에 응답line, 헤더 전송
  get_filetype(filename, filetype);    // 파일 타입 검사
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); // line:netp:servestatic:beginserve
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));

  // filename open, 식별자(파일 디스크립터) 얻어옴
  srcfd = Open(filename, O_RDONLY, 0); // filename open, 식별자 얻어옴

  /*요청한 파일을 가상메모리에 매핑함.
  srcfd의 filesize만큼 시작하는 private R/O메모리에 매핑*/
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);

  // 매핑했으니 식별자 필요없어져서 close함
  Close(srcfd); // line:netp:servestatic:close

  // 파일을 클라이언트에게 전송
  // rio_writen은 srcp에서 시작하는 filesize만큼의 바이트를 클라이언트의 연결식별자로 복사
  Rio_writen(fd, srcp, filesize); // line:netp:servestatic:write

  // 가상메모리 반환
  Munmap(srcp, filesize); // line:netp:servestatic:munmap
}

// serve_dynamic은 프로세스를 fork해서 자식의 컨텍스트에서 cgi를 실행함.
// fork란? 부모 프로세스를 복사하지만 새로운 PID를 가지며 독립적으로 실행되는 자식 프로세스를 만듦.
// execve는
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  // 클라이언트에 서버 준비됐다고 응답함.
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // 자식 프로세스 포크.
  if (Fork() == 0)
  {
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1); // 환경변수를 uri의 cgi로 초기화함 (set-env임)

    // 자식 프로세스의 표준 출력(STDOUT)을 클라이언트와 연결된 소켓 파일 디스크립터(fd)로 변경.
    // 이렇게 하면 CGI 스크립트의 출력이 직접 클라이언트로 전송됨.(소켓으로 바로감)
    Dup2(fd, STDOUT_FILENO);

    // cgi 로드하고 실행함. 자식context에서 실행되니까 부모에서 열린파일, 환경변수도 불러올 수 있음.
    Execve(filename, emptylist, environ);
  }

  Wait(NULL); // 부모는 자식이 종료되는거 기다림
}
