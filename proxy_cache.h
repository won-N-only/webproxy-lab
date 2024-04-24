//////////////////////////////////////헤더 및 전처리문/////////////////////////////////////
#include <stddef.h>
#include <stdio.h>
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

//////////////////////////////////////캐시 관련 함수/////////////////////////////////////

// 캐시 저장 노드 구성
typedef struct cache_node
{
	char *hostname;
	char *uri;
	char *header;
	size_t block_size;
	char *content;
	struct cache_node *prev;
	struct cache_node *next;
} cache_node;

//
typedef struct cache_list
{
	struct cache_node *next;
	size_t available_size;
} cache_list;
struct cache_node *list_end;
struct cache_list *list;

void init_cache();
int check_available(size_t size);
int search_cache(char *hostname, char *uri,
				 struct cache_node **cache_ptr);
struct cache_node *create_node(char *hostname, char *uri,
							   size_t object_size, char *object_buf, char *header);
void insert_node(struct cache_node *new_node);
void delete_node(struct cache_node *delete_node);
void cutcutcut(size_t size);
void free_node_var(struct cache_node *delete_node);

// 캐시 리스트 선언
void init_cache()
{
	list = (struct cache_list *)malloc(sizeof(struct cache_list));

	list->available_size = MAX_CACHE_SIZE;
	list->next = NULL;
	list_end = NULL;
}

// 가용 공간 크기 확인
int check_available(size_t size)
{
	if ((list->available_size) >= size)
		return 1;

	else
		return 0;
}

// 캐시 리스트에 있는지 찾음
int search_cache(char *hostname, char *uri,
				 struct cache_node **cache_ptr)
{
	// 비어있으면
	if ((list->next) == NULL)
		return 0;

	// 리스트 완전 탐색
	struct cache_node *search_cache = list->next;
	while (search_cache != NULL)
	{
		if (!strcmp(search_cache->hostname, hostname))
		{
			if (!strcmp(search_cache->uri, uri))
			{
				*cache_ptr = search_cache;
				return 1;
			}
		}
		search_cache = search_cache->next;
	}
	return 0;
}

// 캐시 저장하기 전 노드 만들기
struct cache_node *create_node(char *hostname, char *uri,
							   size_t object_size, char *object_buf, char *cache_header)
{
	struct cache_node *new_cache_node = malloc(sizeof(struct cache_node));
	new_cache_node->hostname = strdup(hostname);
	new_cache_node->uri = strdup(uri);
	new_cache_node->header = strdup(cache_header);
	new_cache_node->block_size = object_size;
	new_cache_node->content = malloc(object_size);
	memcpy(new_cache_node->content, object_buf, object_size);
	return new_cache_node;
}

// 만든 노드 삽입
void insert_node(struct cache_node *new_node)
{
	// 노드 삽입위해 firstnode 지정
	struct cache_node *fist_node = list->next;

	// list가 차있으면
	if (fist_node != NULL)
	{
		fist_node->prev = new_node;
		new_node->next = fist_node;
		new_node->prev = NULL;
		list->next = new_node;
	}

	// list가 없으면
	else
	{
		list->next = new_node;
		list_end = new_node;
		new_node->prev = NULL;
		new_node->next = NULL;
	}

	list->available_size -= new_node->block_size;
}

// 노드 삭제
void delete_node(struct cache_node *node)
{
	size_t size = node->block_size;
	struct cache_node *previous_node = node->prev;
	struct cache_node *next_node = node->next;

	if (previous_node != NULL)
	{
		previous_node->next = next_node;

		if (next_node != NULL)
			next_node->prev = previous_node;

		else
			list_end = previous_node;
	}

	// 노드의 구성요소 전부 free 후 노드 free
	free_node_var(node);
	Free(node);

	// 캐시 list 가용 공간 재조정
	list->available_size += size;
}

// 노드 구성요소 전부 free
void free_node_var(struct cache_node *node)
{
	Free(node->hostname);
	Free(node->uri);
	Free(node->content);
	Free(node->header);
}

// 캐시 리스트 다 차면 가장 오래전에  쓴것 부터 삭제
void cutcutcut(size_t size)
{
	// 뒤에서 부터 지워나감
	size_t del_size = 0;
	size_t total_del_size = 0;
	while (total_del_size < (size - list->available_size))
	{
		del_size = list_end->block_size;
		delete_node(list_end);
		total_del_size += del_size;
	}
}

//////////////////////////////////////선언부/////////////////////////////////////

// 헤더 모아서 보내는 용도의 구조체
typedef struct request_info
{
	char *hostname;
	char *method;
	char *port;
	char *uri;
} request_info;

/* You won't lose style points for including this long line in your code */
static const char *user_agent_header =
	"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
	"Firefox/10.0.3\r\n";

void *doit(void *connfd_ptr);
void clienterror(int fd, char *cause, char *errnum,
				 char *shortmsg, char *longmsg);
void read_requesthdrs(rio_t *rp, char *header_buf);
int parse_uri(int fd, char *method, char *url,
			  char *version, struct request_info *r_info);
void handle_connection(int fd, struct request_info *r_info, char *header_buf);
int forward_cache(int fd, struct request_info *r_info, char *header_buf);
void handle_hostname(char *host, char *port, char *result);
void free_r_info(struct request_info *r_info);
