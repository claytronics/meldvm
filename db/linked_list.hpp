#ifndef DB_LINKED_LIST_HPP
# define DB_LINKED_LIST_HPP

#include "vm/tuple.hpp"
#include "vm/predicate.hpp"

void
print_tuple(void *tpl, void *pred);

bool 
list_tuple_equal(void* tpll, void* tpla, void *preda);

#endif
