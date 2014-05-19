
#ifndef C_DB_LINKED_LIST_HPP
#define C_DB_LINKED_LIST_HPP

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <iostream>

#ifdef __cplusplus
extern "C" {  
#endif

#define VM_TUPLE_PTR void*
#define VM_PRE_PTR void*

  struct Node {
    VM_TUPLE_PTR tuple_ptr;
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
  bool delete_from_list(linked_list *ls, void *tpl);
  bool add_last(linked_list *ls, void *tpl);
  linked_list* create_list(void *pred);
  void print_list(linked_list *ls);

#ifdef __cplusplus
}
#endif

#endif
