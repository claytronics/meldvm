
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

      inline hash_table* get_table(const vm::predicate_id p)
      {
         return (hash_table*)(data + ITEM_SIZE * p);
      }

      inline hash_table* get_table(const vm::predicate_id p) const
      {
         return (hash_table*)(data + ITEM_SIZE * p);
      }

      inline hash_table* create_table(const vm::predicate *pred)
      {
         hash_table *table(get_table(pred->get_id()));
         mem::allocator<hash_table>().construct(table);
         const vm::field_num field(pred->get_hashed_field());
         table->setup(field, pred->get_field_type(field)->get_type());
         return table;
      }

      inline tuple_list* create_list(const vm::predicate *pred)
      {
         tuple_list *add(get_list(pred->get_id()));
         mem::allocator<tuple_list>().construct(add);
         return add;
      }

      inline tuple_list* get_list(const vm::predicate_id p)
      {
         return (tuple_list*)(data + ITEM_SIZE * p);
      }

      inline const tuple_list* get_list(const vm::predicate_id p) const
      {
         return (tuple_list*)(data + ITEM_SIZE * p);
      }

      inline hash_table *transform_hash_table_new_field(hash_table *tbl, const vm::predicate *pred)
      {
         const size_t size_table(tbl->get_num_buckets());
         hash_table new_hash;
         const vm::field_num field(pred->get_hashed_field());
         new_hash.setup(field, pred->get_field_type(field)->get_type(), size_table);

         hash_table::iterator it(tbl->begin());

         while(!it.end()) {
            tuple_list *bucket_ls(*it);
            ++it;

            for(tuple_list::iterator it2(bucket_ls->begin()), end(bucket_ls->end()); it2 != end; ++it2) {
               vm::tuple *tpl(*it2);

               new_hash.insert(tpl);
            }
         }

         tbl->destroy();
         // copy new
         memcpy(tbl, &new_hash, sizeof(hash_table));

         return tbl;
      }

      inline hash_table *transform_list_to_hash_table(table_list *ls, const vm::predicate *pred)
      {
         tuple_list::iterator it(ls->begin());
         tuple_list::iterator end(ls->end());
         hash_table *tbl(create_table(pred));

         while(it != end) {
            vm::tuple *tpl(*it);
            ++it;
            tbl->insert(tpl);
         }

         types.set_bit(pred->get_id());

         return tbl;
      }

      inline tuple_list* transform_hash_table_to_list(hash_table *tbl, const vm::predicate *pred)
      {
         hash_table::iterator it(tbl->begin());
         assert(tbl->next_expand == NULL);
         tuple_list ls;
         // cannot get list immediatelly since that would remove the pointer to the hash table data

         size_t total_size(0);
         while(!it.end()) {
            tuple_list *bucket_ls(*it);
            ++it;

            total_size += bucket_ls->get_size();
            ls.splice_back(*bucket_ls);
         }
         assert(ls.get_size() == total_size);

         tbl->destroy();
         memcpy(tbl, &ls, sizeof(tuple_list));

         types.unset_bit(pred->get_id());

         return (tuple_list*)tbl;
      }

      inline bool decide_if_expand(vm::rule_matcher& m, const vm::predicate *pred, hash_table *table) const
      {
         return m.get_count(pred->get_id()) >= table->get_num_buckets();
      }

   public:

      inline tuple_list* get_linked_list(const vm::predicate_id p) { return get_list(p); }
      inline const tuple_list* get_linked_list(const vm::predicate_id p) const { return get_list(p); }

      inline hash_table* get_hash_table(const vm::predicate_id p) { return get_table(p); }
      inline const hash_table* get_hash_table(const vm::predicate_id p) const { return get_table(p); }

      inline bool stored_as_hash_table(const vm::predicate *pred) const { return types.get_bit(pred->get_id()); }

      inline void add_fact(vm::tuple *tpl, vm::predicate *pred, vm::rule_matcher& m)
      {
         if(pred->is_hash_table()) {
            if(stored_as_hash_table(pred)) {
               hash_table *table(get_table(pred->get_id()));
               size_t size_bucket(table->insert(tpl));
               if(!table->next_expand && size_bucket >= CREATE_HASHTABLE_THREADSHOLD) {
                  if(decide_if_expand(m, pred, table)) {
                     assert(!table->next_expand);
                     table->next_expand = expand;
                     expand = table;
                  }
               }
            } else {
               // still using a list
               table_list *ls(get_list(pred->get_id()));

               if(ls->get_size() + 1 >= CREATE_HASHTABLE_THREADSHOLD) {
                  hash_table *table(transform_list_to_hash_table(ls, pred));
                  table->insert(tpl);
               } else
                  ls->push_back(tpl);
            }
         } else {
            if(stored_as_hash_table(pred)) {
               hash_table *table(get_table(pred->get_id()));
               table->insert(tpl);
            } else {
               table_list *ls(get_list(pred->get_id()));
               ls->push_back(tpl);
            }
         }
      }

      inline void increment_database(vm::predicate *pred, tuple_list *ls, vm::rule_matcher& m)
      {
         if(pred->is_hash_table()) {
            if(stored_as_hash_table(pred)) {
               hash_table *table(get_table(pred->get_id()));
               bool big_bucket(false);
               for(tuple_list::iterator it(ls->begin()), end(ls->end()); it != end;) {
                  vm::tuple *tpl(*it);
                  it++;
                  const size_t size_bucket(table->insert(tpl));
                  if(size_bucket >= CREATE_HASHTABLE_THREADSHOLD)
                     big_bucket = true;
               }
               ls->clear();
               if(!table->next_expand && big_bucket) {
                  if(decide_if_expand(m, pred, table)) {
                     assert(!table->next_expand);
                     table->next_expand = expand;
                     expand = table;
                  }
               }
            } else {
               table_list *add(get_list(pred->get_id()));

               if(add->get_size() + ls->get_size() >= CREATE_HASHTABLE_THREADSHOLD) {
                  hash_table *table(transform_list_to_hash_table(add, pred));
                  for(tuple_list::iterator it(ls->begin()), end(ls->end()); it != end; ) {
                     vm::tuple *tpl(*it);
                     it++;
                     table->insert(tpl);
                  }
                  ls->clear();
               } else
                  add->splice_back(*ls);
            }
         } else {
            if(stored_as_hash_table(pred)) {
               hash_table *table(get_table(pred->get_id()));
               for(tuple_list::iterator it(ls->begin()), end(ls->end()); it != end;) {
                  vm::tuple *tpl(*it);
                  it++;
                  table->insert(tpl);
               }
               ls->clear();
            } else {
               table_list *add(get_list(pred->get_id()));
               add->splice_back(*ls);
            }
         }
      }

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
         for(vm::bitmap::iterator it(types.begin(vm::theProgram->num_predicates())); !it.end(); ++it) {
            const vm::predicate_id id(*it);
            hash_table *tbl(get_table(id));

            if(tbl->too_sparse()) {
               if(tbl->smallest_possible())
                  transform_hash_table_to_list(tbl, vm::theProgram->get_predicate(id));
               else
                  tbl->shrink();
            }
         }
      }

      inline void rebuild_index(void)
      {
         for(size_t i(0); i < vm::theProgram->num_predicates(); ++i) {
            vm::predicate *pred(vm::theProgram->get_predicate(i));

            if(!pred->is_linear_pred())
               continue;

            if(pred->is_hash_table()) {
               if(stored_as_hash_table(pred)) {
                  hash_table *tbl(get_hash_table(i));
                  // check if using the correct argument
                  const vm::field_num old(tbl->get_hash_argument());
                  if(old != pred->get_hashed_field()) {
                     transform_hash_table_new_field(tbl, pred);
                  } else {
                     // do nothing
                  }
               } else {
                  tuple_list *ls(get_linked_list(i));
                  if(ls->get_size() >= CREATE_HASHTABLE_THREADSHOLD)
                     transform_list_to_hash_table(ls, pred);
               }
            } else {
               if(stored_as_hash_table(pred)) {
                  hash_table *tbl(get_hash_table(i));
                  transform_hash_table_to_list(tbl, pred);
               } else {
                  // do nothing...
               }
            }
         }
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
