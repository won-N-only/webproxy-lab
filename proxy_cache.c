#include <stdio.h>
#include <stdlib.h>
#include "csapp.h"

typedef struct CacheNode
{
  char *url;      // URL as key
  char *response; // HTTP response data
  struct CacheNode *prev, *next;
} CacheNode;

typedef struct
{
  CacheNode *head, *tail;
  int count;        // Current count of nodes in the cache
  int max_capacity; // Maximum capacity of the cache
} CacheList;

CacheList *create_cache(int capacity)
{
  CacheList *cache = malloc(sizeof(CacheList));
  cache->head = cache->tail = NULL;
  cache->count = 0;
  cache->max_capacity = capacity;
  return cache;
}
void access_cache(CacheList *cache, char *url, char *response)
{
  CacheNode *node = cache->head;
  CacheNode *prev = NULL;

  // Find the node
  while (node != NULL)
  {
    if (strcmp(node->url, url) == 0)
    {
      if (prev != NULL)
      { // Move to the front if it's not already
        prev->next = node->next;
        if (node->next != NULL)
        {
          node->next->prev = prev;
        }
        else
        { // Update tail if needed
          cache->tail = prev;
        }
        node->next = cache->head;
        node->prev = NULL;
        if (cache->head != NULL)
        {
          cache->head->prev = node;
        }
        cache->head = node;
      }
      return; // Node found and moved to front, exit
    }
    prev = node;
    node = node->next;
  }

  // Not found, create new node
  CacheNode *new_node = malloc(sizeof(CacheNode));
  new_node->url = strdup(url);
  new_node->response = strdup(response);
  new_node->prev = NULL;
  new_node->next = cache->head;

  if (cache->head != NULL)
  {
    cache->head->prev = new_node;
  }
  cache->head = new_node;

  if (cache->tail == NULL)
  { // First node
    cache->tail = new_node;
  }

  cache->count++;

  // Remove least recently used element if over capacity
  if (cache->count > cache->max_capacity)
  {
    CacheNode *to_delete = cache->tail;
    cache->tail = to_delete->prev;
    if (cache->tail != NULL)
    {
      cache->tail->next = NULL;
    }
    free(to_delete->url);
    free(to_delete->response);
    free(to_delete);
    cache->count--;
  }
}
void free_cache(CacheList *cache)
{
  CacheNode *current = cache->head;
  while (current != NULL)
  {
    CacheNode *next = current->next;
    free(current->url);
    free(current->response);
    free(current);
    current = next;
  }
  free(cache);
}
