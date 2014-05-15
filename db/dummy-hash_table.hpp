
#ifndef DB_HASH_TABLE_HPP
#define DB_HASH_TABLE_HPP

#include <assert.h>
#include <ostream>
#include <iostream>

#include "conf.hpp"
#include "vm/tuple.hpp"
#include "db/intrusive_list.hpp"
#include "mem/allocator.hpp"

namespace db
{

#define HASH_TABLE_INITIAL_TABLE_SIZE 8

typedef db::intrusive_list<vm::tuple> table_list;

struct hash_table
{
private:
  vm::uint_val hash_field(const vm::tuple_field field) const;

   public:

      hash_table *next_expand;

      inline size_t get_num_buckets(void) const {}
      inline vm::field_num get_hash_argument(void) const {}

      class iterator
      {
         private:

            table_list *bucket;
            table_list *finish;

            inline void find_good_bucket(void)
            {
               for(; bucket != finish; bucket++) {
                  if(!bucket->empty())
                     return;
               }
            }

         public:

            inline bool end(void) const { return bucket == finish; }

            inline db::intrusive_list<vm::tuple>* operator*(void) const
            {
               return bucket;
            }

            inline iterator& operator++(void)
            {
               bucket++;
               find_good_bucket();
               return *this;
            }

            inline iterator operator++(int)
            {
               bucket++;
               find_good_bucket();
               return *this;
            }

            explicit iterator(table_list *start_bucket, table_list *end_bucket):
               bucket(start_bucket), finish(end_bucket)
            {
               find_good_bucket();
            }
      };

      inline iterator begin(void) {}
      inline iterator begin(void) const {}

      inline size_t get_table_size() const {}

      size_t insert(vm::tuple *);
      size_t insert_front(vm::tuple *);

      inline db::intrusive_list<vm::tuple>* lookup_list(const vm::tuple_field field)
      {
         
      }

      inline void dump(std::ostream& out, const vm::predicate *pred) const
      {
      }

      inline void expand(void) {}
      inline void shrink(void) {}

      inline bool too_crowded(void) const
      {}

      inline bool smallest_possible(void) const {}

      inline bool too_sparse(void) const
      {
      }

      inline void setup(const vm::field_num field, const vm::field_type type,
            const size_t default_table_size = HASH_TABLE_INITIAL_TABLE_SIZE)
      {
       
      }

      inline void destroy(void)
      {
       
      }
};

}

#endif
