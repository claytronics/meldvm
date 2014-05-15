
#ifndef DB_TRIE_HPP
#define DB_TRIE_HPP

#include <list>
#include <stack>
#include <tr1/unordered_map>
#include <ostream>

#include "mem/base.hpp"
#include "vm/tuple.hpp"
#include "vm/defs.hpp"
#include "db/tuple.hpp"
#include "vm/predicate.hpp"
#include "vm/match.hpp"
#include "vm/types.hpp"

namespace db
{
   
class trie_hash;
class trie_leaf;
class tuple_trie_leaf;

typedef std::list<simple_tuple*, mem::allocator<simple_tuple*> > simple_tuple_list;
typedef std::vector<tuple_trie_leaf*, mem::allocator<tuple_trie_leaf*> > tuple_vector;

class trie_node: public mem::base
{
public:

   MEM_METHODS(trie_node)

   trie_node *parent;
   trie_node *next;
   trie_node *prev;
   trie_node *child;

   vm::tuple_field data;
   
   bool hashed;
   trie_node **bucket;
   
   trie_node* get_by_int(const vm::int_val) const;
   trie_node* get_by_float(const vm::float_val) const;
   trie_node* get_by_node(const vm::node_val) const;
   
   void convert_hash(vm::type*);

   inline bool is_hashed(void) const { return hashed; }
   inline trie_hash* get_hash(void) const { return (trie_hash*)child; }
   inline bool is_leaf(void) const { return (vm::ptr_val)child & 0x1; }

   inline trie_node* get_next(void) const { return next; }
   inline trie_node* get_child(void) const { return (trie_node*)((vm::ptr_val)child & (~(vm::ptr_val)(0x1))); }
   inline trie_node* get_parent(void) const { return parent; }

   inline trie_leaf* get_leaf(void) const
   {
      return (trie_leaf *)get_child();
   }
   
   inline void set_leaf(trie_leaf *leaf)
   {
      child = (trie_node *)((vm::ptr_val)leaf | 0x1);
   }
   
   size_t count_refs(void) const;
   
   trie_node *match(const vm::tuple_field&, vm::type*, vm::match_stack&, size_t&) const;
   
   trie_node *insert(const vm::tuple_field&, vm::type*, vm::match_stack&);
   
   explicit trie_node(const vm::tuple_field& _data):
      parent(NULL),
      next(NULL),
      prev(NULL),
      child(NULL),
      data(_data),
      hashed(false),
      bucket(NULL)
   {
      assert(next == NULL && prev == NULL && parent == NULL && child == NULL && hashed == false);
   }

   explicit trie_node(void): // no data
      parent(NULL),
      next(NULL),
      prev(NULL),
      child(NULL),
      hashed(false),
      bucket(NULL)
   {
      assert(next == NULL && prev == NULL && parent == NULL && child == NULL && hashed == false);
   }

   ~trie_node(void);
};

class trie_hash: public mem::base
{
private:
  
public:

   MEM_METHODS(trie_hash)
   
   size_t count_refs(void) const;
   
   void insert_int(const vm::int_val&, trie_node *);
	void insert_uint(const vm::uint_val&, trie_node *);
   void insert_float(const vm::float_val&, trie_node *);
   void insert_node(const vm::node_val&, trie_node *);
   
   trie_node *get_int(const vm::int_val&) const;
   trie_node *get_float(const vm::float_val&) const;
   trie_node *get_node(const vm::node_val&) const;
	trie_node *get_uint(const vm::uint_val&) const;   

   void expand(void);
   
   explicit trie_hash(vm::type *, trie_node*);
   
   ~trie_hash(void);
};

class trie_leaf: public mem::base
{
private:
   
public:
   
   virtual vm::ref_count get_count(void) const = 0;
   
   virtual void add_new(const vm::depth_t depth, const vm::derivation_count many) = 0;

   virtual void sub(const vm::depth_t depth, const vm::derivation_count many) = 0;
   
   virtual bool to_delete(void) const = 0;
   
   explicit trie_leaf(void)
   {
   }

   virtual void destroy(vm::predicate*) { }
};

class depth_counter: public mem::base
{
private:
  typedef std::map<vm::depth_t, vm::ref_count> map_count;

   public:

  typedef map_count::const_iterator const_iterator;

      inline bool empty(void) const
      {
      }

      inline vm::ref_count get_count(const vm::depth_t depth) const
      {
      }

      inline const_iterator begin(void) const {}
      inline const_iterator end(void) const {}

