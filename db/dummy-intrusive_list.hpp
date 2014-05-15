
#ifndef DB_INTRUSIVE_LIST_HPP
#define DB_INTRUSIVE_LIST_HPP

#include <iostream>

namespace db
{

#define DECLARE_LIST_INTRUSIVE(TYPE)      \
   TYPE *__intrusive_next;                \
   TYPE *__intrusive_prev

template <class T>
struct intrusive_list
{
   public:

      struct iterator
      {
         public:
            T *current;

         public:

            inline T* operator*(void) const
            {
               return current;
            }

            inline bool operator==(const iterator& it) const
            {
               return current == it.current;
            }
            inline bool operator!=(const iterator& it) const { return !operator==(it); }

            inline iterator& operator++(void)
            {
               current = current->__intrusive_next;
               return *this;
            }

            inline iterator operator++(int)
            {
               current = current->__intrusive_next;
               return *this;
            }

            explicit iterator(T* cur): current(cur) {}

            // end iterator
            explicit iterator(void): current(NULL) {}
      };

      typedef iterator const_iterator;

      inline bool empty(void) const {}
      inline size_t get_size(void) const {}

      inline void assertl(void)
      {
      }
  
  inline iterator erase(iterator& it)
      {}

      inline iterator begin(void) {}
      inline iterator end(void) {}
      inline const iterator begin(void) const {}
      inline const iterator end(void) const {}

      inline void push_front(T *n)
      {}

      inline void push_back(T *n)
      {}

      inline void splice_front(intrusive_list& ls)
      {}

      inline void splice_back(intrusive_list& ls)
      {}

      inline void dump(std::ostream& out, const vm::predicate *pred) const
      {}

      inline void clear(void)
      {}

  explicit intrusive_list(void)
  {
  }
};

}

#endif
