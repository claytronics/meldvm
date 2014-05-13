
#ifndef DB_LINKED_LIST_HPP
#define DB_LINKED_LIST_HPP

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <iostream>

extern "C" {  
#define VM_TUPLE_PTR void*
#define VM_PRE_PTR void*

  struct Node {
    VM_TUPLE_PTR tuplePtr;
    struct Node *next;
  };
  typedef struct Node list_node;
  
  struct linked_list {
    list_node *head;
    list_node *tail;
    VM_PRE_PTR root_predicate;
  };
  typedef struct linked_list linked_list;
  
  list_node* search_in_list(linked_list *ls, void *tpl, list_node **prev);
  int delete_from_list(linked_list *ls, void *tpl);
  void add_last(linked_list *ls, void *tpl);
  linked_list* create_list(void *pred);
  void print_list(linked_list *ls);
}

void
print_tuple(void *tpl, void *pred);

bool 
list_tuple_equal(void* tpll, void* tpla, void *preda);

#endif