      inline vm::depth_t max_depth(void) const
      {
      }
  
  inline vm::depth_t min_depth(void) const
  {}

      inline void add(const vm::depth_t depth, const vm::derivation_count count)
      {
      }

      // decrements a count of some depth
      // returns true if the count of such depth has gone to 0
      inline bool sub(const vm::depth_t depth, const vm::derivation_count count)
      {
      }

      // deletes all references above a certain depth
      // and returns the number of references deleted
      inline vm::ref_count delete_depths_above(const vm::depth_t depth)
      {
      }

      explicit depth_counter(void) {}
};

class tuple_trie_leaf: public trie_leaf
{
private:
public:

   MEM_METHODS(tuple_trie_leaf)
   
	inline vm::tuple *get_underlying_tuple(void) const {}
	
   virtual inline vm::ref_count get_count(void) const {}

   inline bool has_depth_counter(void) const {}

   inline depth_counter *get_depth_counter(void) const {}

   inline bool new_ref_use(void) {
   }

   inline void delete_ref_use(void) {
   }

   inline void reset_ref_use(void) {
   }

   inline vm::depth_t get_max_depth(void) const {
   }

   inline vm::depth_t get_min_depth(void) const {
   }

   inline depth_counter::const_iterator get_depth_begin(void) const {}
   inline depth_counter::const_iterator get_depth_end(void) const {}

   inline vm::ref_count delete_depths_above(const vm::depth_t depth)
   {
   }
   
   virtual inline void add_new(const vm::depth_t depth,
         const vm::derivation_count many)
   {}

   inline void checked_sub(const vm::derivation_count many)
   {}

   virtual inline void sub(const vm::depth_t depth,
         const vm::derivation_count many)
   {
   }
   
   virtual inline bool to_delete(void) const {}
   
   explicit tuple_trie_leaf(simple_tuple *_tpl) 
  {}

   virtual void destroy(vm::predicate *pred)
   {}
};

class tuple_trie_iterator: public mem::base
{
private:
   
public:

   MEM_METHODS(tuple_trie_iterator)
   
   inline tuple_trie_leaf* get_leaf(void) const
   {
   }
   
   inline tuple_trie_leaf* operator*(void) const
   {
   }
   
   inline bool operator==(const tuple_trie_iterator& it) const
   {
   
   }
   
   inline bool operator!=(const tuple_trie_iterator& it) const { return !operator==(it); }
   
   inline tuple_trie_iterator& operator++(void)
   {
   }
   
   inline tuple_trie_iterator operator++(int)
   {
   }
   
   explicit tuple_trie_iterator(tuple_trie_leaf *first_leaf)
   {
   }
   
  explicit tuple_trie_iterator(void)
   {
   }
};

class trie
{
protected:
   inline void basic_invariants(void)
   {
   }
   
   void commit_delete(trie_node *, vm::predicate *, const vm::ref_count);
   size_t delete_branch(trie_node *, vm::predicate *);
   void delete_path(trie_node *);
   void sanity_check(void) const;
   
   virtual trie_leaf* create_leaf(void *data, vm::predicate*, const vm::ref_count many, const vm::depth_t depth) = 0;
   void inner_delete_by_leaf(trie_leaf *, vm::predicate *, const vm::derivation_count, const vm::depth_t);
   
   trie_node *check_insert(void *, vm::predicate *, const vm::derivation_count, const vm::depth_t, vm::match_stack&, bool&);
   
public:
   
   class delete_info
   {
   private:
   public:

      inline bool to_delete(void) const {}
      inline bool is_valid(void) const {}

      void perform_delete(vm::predicate *pred)
      {
      }

      inline depth_counter* get_depth_counter(void) const
      {
      }

      inline vm::ref_count delete_depths_above(const vm::depth_t depth)
      {
      }
      
      explicit delete_info(tuple_trie_leaf *_leaf,
            trie *_tr,
            const bool _to_del,
            trie_node *_tr_node,
            const vm::derivation_count _many)
      {
      }
   };
   
   inline bool empty(void) const {}
   inline size_t size(void) const {}
   
   // if second argument is 0, the leaf is ensured to be deleted
   void delete_by_leaf(trie_leaf *, vm::predicate *, const vm::depth_t);
   void delete_by_index(vm::predicate *, const vm::match&);
   void wipeout(vm::predicate *);
   
   explicit trie(void);
};

struct trie_continuation_frame {
   vm::match_stack mstack;
	trie_node *parent;
   trie_node *node;
};

typedef utils::stack<trie_continuation_frame> trie_continuation_stack;

class tuple_trie: public trie, public mem::base
{
private:
public:

