#include "c-db/c-tuple.hpp"
#include "c-defs.hpp"

VM_tuple* DB_STPL_get_tuple(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  return s->get_tuple();
}   

VM_strat_level DB_STPL_get_strat_level(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  return s->get_strat_level();
}
  
VM_predicate* DB_STPL_get_predicate(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  return s->get_predicate();
}

#if 0
// NOT CALLED ANYWHERE  
VM_predicate_id DB_STPL_get_predicate_id(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  return s->get_predicate_id();
}
#endif

bool DB_STPL_is_aggregate(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  return s->is_aggregate();
}

void DB_STPL_set_as_aggregate(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  s->set_as_aggregate();
}

/*void DB_STPL_print(DB_simple_tuple *stpl)
{
  stpl->data->print(cout // also replace vm::tuple::print?
  }*/

VM_derivation_count DB_STPL_get_count(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  return s->get_count();
}

bool DB_STPL_reached_zero(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  return s->reached_zero();
}

//void DB_STPL_inc_count(VM_derivation_count& inc)
/*{

  }*/

//void DB_STPL_dec_count(VM_derivation_count& inc)
/*{

  }*/

//void DB_STPL_add_count(VM_derivation_count& inc)
/*{

  }*/

VM_depth_t DB_STPL_get_depth(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  return s->get_depth();
}
  
size_t DB_STPL_storage_size(DB_simple_tuple *stpl)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  return s->storage_size();
}

DB_simple_tuple* DB_STPL_create_new(VM_tuple *_tuple, VM_predicate *_pred, VM_depth_t depth)
{
  vm::tuple *tuple = (vm::tuple*) _tuple;
  vm::predicate *pred = (vm::predicate*) _pred;
  //return new simple_tuple(tuple, pred, 1, depth);
}

DB_simple_tuple* DB_STPL_remove_new(VM_tuple *_tuple, VM_predicate *_pred, VM_depth_t depth)
{
  vm::tuple *tuple = (vm::tuple*) _tuple;
  vm::predicate *pred = (vm::predicate*) _pred;
  //return new simple_tuple(tuple, pred, -1, depth);
}  


#if 0
void DB_STPL_pack(DB_simple_tuple *stpl, UTILS_byte *buf, size_t buf_size, int *pos)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  s->pack(buf, buf_size, pos);
}

static DB_simple_tuple* DB_STPL_unpack(DB_simple_tuple *stpl, VM_predicate *pred, UTILS_byte *buf, size_t buf_size, int *pos, VM_program *prog)
{
  db::simple_tuple *s = (db::simple_tuple*) stpl;
  vm::predicate *predicate = (vm::predicate*) pred;
  vm::program *program = (vm::program*) prog;
  return s->unpack(predicate, buf, buf_size, pos, program);
}
#endif

#if 0
// Dont know yet how I'll handle these ones.
static void DB_STPL_wipeout(simple_tuple *stpl)
{

}
#endif
