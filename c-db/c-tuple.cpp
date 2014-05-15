#include "c-db/c-tuple.hpp"

VM_tuple* DB_get_tuple(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  return s->get_tuple();
}   

VM_strat_level DB_get_strat_level(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  return s->get_strat_level();
}
  
VM_predicate* DB_get_predicate(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  return s->get_predicate();
}
  
VM_predicate_id DB_get_predicate_id(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  return s->get_predicate_id();
}

bool DB_is_aggregate(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  return s->is_aggregate();
}

void DB_set_as_aggregate(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  s->set_as_aggregate();
}

//void DB_print(std::ostream&)
/*{

  }*/

VM_derivation_count DB_get_count(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  return s->get_count();
}

bool DB_reached_zero(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  return s->reached_zero();
}

//void DB_inc_count(VM_derivation_count& inc)
/*{

  }*/

//void DB_dec_count(VM_derivation_count& inc)
/*{

  }*/

//void DB_add_count(VM_derivation_count& inc)
/*{

  }*/

VM_depth_t DB_get_depth(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  return s->get_depth();
}
  
size_t DB_storage_size(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  return s->storage_size();
}

void DB_pack(DB_simple_tuple *stpl, UTILS_byte *buf, size_t buf_size, int *pos)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  s->pack(buf, buf_size, pos);
}
static DB_simple_tuple* unpack(DB_simple_tuple *stpl, VM_predicate *pred, UTILS_byte *buf, size_t buf_size, int *pos, VM_program *prog)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  vm::predicate *predicate = (vm::predicate*) pred;
  vm::program *program = (vm::program*) prog;
  return s->unpack(predicate, buf, buf_size, pos, program);
}

/* Dont know yet how I'll handle these ones.
static simple_tuple* DB_create_new(VM_tuple *tuple, VM_predicate *pred, VM_depth_t depth)
{
  
}

static simple_tuple* DB_remove_new(VM_tuple *tuple, VM_predicate *pred, VM_depth_t depth)
{

}  

static void DB_wipeout(simple_tuple *stpl)
{

}*/
