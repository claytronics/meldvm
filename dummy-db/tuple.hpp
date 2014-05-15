
#ifndef DB_TUPLE_HPP
#define DB_TUPLE_HPP

#include "conf.hpp"

#include <vector>
#include <ostream>
#include <list>

#ifdef COMPILE_MPI
#include <boost/mpi.hpp>
#endif

#if 0
//////////////////////////////
#ifdef __cplusplus
extern "C" {
#endif

VM_tuple* DB_get_tuple(simpleTuple* s);

#ifdef __cplusplus
}
#endif
////////////////////////////// c++ version
VM_tuple* DB_get_tuple(simpleTuple* s)
{
return s->get_tuple();
}
#endif


#include "vm/defs.hpp"
#include "vm/predicate.hpp"
#include "vm/tuple.hpp"
#include "mem/allocator.hpp"
#include "mem/base.hpp"
#include "utils/types.hpp"

namespace db
{
   
class simple_tuple: public mem::base
{
public:

   MEM_METHODS(simple_tuple)

   VM_tuple* get_tuple(void);
   
   VM_strat_level get_strat_level(void);

   VM_predicate* get_predicate(void);
   
   VM_predicate_id get_predicate_id(void);

   bool is_aggregate(void);
  void set_as_aggregate(void);

   void print(std::ostream&);

   VM_derivation_count get_count(void);
   bool reached_zero(void) { return get_count() == 0; }
   void inc_count(VM_derivation_count& inc);
   void dec_count(VM_derivation_count& inc);
   void add_count(VM_derivation_count& inc);

   VM_depth_t get_depth(void);
   
  size_t storage_size(void);
   
   void pack(UTILS_byte *, size_t, int *);
   
   static simple_tuple* unpack(VM_predicate *, UTILS_byte *, size_t, int *, VM_program *);

   static simple_tuple* create_new(VM_tuple *tuple, VM_predicate *pred, VM_depth_t depth);

   static simple_tuple* remove_new(VM_tuple *tuple, VM_predicate *pred, VM_depth_t depth);
   
   static void wipeout(simple_tuple *stpl) { VM_tuple_destroy(get_tuple(stpl), get_predicate(stpl)); delete stpl; }

  explicit simple_tuple(VM_tuple *_tuple, VM_predicate *_pred, VM_derivation_count _count, VM_depth_t _depth = 0);

  explicit simple_tuple(void);

  ~simple_tuple(void);
};

std::ostream& operator<<(std::ostream&, simple_tuple&);

typedef std::list<simple_tuple*, mem::allocator<simple_tuple*> > simple_tuple_list;
typedef std::vector<simple_tuple*, mem::allocator<simple_tuple*> > simple_tuple_vector;

}

#endif
