
#include "db/linked_list.hpp"

#include "vm/tuple.hpp"
#include "vm/predicate.hpp"

void
print_tuple(void *tpl, void *pred) 
{
  vm::tuple *tpl2 = (vm::tuple*) tpl;
  vm::predicate *pred2 = (vm::predicate*) pred;
  assert( (tpl2 != NULL)&&(pred2 != NULL) );
  tpl2->print(std::cout, pred2);
}

bool 
list_tuple_equal(void* tpll, void* tpla, void *preda)
{
  vm::tuple *tpl = (vm::tuple*) tpll;
  vm::tuple *tpl2 = (vm::tuple*) tpla;
  vm::predicate *pred2 = (vm::predicate*) preda;
  return (tpl->equal(*tpl2, pred2));
}
