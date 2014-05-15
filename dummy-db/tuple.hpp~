
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
////////////////////////////////////////////////////////////////
for s in */*.hpp */*.cpp; do
   d=dummy-${s#}
   sed -e 's/VM_tuple/VM_tuple/g' < $s > $d
   sed -e 's/VM_predicate/VM_predicate/g' < $s > $d
done
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

   //   inline VM_tuple* get_tuple(void) const {}
   
   inline VM_strat_level get_strat_level(void) const {}

	inline VM_predicate* get_predicate(void) const {}
   
   inline VM_predicate_id get_predicate_id(void) const {}

   inline bool is_aggregate(void) const {}
   inline void set_as_aggregate(void) {}

   void print(std::ostream&) const;

   inline VM_derivation_count get_count(void) const {}
   inline bool reached_zero(void) const { return get_count() == 0; }
   inline void inc_count(const VM_derivation_count& inc) {}
   inline void dec_count(const VM_derivation_count& inc) {}
   inline void add_count(const VM_derivation_count& inc) {}

   inline VM_depth_t get_depth(void) const {}
   
  inline size_t storage_size(void) const {}
   
   void pack(UTILS_byte *, const size_t, int *) const;
   
   static simple_tuple* unpack(VM_predicate *, UTILS_byte *, const size_t, int *, VM_program *);

   static simple_tuple* create_new(VM_tuple *tuple, VM_predicate *pred, const VM_depth_t depth) {}

   static simple_tuple* remove_new(VM_tuple *tuple, VM_predicate *pred, const VM_depth_t depth) {}
   
   static void wipeout(simple_tuple *stpl) { VM_tuple_destroy(stpl->get_tuple(), stpl->get_predicate()); delete stpl; }

   explicit simple_tuple(VM_tuple *_tuple, VM_predicate *_pred, const VM_derivation_count _count, const VM_depth_t _depth = 0)
  {}

  explicit simple_tuple(void) // for serialization purposes
   {
   }

   ~simple_tuple(void);
};

std::ostream& operator<<(std::ostream&, const simple_tuple&);

typedef std::list<simple_tuple*, mem::allocator<simple_tuple*> > simple_tuple_list;
typedef std::vector<simple_tuple*, mem::allocator<simple_tuple*> > simple_tuple_vector;

}

#endif
