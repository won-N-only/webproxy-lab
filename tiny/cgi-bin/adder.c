#include "csapp.h"
/*
서버에 코드를 전달하는 과정

HTTP 응답 생성: printf을 사용하여 HTTP 헤더와 응답 본문을 출력.
[Connection: close]는 연결을 종료
[Content-length]는 응답 본문의 길이
[Content-type]은 MIME 타입

출력 및 종료: printf를 통해 최종적인 HTML 응답 본문을 출력하고
fflush(stdout)으로 출력 버퍼를 비워 서버로 데이터를 전송.
*/

int main(void)
{
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  // 환경 변수에서 QUERY_STRING을 가져옴
  if ((buf = getenv("QUERY_STRING")) != NULL)
  {
    p = strchr(buf, '&'); // & 문자의 위치를 찾음
    *p = '\0';            // 첫 번째 숫자와 두 번째 숫자를 분리하기 위해 NULL 문자로 바꿈
    strcpy(arg1, buf);    // 첫 번째 숫자를 arg1에 복사
    strcpy(arg2, p + 1);  // 두 번째 숫자를 arg2에 복사
    n1 = atoi(arg1);      // 문자열을 정수로 변환
    n2 = atoi(arg2);
  }

  // 응답 본문 구성
  sprintf(content, "QUERY_STRING=%s", buf);                                           // 원래의 QUERY_STRING을 content에 추가
  sprintf(content, "Welcome to add.com:");                                            // 웹사이트 환영 메시지
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);                // 본문에 추가적인 문자열을 계속 추가
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2); // 계산 결과
  sprintf(content, "%sThanks for visiting!\r\n\r\n", content);                        // 방문 감사 메시지

  // HTTP 응답 헤더 출력
  // cgi에서는 stdout buffer에 출력을 모아둠 , flush해야 날아감
  // HTTP 헤더를 정의한 후, 두 개의 \r\n을 사용하여 헤더 섹션의 종료와 응답 본문의 시작을 나타냄.
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");

  // HTTP 응답 본문 출력
  printf("%s", content);
  fflush(stdout); // 출력 버퍼를 강제로 비움

  exit(0);
}
