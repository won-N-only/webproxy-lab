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
  char *buf, *p, *method;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];

  char *num1, *num2;
  int n1 = 0, n2 = 0, sum;

  // 환경 변수에서 QUERY_STRING을 가져옴
  buf = getenv("QUERY_STRING");
  method = getenv("METHOD");

  // num1과 num2 값 파싱
  num1 = strstr(buf, "num1="); // "num1="
  if (num1)
  {
    num1 += 5;                   // "num1=" 문자열 다음의 숫자 시작 위치로 이동
    num2 = strstr(buf, "num2="); // "num2="
    if (num2)
    {
      num2 += 5;       // "num2=" 문자열 다음의 숫자 시작 위치로 이동
      n1 = atoi(num1); // 첫 번째 숫자로 변환
      n2 = atoi(num2); // 두 번째 숫자로 변환
      sum = n1 + n2;   // 두 숫자를 더함
    }
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
  if (strcasecmp(method, "GET") == 0)
  {
    printf("%s", content);
  }
  fflush(stdout); // 출력 버퍼를 강제로 비움

  exit(0);
}
