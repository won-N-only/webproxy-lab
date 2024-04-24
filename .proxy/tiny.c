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

  // 컨텐츠 길이 헤더 설정, HTML 본문의 길이를 바이트 단위로 명시
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));

  // 헤더 정보와 HTML 본문을 소켓을 통해 클라이언트에 전송
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

// Tiny는 request header의 정보를 하나도 사용하지 않는다.
// 요청 라인 한줄, 요청 헤더 여러줄 받는데
// 요청 라인은 저장해주고(우리가 tiny에서 필요한 건 이거임), 요청 헤더들은 그냥 출력
// 나중에 get에서 header로 바꿔보라고 이렇게한것같은데?
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  // 요청라인
  Rio_readlineb(rp, buf, MAXLINE); // 한줄씩 읽음(\n만나면 break)

  while (strcmp(buf, "\r\n")) // 빈 text 들어오면 while 종료
  {
    // 요청 헤더
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf); // buf에 등록된거 출력
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  /*정적의 home은 ./ 동적의 home은 ./cgi-bin로 설정함*/

  // strstr는 문자열 안에서 해당 단어 위치 찾음
  // strcpy는 2번째 인자를 1번째 인자 문자열에 덮어씌움
  // strcat은 인자 문자열을 이음
  char *ptr;
  if (!strstr(uri, "cgi-bin")) // uri에 cgi-bin 없으면
  {
    // 정적 콘텐츠
    strcpy(cgiargs, "");   // cgiargs 인자 string을 지운다.
    strcpy(filename, "."); // 상대 리눅스 경로이름으로 변환 ex) '.'
    strcat(filename, uri); // 상대 리눅스 경로이름으로 변환 ex) '.' + '/index.html'

    /*URI 예시: /images/logo.png
서버 내 파일 경로 변환:
filename은 초기에 "."로 설정되어 현재 디렉토리를 나타냅니다.
URI /images/logo.png가 filename에 추가되면 최종 경로는 ./images/logo.png가 됩니다. 이는 현재 디렉토리에서 images라는 하위 디렉토리 안에 있는 logo.png 파일을 가리킵니다.*/

    if (uri[strlen(uri) - 1] == '/') // uri가 /로 끝나면 /뒤에 home.html 추가//이거 문제에 활용

      strcat(filename, "adder.html"); /// 이거왜안대지 ㅜ ㅜ

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
    else                   // 나머지 uri는 상대경로로 변경
      strcpy(cgiargs, ""); // '?'없으면 빈칸으로 둘게
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
    /* ex) cgiargs: 123&123
     filename: ./cgi-bin/adder */
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
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

// Tiny는 5개의 정적 컨텐츠 지원함
// HTML, TXT, GIF, PNG, JPEG
// static content를 요청하면 서버가 disk에서 파일을 찾아서 메모리 영역으로 복사하고, 복사한 것을 client fd로 복사
void serve_static(int fd, char *filename, int filesize, char *method)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  // 클라이언트에 응답line, 헤더 전송
  get_filetype(filename, filetype); // 파일 타입 검사

  // client에 응답 줄과 헤더를 보낸다.
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);

  // snprintf는 버퍼의 사이즈를 초과하는 data 쓰지않음. 안정성좋아
  // snprintf(buf, sizeof(buf), "%sConnection: close\r\n", buf); // 서버가 각 요청 처리 후에 연결을 close하고 새 요청이 들어올 때마다 새로운 연결을 connect
  // http니까 연결 할떄마다 끊기고 다시하고 그거하는거임
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sProxy-Connection: close\r\n", buf);

  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype); //  \r\n\r\n이 헤더 종료표시

  Rio_writen(fd, buf, strlen(buf)); // buf에서 strlen(buf) 바이트만큼 fd로 전송한다.

  printf("Response headers: \n");
  printf("%s", buf);
  if (strcasecmp(method, "GET") == 0)
  {
    {
      // // filename open, 식별자(파일 디스크립터) 얻어옴
      srcfd = Open(filename, O_RDONLY, 0); // filename open, 식별자 얻어옴

      // /*요청한 파일을 가상메모리에 매핑함.
      // srcfd의 filesize만큼 시작하는 private R/O메모리에 매핑*/
      // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
      srcp = (char *)Malloc(filesize);  // 파일 크기만큼의 메모리를 동적할당한다.
      Rio_readn(srcfd, srcp, filesize); // filename 내용을 동적할당한 메모리에 쓴다.

      // // 매핑했으니 식별자 필요없어져서 close함
      Close(srcfd);

      // // 파일을 클라이언트에게 전송
      // // rio_writen은 srcp에서 시작하는 filesize만큼의 바이트를 클라이언트의 연결식별자로 복사
      Rio_writen(fd, srcp, filesize);

      // // 가상메모리 반환
      // Munmap(srcp, filesize);
      free(srcp);
    }
  }
}
// serve_dynamic은 프로세스를 fork해서 자식의 컨텍스트에서 cgi를 실행함.
// execve는
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  // Return first part of HTTP response
  // 클라이언트에 성공을 알려주는 응답 라인을 보내는 것으로 시작
  // 클라이언트에 서버 준비됐다고 응답함.
  // 동적이라 뭐가 나올지몰라서 type, size 안보냄
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // 자식 프로세스 포크.
  if (Fork() == 0)
  {
    // fork란? 부모 프로세스를 복사하지만 새로운 PID를 가지며 독립적으로 실행되는 자식 프로세스를 만듦.

    // 이때 부모 프로세스는 자식의 PID(Process ID)를, 자식 프로세스는 0을 반환받는다.

    // Real server would set all CGI vars here(실제 서버는 여기서 다른 CGI 환경변수도 설정)

    setenv("QUERY_STRING", cgiargs, 1); // 환경변수에 (uri에서 받아온 cgi)를 넣음 (set-env임)
                                        ///////////// ////// setenv로 request method 이용 가능할듯?////////////////////////
    setenv("METHOD", method, 1);        // 환경변수에 (uri에서 받아온 cgi)를 넣음 (set-env임)

    // 자식 프로세스의 표준 출력(STDOUT)을 클라이언트와 연결된 소켓 파일 디스크립터(fd)로 변경.
    // fd = STDOUT_FD(=1)로변경하는거
    // 이렇게 하면 CGI 스크립트의 출력이 직접 클라이언트로 전송됨.(소켓으로 바로감)
    // 자식 프로세스가 get_env하기 전에 실행됨
    Dup2(fd, STDOUT_FILENO);

    // cgi 로드하고 실행함.(여기선 adder) 자식context에서 실행되니까 부모에서 열린파일, 환경변수도 불러올 수 있음.
    Execve(filename, emptylist, environ);
  }

  // 자식이 아니면 즉,부모는 자식이 종료되어 정리되는 것을 기다리기 위해 wait 함수에서 블록된다.
  /*
  블록 상태(Blocked State):
  해당 함수를 호출한 프로세스가 대기 상태에 들어가서, 자식 프로세스의 종료를 기다린다는 의미.
  이 때, 부모 프로세스는 다른 작업을 수행하지 않고, 자식 프로세스의 종료 신호를 기다림.  CPU 자원도 할당 중단됨
  */
  Wait(NULL); // 부모는 자식이 종료되는거 기다림
}

////해결 불가////
// void signal_handler(int sig)
// {
//   printf("Graceful shutdown\n");
//   close(listenfd);
//   exit(0);
// }