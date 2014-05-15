#include "linked_list.hpp"

// Finds a tuple in the list - returns the node which contains it.
// If a **prev node has been given as argument, the function makes it point to the node before the matching node for deletion.
list_node* 
search_in_list(linked_list *ls, VM_TUPLE_PTR tpl, list_node **prev)
{
  list_node *ptr = ls->head;
  list_node *tmp = NULL;
  bool found = false;
      
  while(ptr != NULL) {
    if(list_tuple_equal(ptr->tuple_ptr, tpl, ls->root_predicate)) {
      found = true;
      break;
    }
    else {
      tmp = ptr;
      ptr = ptr->next;
    }
  }
  
  if(true == found) {
    if(prev)
      *prev = tmp;
    return ptr;
  }
  else {
    return NULL;
  }
}

// Delete a node from the list
bool
delete_from_list(linked_list *ls, VM_TUPLE_PTR tpl)
{
  list_node *prev = NULL; // Pointer to node previous to the one to delete
  list_node *del = NULL;

  del = search_in_list(ls, tpl, &prev);
  if(del == NULL)
    return false; // Node already deleted
  else {
    if(prev != NULL)
      prev->next = del->next;
      
    if(del == ls->tail)
      ls->tail = prev;
    else if(del == ls->head)
      ls->head = del->next;
  }

free(del);
del = NULL;

return true;
}
  
// Add a new node at the end of the list, after current tail
bool
add_last(linked_list *ls, VM_TUPLE_PTR tpl) 
{
  Node *newNode = (list_node*)malloc(sizeof(list_node));
 
  newNode->tuple_ptr = tpl;
  newNode->next = NULL;

  if(ls->head == NULL)
    ls->head = newNode;
  else
    ls->tail->next = newNode;
    
  ls->tail = newNode;
  return true;
}
  
// Create new list for predicate pred
linked_list* 
create_list(VM_PRE_PTR pred)
{
  linked_list *ls = (linked_list*)malloc(sizeof(linked_list));
  assert(ls != NULL);
  ls->root_predicate = pred;
  
  ls->head = NULL;
  ls->tail = NULL;

  return ls;
}

void 
print_list(linked_list *ls)
{
  list_node *ptr = ls->head;

  //printf("\n ------- Beginning of list ------- \n");
  while(ptr != NULL) {
    print_tuple(ptr->tuple_ptr, ls->root_predicate);
    printf("\n");
    ptr = ptr->next;
  }
  printf(" -------    End of List    ------- \n");

  return;
}
