#include "db/linked_list.hpp"
#include "vm/tuple.hpp"
#include "vm/predicate.hpp"

void
print_tuple(void *tpl, void *pred) 
{
  vm::tuple *print_tpl = (vm::tuple*) tpl;
  vm::predicate *print_pred = (vm::predicate*) pred;
  print_tpl->print(std::cout, print_pred);
}

bool 
list_tuple_equal(void* tpll, void* tpla, void *preda)
{
  vm::tuple *ls_tpl = (vm::tuple*) tpll;
  vm::tuple *ext_tpl = (vm::tuple*) tpla;
  vm::predicate *pred = (vm::predicate*) preda;
  return (ls_tpl->equal(*ext_tpl, pred));
}
