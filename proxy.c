#include "proxy_cache.h"

int main(int argc, char **argv)
{
  int listenfd, connfd;
  int *connfd_ptr;
  int rc;
  pthread_t tid;
  struct sockaddr_storage clientaddr;
  socklen_t clientlen = sizeof(clientaddr);

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // 캐시 리스트 초기화
  init_cache();

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    connfd_ptr = Malloc(sizeof(void *));
    *connfd_ptr = connfd;

    // 쓰레드 할때 Pthread 안만들고 doit에 넣음
    Pthread_create(&tid, NULL, doit, connfd_ptr);
  }
}

void *doit(void *connfd_ptr)
{
  // 멀티쓰레드를위함//
  int fd = *(int *)connfd_ptr;
  Pthread_detach(Pthread_self());
  Free(connfd_ptr);
  int rc;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE],
      hostname[MAXLINE], path[MAXLINE], port[MAXLINE];
  char header_buf[MAXLINE];
  rio_t rio;
  request_info *req_info;

  req_info = Malloc(sizeof(request_info));

  // 클라이언트의 요청 읽음
  Rio_readinitb(&rio, fd);
  if (!Rio_readlineb(&rio, buf, MAXLINE))
  {
    Free(req_info);
    Close(fd);
    return NULL;
  }

  // 요청을 method, uri, version로 나눔
  // 세개 완벽하게 안들어오면 컷
  sscanf(buf, "%s %s %s", method, uri, version);

  // uri 파싱
  parse_uri(fd, method, uri, version, req_info);

  // 캐시에 있으면 찾은 뒤 보냄
  rc = forward_cache(fd, req_info, header_buf);

  // 캐시에 없으면
  if (rc == 0)
    handle_connection(fd, req_info, header_buf);

  // 요청 정보 지움
  free_req_info(req_info);
  Free(req_info);

  Close(fd);

  return NULL;
}

// 클라이언트에게 캐시 전송
// 전송할 수 있으면 캐시 위치 갱신함
int forward_cache(int fd, struct request_info *req_info, char *header_buf)
{
  struct cache_node *result_cache;
  int idx;
  char key[100];

  // 캐시 찾기 위해서 key에 (hostname+port)담음
  make_key(req_info->hostname, req_info->port, key);

  // 캐시 유무 검사 후 있으면 1 반환
  idx = search_cache(key, req_info->uri, &result_cache);

  // 캐시에 없으면 컷
  if (!idx)
    return 0;

  // 캐시에 있으면 보냄
  if (idx)
  {
    Rio_writen(fd, result_cache->header,
               strlen(result_cache->header));
    Rio_writen(fd, result_cache->content,
               result_cache->block_size);
    return;
  }
  return 0;
}

