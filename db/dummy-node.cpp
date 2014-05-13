
#include <iostream>
#include <assert.h>

#include "db/node.hpp"
#include "vm/state.hpp"
#include "utils/utils.hpp"

#ifdef USE_UI
#include "ui/macros.hpp"
#endif

using namespace db;
using namespace std;
using namespace vm;
using namespace utils;

namespace db
{
  void assert_end(void)
  {
  }

  void assert_end_iteration(void)
  {
  }

  tuple_trie::tuple_search_iterator
  node::match_predicate(const predicate_id id) const
  {

  }

  tuple_trie::tuple_search_iterator
  node::match_predicate(const predicate_id id, const match* m) const
  {

  }

  void
  node::dump(ostream& cout) const
  {
  }

}
