#ifndef DB_TUPLE_H
#define DB_TUPLE_H

#include "db/tuple.hpp"

#ifdef __cplusplus
extern "C" {
#endif
  VM_tuple* DB_get_tuple(DB_simple_tuple *stpl);   
  VM_strat_level DB_get_strat_level(DB_simple_tuple *stpl);
  
  VM_predicate* DB_get_predicate(DB_simple_tuple *stpl);
  
  VM_predicate_id DB_get_predicate_id(DB_simple_tuple *stpl);

  bool DB_is_aggregate(DB_simple_tuple *stpl);
  void DB_set_as_aggregate(DB_simple_tuple *stpl);

  //void DB_print(std::ostream&);

  VM_derivation_count DB_get_count(DB_simple_tuple *stpl);
  bool DB_reached_zero(DB_simple_tuple *stpl);
  //void DB_inc_count(VM_derivation_count& inc);
  //void DB_dec_count(VM_derivation_count& inc);
  //void DB_add_count(VM_derivation_count& inc);

  VM_depth_t DB_get_depth(DB_simple_tuple *stpl);
  
  size_t DB_storage_size(DB_simple_tuple *stpl);

  void DB_pack(DB_simple_tuple* stpl, UTILS_byte *buf, size_t buf_size, int *pos);
  static DB_simple_tuple* DB_unpack(DB_simple_tuple *stpl, VM_predicate *pred, UTILS_byte *buf, size_t buf_size, int *pos, VM_program *program);

  static DB_simple_tuple* DB_create_new(VM_tuple *tuple, VM_predicate *pred, VM_depth_t depth);
  static DB_simple_tuple* DB_remove_new(VM_tuple *tuple, VM_predicate *pred, VM_depth_t depth);  

  static void DB_wipeout(DB_simple_tuple *stpl);
#ifdef __cplusplus
}
#endif   

#endif
