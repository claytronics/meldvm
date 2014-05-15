
#ifndef DB_AGG_CONFIGURATION_HPP
#define DB_AGG_CONFIGURATION_HPP

#include <ostream>

#include "mem/base.hpp"
#include "db/tuple.hpp"
#include "vm/defs.hpp"
#include "vm/types.hpp"
#include "db/trie.hpp"

namespace db
{
   
class agg_configuration: public mem::base
{
public:

   MEM_METHODS(agg_configuration)

   void print(std::ostream&, vm::predicate *) const;

   void generate(vm::predicate *, const vm::aggregate_type, const vm::field_num, simple_tuple_list&);

   bool test(vm::predicate *, vm::tuple *, const vm::field_num) const;

   inline bool has_changed(void) const {}
   inline bool is_empty(void) const {}
   inline size_t size(void) const {}

   virtual void add_to_set(vm::tuple *, vm::predicate *, const vm::derivation_count, const vm::depth_t);
   
   bool matches_first_int_arg(vm::predicate *, const vm::int_val) const;

   explicit agg_configuration(void)  {
   }

   inline void wipeout(vm::predicate *pred) {
   }
};
}

#endif
