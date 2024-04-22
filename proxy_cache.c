
#include "rbtree.h"
#include <stdlib.h>

/////////////////////// TODO: initialize struct if needed///////////////////////
rbtree *new_rbtree()
{
  rbtree *t = (rbtree *)calloc(1, sizeof(rbtree));
  node_t *nil = (node_t *)calloc(1, sizeof(node_t));

  t->nil = nil;
  t->root = nil;

  nil->color = RBTREE_BLACK;

  return t;
}

/////////////////////// TODO: reclaim the tree nodes's memory///////////////////////
void inorder_del(rbtree *t, node_t *curr);
void delete_rbtree(rbtree *t)
{
  node_t *curr = t->root;

  if (curr != t->nil)
    inorder_del(t, curr);

  free(t->nil);
  free(t);
}
void inorder_del(rbtree *t, node_t *curr)
{
  if (curr->right != t->nil)
    inorder_del(t, curr->right);

  if (curr->left != t->nil)
    inorder_del(t, curr->left);

  free(curr);
}

/////////////////////// TODO: implement insert///////////////////////
node_t *insertion_balance(rbtree *t, node_t *new);
node_t *left_rotate(rbtree *t, node_t *node);
node_t *right_rotate(rbtree *t, node_t *node);
node_t *rbtree_insert(rbtree *t, const key_t key)
{
  node_t *new = (node_t *)calloc(1, sizeof(node_t));
  new->key = key;
  new->left = t->nil;
  new->right = t->nil;
  new->color = RBTREE_RED;

  if (t->root == t->nil)
  {
    t->root = new;
    new->color = RBTREE_BLACK;
    new->parent = t->nil;
    return new;
  }
  node_t *curr = t->root;
  node_t *p;

  while (curr != t->nil)
  {
    p = curr;
    if (curr->key > new->key)
      curr = curr->left;
    else
      curr = curr->right;
  }

  if (key < p->key)
    p->left = new;
  else
    p->right = new;

  new->parent = p;

  insertion_balance(t, new);

  return t->root;
}

/////////////////////// TODO: implement insert fix-up///////////////////////
node_t *insertion_balance(rbtree *t, node_t *new)
{
  while (new->parent->color == RBTREE_RED)
  {
    if (new->parent == new->parent->parent->left)
    {
      node_t *uncle = new->parent->parent->right;
      if (uncle->color == RBTREE_RED)
      {
        new->parent->color = RBTREE_BLACK;
        uncle->color = RBTREE_BLACK;
        new->parent->parent->color = RBTREE_RED;
        new = uncle->parent;
      }
      else
      {
        if (new == new->parent->right)
        {
          new = new->parent;
          left_rotate(t, new);
        }
        new->parent->color = RBTREE_BLACK;
        new->parent->parent->color = RBTREE_RED;
        right_rotate(t, new->parent->parent);
      }
    }
    else
    {
      node_t *uncle = new->parent->parent->left;
      if (uncle->color == RBTREE_RED)
      {
        new->parent->color = RBTREE_BLACK;
        uncle->color = RBTREE_BLACK;
        new->parent->parent->color = RBTREE_RED;
        new = uncle->parent;
      }
      else
      {
        if (new == new->parent->left)
        {
          new = new->parent;
          right_rotate(t, new);
        }
        new->parent->color = RBTREE_BLACK;
        new->parent->parent->color = RBTREE_RED;
        left_rotate(t, new->parent->parent);
      }
    }
  }

  t->root->color = RBTREE_BLACK;
  return t->root;
}
node_t *left_rotate(rbtree *t, node_t *node)
{
  node_t *r = node->right;
  node->right = r->left;

  if (r->left != t->nil)
    r->left->parent = node;
  r->parent = node->parent;

  if (node->parent == t->nil)
    t->root = r;
  else if (node == node->parent->left)
    node->parent->left = r;
  else
    node->parent->right = r;

  r->left = node;
  node->parent = r;

  return r; 
}
node_t *right_rotate(rbtree *t, node_t *node)
{
  node_t *l = node->left;
  node->left = l->right;

  if (l->right != t->nil)
    l->right->parent = node;
  l->parent = node->parent;

  if (node->parent == t->nil)
    t->root = l;
  else if (node == node->parent->right)
    node->parent->right = l;
  else
    node->parent->left = l;

  l->right = node;
  node->parent = l;

  return l;
}

/////////////////////// TODO: implement find///////////////////////
node_t *rbtree_find(const rbtree *t, const key_t key)
{
  node_t *node = t->root;
  while (node != NULL && node != t->nil)
  {
    if (key == node->key)
      return node;

    else if (key > node->key)
      node = node->right;

    else if (key < node->key)
      node = node->left;
  }
  return (node_t* )NULL;
}
node_t *rbtree_min(const rbtree *t)
{
  node_t *node = t->root;
  while (node->left != t->nil)
    node = node->left;
  
  return node;
}
node_t *rbtree_max(const rbtree *t)
{
  node_t *node = t->root;
  while (node->right != t->nil)
    node = node->right;
  
  return node;
}

