#ifndef C_DB_TUPLE_HPP
#define C_DB_TUPLE_HPP

#include "db/tuple.hpp"

#include "c-defs.hpp"

#ifdef __cplusplus
extern "C" {
#endif
  
  VM_tuple* DB_STPL_get_tuple(DB_simple_tuple *stpl);   
  VM_strat_level DB_STPL_get_strat_level(DB_simple_tuple *stpl);
  VM_predicate* DB_STPL_get_predicate(DB_simple_tuple *stpl);

  DB_simple_tuple* DB_STPL_create_new(VM_tuple *_tuple, VM_predicate *_pred, VM_depth_t depth);
  DB_simple_tuple* DB_STPL_remove_new(VM_tuple *_tuple, VM_predicate *_pred, VM_depth_t depth);  

  bool DB_STPL_is_aggregate(DB_simple_tuple *stpl);
  void DB_STPL_set_as_aggregate(DB_simple_tuple *stpl);

  VM_derivation_count DB_STPL_get_count(DB_simple_tuple *stpl);
  bool DB_STPL_reached_zero(DB_simple_tuple *stpl);
  //void DB_STPL_inc_count(VM_derivation_count& inc);
  //void DB_STPL_dec_count(VM_derivation_count& inc);
  //void DB_STPL_add_count(VM_derivation_count& inc);

  VM_depth_t DB_STPL_get_depth(DB_simple_tuple *stpl);
  size_t DB_STPL_storage_size(DB_simple_tuple *stpl);

  //void DB_STPL_print(std::ostream&);

#if 0                /* Functions not used */
  VM_predicate_id DB_STPL_get_predicate_id(DB_simple_tuple *stpl);
  void DB_STPL_pack(DB_simple_tuple* stpl, UTILS_byte *buf, size_t buf_size, int *pos);
  DB_simple_tuple* DB_STPL_unpack(DB_simple_tuple *stpl, VM_predicate *pred, UTILS_byte *buf, size_t buf_size, int *pos, VM_program *program);
#endif

#ifdef __cplusplus
}
#endif   

#endif