// 서버에서 응답 받고 클라이언트로 전달
// 응답을 캐시에 저장
void handle_connection(int connfd, struct request_info *req_info, char *header_buf)
{
  int svrfd, cannot_cache, read_size, totla_size;
  char *host, *uri, *port, key[MAXLINE], buf[MAXLINE],
      cache_header_buf[MAXLINE], cache_content_buf[MAX_OBJECT_SIZE];
  rio_t rio;

  // 요청 변수화
  host = req_info->hostname;
  uri = req_info->uri;

  // 포트 없으면기본포트 8080으로 지정
  if (req_info->port[0] == '\0')
    port = "8080";
  else
    port = req_info->port;

  // hostname, port로 key 만듦
  make_key(req_info->hostname, req_info->port, key);

  svrfd = Open_clientfd(host, port);
  Rio_readinitb(&rio, svrfd);

  // 요청헤더 전송
  sprintf(buf, "%s %s %s\r\n", "GET", uri, "HTTP/1.0");
  sprintf(buf, "%sHost: %s\r\n", buf, key);
  sprintf(buf, "%s%s\r\n", buf, header_buf);
  Rio_writen(svrfd, buf, strlen(buf));
  //////////////////////////////////////요청 종료/////////////////////////////////////

  //////////////////////////////////////응답 시작/////////////////////////////////////

  // 문자열이 끝날때까지 응답 헤더 수신
  Rio_readlineb(&rio, buf, MAXLINE);
  while (strcmp(buf, "\r\n"))
  {
    // 버퍼에 문자열 삽입
    sprintf(cache_header_buf, "%s%s", cache_header_buf, buf);
    Rio_readlineb(&rio, buf, MAXLINE);
  }

  // 문자열 끝 표시위해 \r\n삽입
  sprintf(cache_header_buf, "%s\r\n", cache_header_buf);

  // 응답헤더 송신
  Rio_writen(connfd, cache_header_buf, strlen(cache_header_buf));

  // 응답 본문 수신
  totla_size = 0;
  cannot_cache = 0;

  // 파일크기가 MAXLINE보다 클걸 대비해 while(readnb) 사용
  while (1)
  {
    // 서버에서 읽어오고 buf에 쓰고
    read_size = Rio_readnb(&rio, buf, MAXLINE);
    Rio_writen(connfd, buf, read_size);

    // buf에 쓴거 캐싱함
    // 전체 크기가 max_obj_size보다 크면 캐싱못해
    totla_size += read_size;
    if (totla_size > MAX_OBJECT_SIZE)
      cannot_cache = 1;

    // 작으면 가능
    else
    {
      void *ptr = (void *)((char *)(cache_content_buf) + totla_size - read_size);
      memcpy(ptr, buf, read_size);
    }

    // 다 읽어왔으면
    if (read_size == 0)
      break;
  }
  Close(svrfd);

  //////////////////////////////////////응답 종료/////////////////////////////////////

  //////////////////////////////////////캐싱 시작/////////////////////////////////////

  // 서버 요청 이상하면 컷
  if (totla_size <= 0)
    return;

  // 캐싱 가능하면
  if (!cannot_cache)
  {
    // 캐시 저장 위치 포인터로 불러옴
    struct cache_node *new_cache =
        create_node(key, uri, totla_size,
                    cache_content_buf, cache_header_buf);

    // 파일 크기가 캐시 리스트에 들어갈 수 있으면 바로 삽입
    if (check_available(totla_size))
      insert_node(new_cache);

    // 파일 크기가 캐시 리스트 가용 크기를 초과하면
    else
    {
      // 삭제 후 삽입
      cutcutcut(totla_size);
      insert_node(new_cache);
    }
  }
  return;
}

// 캐시 찾기위해 호스트, 포트번호 합침
// host =naver.com, port=8080 이면, result=naver.com:8080이 됨
void make_key(char *host, char *port, char *result)
{
  size_t needed;

  // 포트번호 없으면 포트없이 저장
  if ((port)[0] == '\0')
    strcpy(result, host);
  else
  {
    // snprintf로 문자열의 총 길이 needed에 담음
    needed = snprintf(NULL, 0, "%s:%s", host, port);

    // result에 host+port(cache key) 저장
    char buf[needed];
    snprintf(result, needed + 1, "%s:%s", host, port);
  }
}

// 요청 보내고 나서 req_info 지움
void free_req_info(struct request_info *req_info)
{
  if (req_info->hostname[0] != '\0')
    Free(req_info->hostname);
  if (req_info->port[0] != '\0')
    Free(req_info->port);
  if (req_info->uri[0] != '\0')
    Free(req_info->uri);
}
//////////////////////////////////////기타 함수 부분/////////////////////////////////////

// uri 파싱
int parse_uri(int fd, char *method, char *url,
              char *version, struct request_info *req_info)
{ /*
 uri는 다음과 같은 구조라 가정함
 scheme://   사용자  @hostname        :port   /path(경로)         ?query            #fragment
 https://    sili    @www.naver.com   :80     /forum/questions    ?search=jungle    #search
 */
  char hostname[100] = {0};
  char port[10] = "8080";  // 기본 포트
  char uri[MAXLINE] = "/"; // 기본 URI
  char path[MAXLINE] = {0};

  // URL에서 hostname, port, path를 추출
  if (sscanf(url, "http://%99[^:]:%9[^/]/%s", hostname, port, path) < 1)
  {
    return 0; // URL 형식이 맞지 않으면 0 반환
  }

  // URI 생성
  if (path[0] != '\0')
  {
    snprintf(uri, MAXLINE, "/%s", path); // 경로가 있으면 URI를 업데이트
  }

  // 파싱한 데이터를 req_info에 저장함
  req_info->hostname = strdup(hostname);
  req_info->port = strdup(port);
  req_info->uri = strdup(uri);

  return 1;
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