   void assert_used(void) {
   }

   MEM_METHODS(tuple_trie)
   
   typedef tuple_trie_iterator iterator;
   typedef tuple_trie_iterator const_iterator;

	// this search iterator uses either the leaf list
	// or a continuation stack to retrieve the next valid match
	// the leaf list is used because we want all the tuples from the trie
   class tuple_search_iterator: public mem::base
   {
      public:
         
         typedef enum {
            USE_LIST,
            USE_STACK,
            USE_END
         } iterator_type;

      private:
      public:
	
			void find_next(trie_continuation_frame&, const bool force_down = false);

         inline tuple_search_iterator& operator++(void)
         {
         }

		   inline tuple_trie_leaf* operator*(void) const
		   {
		   }

         inline bool operator==(const tuple_search_iterator& it) const
         {
         }
         
         inline bool operator!=(const tuple_search_iterator& it) const {}

         explicit tuple_search_iterator(tuple_trie_leaf *leaf)
         {
         }

         // end iterator
         explicit tuple_search_iterator(void){}

			// iterator for use with continuation stack
         explicit tuple_search_iterator(trie_continuation_frame& frm)
         {
				find_next(frm, true);
         }
   };

   
   // inserts tuple into the trie
   // returns false if tuple is repeated (+ ref count)
   // returns true if tuple is new
   bool insert_tuple(vm::tuple *, vm::predicate *, const vm::derivation_count, const vm::depth_t);
   
   // returns delete info object
   // call to_delete to know if the ref count reached zero
   // call the object to commit the deletion operation
   delete_info delete_tuple(vm::tuple *, vm::predicate *, const vm::derivation_count, const vm::depth_t);
   
   inline const_iterator begin(void) const {}
   inline const_iterator end(void) const {}
   
   inline iterator begin(void) {}
   inline iterator end(void) {}
   
   std::vector<std::string> get_print_strings(vm::predicate *) const;
   void print(std::ostream&, vm::predicate *) const;
   
   tuple_search_iterator match_predicate(const vm::match*) const;

   tuple_search_iterator match_predicate(void) const;
	static inline tuple_search_iterator match_end(void) {}
   
   explicit tuple_trie(void): trie() { basic_invariants(); }
   
   virtual ~tuple_trie(void) {}
};

class agg_configuration;

class agg_trie_leaf: public trie_leaf
{
private:
public:

   MEM_METHODS(agg_trie_leaf)
   
   inline void set_conf(agg_configuration* _conf) {}
   
   inline agg_configuration *get_conf(void) const {}
   
   virtual inline vm::ref_count get_count(void) const {}
   
   virtual inline void add_new(const vm::depth_t, const vm::derivation_count) { }
   virtual inline void sub(const vm::depth_t, const vm::derivation_count) { }
   
   inline void set_zero_refs(void) {}

  virtual inline bool to_delete(void) const {}
   
   explicit agg_trie_leaf(agg_configuration *_conf)
   {
   }
   
   virtual ~agg_trie_leaf(void);
};

class agg_trie_iterator: public mem::base
{
private:
public:

   MEM_METHODS(agg_trie_iterator)
   
   inline agg_configuration* operator*(void) const
   {
   }
   
   inline bool operator==(const agg_trie_iterator& it) const
   {
   }
   
   inline bool operator!=(const agg_trie_iterator& it) const { return !operator==(it); }
   
   inline agg_trie_iterator& operator++(void)
   {
   }
   
   inline agg_trie_iterator operator++(int)
   {
   }
   
   explicit agg_trie_iterator(agg_trie_leaf *first_leaf)
  {
  }
   
   explicit agg_trie_iterator(void)
  {
  }
};

class agg_trie: public trie, public mem::base
{
private:
public:

   MEM_METHODS(agg_trie)
   
   typedef agg_trie_iterator iterator;
   typedef agg_trie_iterator const_iterator;
   
   agg_trie_leaf *find_configuration(vm::tuple *, vm::predicate *);
   
   void delete_configuration(trie_node *node);
   
   inline const_iterator begin(void) const {}
   inline const_iterator end(void) const {}
   
   inline iterator begin(void) {}
   inline iterator end(void) {}
   
   iterator erase(iterator& it, vm::predicate *);
   
   explicit agg_trie(void) {}
   
   virtual ~agg_trie(void) {}
};
   
}

#endif
