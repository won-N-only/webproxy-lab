#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct cache_node
{
	char *hostname;
	char *path;
	char *header;
	size_t size;
	char *content;

	struct cache_node *prev;
	struct cache_node *next;
} cache_node;

typedef struct cache_list
{
	struct cache_node *head;
	struct cache_node *foot;
	size_t available_size;
} cache_list;

struct cache_node *create_node(char *hostname, char *path,
							   size_t size, char *buf, char *cache_header);
void init_cache();
int check_available(size_t new_size);
int search_cache(char *hostname, char *path,
				 struct cache_node **p);
void insert_node(struct cache_node *node);
void delete_node(struct cache_node *node);
void recycle(size_t size);
void free_node(struct cache_node *node);

// 캐시 리스트 초기화
struct cache_list *list;
void init_cache()
{
	list = (struct cache_list *)malloc(sizeof(struct cache_list));

	list->available_size = MAX_CACHE_SIZE;
	list->head = NULL;
	list->foot = NULL;
}

// 캐시 공간에 들어가나 체크
int check_available(size_t new_size)
{
	if (list->available_size >= new_size)
		return 1;
	else
		return 0;
}

// 캐시리스트 읽음
int search_cache(char *hostname, char *path, struct cache_node **p)
{
	// 리스트가 비어있으면
	if ((list->head) == NULL)
		return 0;

	// 리스트 완전탐색하면서 hostname, path 같은것 찾음
	struct cache_node *node = list->head;
	while (node != NULL)
	{
		if (!strcmp(node->hostname, hostname))
		{
			if (!strcmp(node->path, path))
			{
				// cache 사용 갱신
				node->prev->next = node->next;
				node->next->prev = node->prev;
				list->head->next->prev = node;
				node->prev = list->head;
				node->next = list->head->next;

				*p = node;
				return 1;
			}
		}
		node = node->next;
	}
	return 0;
}

// 삽입노드 생성
struct cache_node *create_node(char *hostname, char *path,
							   size_t size, char *buf, char *cache_header)
{
	// 삽입 노드만큼 메모리공간 확보
	struct cache_node *new_node = malloc(sizeof(struct cache_node));

	// 삽입노드 초기화(by string duplicate)
	new_node->hostname = strdup(hostname);
	new_node->path = strdup(path);
	new_node->header = strdup(cache_header);
	new_node->size = size;
	new_node->content = malloc(size);

	// content에 buf 내용 옮김
	memcpy(new_node->content, buf, size);
	return new_node;
}

// 생성한 노드 삽입
void insert_node(struct cache_node *node)
{
	struct cache_node *posi = list->head;

	if (posi != NULL)
	{
		posi->prev = node;
		node->next = posi;
	}
	node->prev = NULL;
	list->head = node;
	if (list->foot == NULL)
	{
		list->foot = node;
	}

	list->available_size -= node->size;
}

// 노드 삭제
void delete_node(struct cache_node *node)
{
	size_t size = node->size;

	// 리스트에서 노드 분리 후 free
	struct cache_node *prev_node = node->prev;
	struct cache_node *next_node = node->next;

	// 이전 노드가 존재할 때
	if (prev_node != NULL)
	{
		prev_node->next = next_node;

		// 다음 노드 존재할 때
		if (next_node != NULL)
			next_node->prev = prev_node;

		// 다음 노드 없을 때
		else
		{
			prev_node->next = NULL;
			list->foot = prev_node;
		}

		// 분리 됐으니 free
		free_node(node);

		// 캐시 가용 공간 재조정
		list->available_size += size;
	}
}

// 노드 전체 free
void free_node(struct cache_node *node)
{
	Free(node->hostname);
	Free(node->path);
	Free(node->content);
	Free(node->header);
}

// cache공간이 꽉 찼을 때 삭제함
void recycle(size_t size)
{
	size = 0;
	size_t total_size = 0;

	while (total_size < size - (list->available_size))
	{
		size = list->foot->size;
		delete_node(list->foot);
		total_size += size;
	}
}