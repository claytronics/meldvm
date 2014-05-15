
#ifndef DB_LINEAR_STORE_HPP
#define DB_LINEAR_STORE_HPP

#include <list>
#include <set>
#include <iostream>
#include <unordered_map>

#include "mem/base.hpp"
#include "db/tuple.hpp"
#include "vm/defs.hpp"
#include "vm/bitmap.hpp"
#include "db/intrusive_list.hpp"
#include "db/hash_table.hpp"
#include "utils/spinlock.hpp"

#define CREATE_HASHTABLE_THREADSHOLD 8
#define ITEM_SIZE ((sizeof(hash_table) > sizeof(tuple_list) ? sizeof(hash_table) : sizeof(tuple_list)))

namespace db
{

struct linear_store
{
   public:

      typedef intrusive_list<vm::tuple> tuple_list;
      // we store N lists or hash tables (the biggest structure) so that it is contiguous
      utils::byte *data;

      vm::bitmap types;
      utils::spinlock internal;

      hash_table *expand;

   private:

   public:

      inline tuple_list* get_linked_list(const vm::predicate_id p) {}
      inline const tuple_list* get_linked_list(const vm::predicate_id p) const { }

      inline hash_table* get_hash_table(const vm::predicate_id p) {}
      inline const hash_table* get_hash_table(const vm::predicate_id p) const {}

      inline bool stored_as_hash_table(const vm::predicate *pred) const { return types.get_bit(pred->get_id()); }

      inline void add_fact(vm::tuple *tpl, vm::predicate *pred, vm::rule_matcher& m)
      {
      }

      inline void increment_database(vm::predicate *pred, tuple_list *ls, vm::rule_matcher& m)
      {}

      inline void improve_index(void)
      {
         while(expand) {
            hash_table *next(expand->next_expand);
            expand->next_expand = NULL;
            if(expand->too_crowded())
               expand->expand();
            expand = next;
         }
      }

      inline void cleanup_index(void)
      {
      }

      inline void rebuild_index(void)
      {
      }

      explicit linear_store(void)
      {
         vm::bitmap::create(types, vm::theProgram->num_predicates_next_uint());
         types.clear(vm::theProgram->num_predicates_next_uint());
         data = mem::allocator<utils::byte>().allocate(ITEM_SIZE * vm::theProgram->num_predicates());
         for(size_t i(0); i < vm::theProgram->num_predicates(); ++i) {
            utils::byte *p(data + ITEM_SIZE * i);
            mem::allocator<tuple_list>().construct((tuple_list*)p);
         }
         expand = NULL;
      }

      inline void destroy(void) {
         for(size_t i(0); i < vm::theProgram->num_predicates(); ++i) {
            utils::byte *p(data + ITEM_SIZE * i);
            if(types.get_bit(i)) {
               hash_table *table((hash_table*)p);
               for(hash_table::iterator it(table->begin()); !it.end(); ++it) {
                  tuple_list *ls(*it);
                  for(tuple_list::iterator it2(ls->begin()), end(ls->end()); it2 != end; ) {
                     vm::tuple *tpl(*it2);
                     it2++;
                     vm::tuple::destroy(tpl, vm::theProgram->get_predicate(i));
                  }
               }
               // turn into list
               table->destroy();
               mem::allocator<tuple_list>().construct((tuple_list*)table);
               types.unset_bit(i);
            } else {
               tuple_list *ls((tuple_list*)p);
               for(tuple_list::iterator it(ls->begin()), end(ls->end()); it != end; ) {
                  vm::tuple *tpl(*it);
                  ++it;
                  vm::tuple::destroy(tpl, vm::theProgram->get_predicate(i));
               }
               ls->clear();
            }
         }
      }

      ~linear_store(void)
      {
         destroy();
         for(size_t i(0); i < vm::theProgram->num_predicates(); ++i) {
            utils::byte *p(data + ITEM_SIZE * i);
            if(types.get_bit(i)) {
               hash_table *table((hash_table*)p);
               table->destroy();
            }
            // nothing to destroy at the list level
         }
         mem::allocator<utils::byte>().deallocate(data, ITEM_SIZE * vm::theProgram->num_predicates());
         vm::bitmap::destroy(types, vm::theProgram->num_predicates_next_uint());
      }
};

}

#endif