/////////////////////// TODO: implement erase///////////////////////
void tp(rbtree *t, node_t *node, node_t *node2);
node_t *find_min(const rbtree *t, node_t *node);
void erase_balance(rbtree *t, node_t *node);
int rbtree_erase(rbtree *t, node_t *node)
{
  node_t *min_node = node;
  node_t *min_node_son;
  color_t node_color = min_node->color;

  if (node->left == t->nil)
  {
    min_node_son = node->right;
    tp(t, node, node->right);
  }
  else if (node->right == t->nil)
  {
    min_node_son = node->left;
    tp(t, node, node->left);
  }

  else
  {
    min_node = find_min(t, node->right);
    node_color = min_node->color;
    min_node_son = min_node->right;

    if (min_node != node->right)
    {
      tp(t, min_node, min_node->right);
      min_node->right = node->right;
      min_node->right->parent = min_node;
    }

    else
      min_node_son->parent = min_node;
    
    tp(t, node, min_node);

    min_node->left = node->left;
    min_node->left->parent = min_node;
    min_node->color = node->color;
  }
  if (node_color == RBTREE_BLACK)
    erase_balance(t, min_node_son);
//이거안해주면 valgrind에 mem-leak생김- >> 왜 ? 
  free(node);

  return 0;
}
void tp(rbtree *t, node_t *node, node_t *replace)
{
  if (node->parent == t->nil)
    t->root = replace;
  else if (node == node->parent->left)
    node->parent->left = replace;
  else
    node->parent->right = replace;

  replace->parent = node->parent;
}
node_t *find_min(const rbtree *t, node_t *node)
{
  while (node->left != t->nil)
    node = node->left;

  return node;
}
void erase_balance(rbtree *t, node_t *node)
{
  node_t *sibling;
  while (node != t->root && node->color == RBTREE_BLACK)
  {
    if (node == node->parent->left)
    {
      sibling = node->parent->right;
      //case1
      if (sibling->color == RBTREE_RED)
      {
        sibling->color = RBTREE_BLACK;
        node->parent->color = RBTREE_RED;

        left_rotate(t, node->parent);

        sibling = node->parent->right;
      }
      //case2
      if (sibling->left->color == RBTREE_BLACK && sibling->right->color == RBTREE_BLACK)
      {
        sibling->color = RBTREE_RED;
        node = node->parent;
      }
      else
      {//case3 ->case4모양처럼만드는
        if (sibling->right->color == RBTREE_BLACK)
        {
          sibling->left->color = RBTREE_BLACK;
          sibling->color = RBTREE_RED;

          right_rotate(t, sibling);

          sibling = node->parent->right;
        }
        //case4
        sibling->color = node->parent->color;
        node->parent->color = RBTREE_BLACK;
        sibling->right->color = RBTREE_BLACK;

        left_rotate(t, node->parent);

        node = t->root;
      }
    }
    else
    {
      sibling = node->parent->left;
      if (sibling->color == RBTREE_RED)
      {
        sibling->color = RBTREE_BLACK;
        node->parent->color = RBTREE_RED;

        right_rotate(t, node->parent);

        sibling = node->parent->left;
      }
      if (sibling->right->color == RBTREE_BLACK && sibling->left->color == RBTREE_BLACK)
      {
        sibling->color = RBTREE_RED;
        node = node->parent;
      }
      else
      {
        if (sibling->left->color == RBTREE_BLACK)
        {
          sibling->right->color = RBTREE_BLACK;
          sibling->color = RBTREE_RED;

          left_rotate(t, sibling);

          sibling = node->parent->left;
        }
        sibling->color = node->parent->color;
        node->parent->color = RBTREE_BLACK;
        sibling->left->color = RBTREE_BLACK;

        right_rotate(t, node->parent);

        node = t->root;
      }
    }
  }
  node->color = RBTREE_BLACK;
}

/////////////////////// TODO: implement to_array///////////////////////
void inorder(const rbtree *t, node_t *node, key_t *arr, int *cnt);
int rbtree_to_array(const rbtree *t, key_t *arr, const size_t n)
{
  if (t->root == t->nil)
    return 0;

  int cnt = 0;
  int *cnt_p = &cnt;
  inorder(t, t->root, arr, cnt_p);

  return cnt;
}
void inorder(const rbtree *t, node_t *node, key_t *arr, int *cnt)
{
  if (node == t->nil)
    return;

  inorder(t, node->left, arr, cnt);

  arr[*cnt] = node->key;
  (*cnt)+=1;

  inorder(t, node->right, arr, cnt);
}