
#include <iostream>
#include <algorithm>
#include <functional>
#include <cmath>
#include <sstream>

#include "vm/exec.hpp"
#include "vm/tuple.hpp"
#include "vm/match.hpp"
#include "db/tuple.hpp"
#include "process/machine.hpp"
#include "sched/nodes/thread_intrusive.hpp"
#ifdef USE_UI
#include "ui/manager.hpp"
#endif
#ifdef USE_JIT
#include "jit/build.hpp"
#endif

#if 0
#define DEBUG_INSTRS
#define DEBUG_RULES
#define DEBUG_SENDS
#define DEBUG_REMOVE
#define DEBUG_ITERS
#endif

#define COMPUTED_GOTOS

#if defined(DEBUG_SENDS)
static boost::mutex print_mtx;
#endif

using namespace vm;
using namespace vm::instr;
using namespace std;
using namespace db;
using namespace runtime;
using namespace utils;

namespace vm
{
   
enum return_type {
   RETURN_OK,
   RETURN_SELECT,
   RETURN_NEXT,
   RETURN_LINEAR,
   RETURN_DERIVED,
	RETURN_END_LINEAR,
   RETURN_NO_RETURN
};

static inline return_type execute(pcounter, state&, const reg_num, tuple*, predicate*);

static inline node_val
get_node_val(pcounter& m, state& state)
{
   (void)state;

   const node_val ret(pcounter_node(m));
   
   pcounter_move_node(&m);

   assert(ret <= All->DATABASE->max_id());
   
   return ret;
}

static inline node_val
get_node_val(const pcounter& m, state& state)
{
   (void)state;
   return pcounter_node(m);
}

static inline tuple*
get_tuple_field(state& state, const pcounter& pc)
{
   return state.get_tuple(val_field_reg(pc));
}

static inline void
execute_alloc(const pcounter& pc, state& state)
{
   predicate *pred(theProgram->get_predicate(alloc_predicate(pc)));
   tuple *tuple(vm::tuple::create(pred));
   const reg_num reg(alloc_reg(pc));

   state.preds[reg] = pred;

   state.set_tuple(reg, tuple);
}

static inline void
execute_add_linear0(tuple *tuple, predicate *pred, state& state)
{
   state.node->add_linear_fact(tuple, pred);
   state.linear_facts_generated++;
#ifdef INSTRUMENTATION
   state.instr_facts_derived++;
#endif
}

static inline void
execute_add_linear(pcounter& pc, state& state)
{
   const reg_num r(pcounter_reg(pc + instr_size));
   predicate *pred(state.preds[r]);
   tuple *tuple(state.get_tuple(r));
   assert(!pred->is_reused_pred());
   assert(!pred->is_action_pred());
   assert(pred->is_linear_pred());

#ifdef USE_UI
   if(state::UI) {
      LOG_LINEAR_DERIVATION(state.node, tuple);
   }
#endif
#ifdef DEBUG_SENDS
   cout << "\tadd linear ";
   tuple->print(cout, pred);
   cout << endl;
#endif

   execute_add_linear0(tuple, pred, state);
}

static inline void
execute_add_persistent0(tuple *tpl, predicate *pred, state& state)
{
#ifdef USE_UI
   if(state::UI) {
      if(tpl->is_linear()) {
         LOG_LINEAR_DERIVATION(state.node, tpl);
      } else {
         LOG_PERSISTENT_DERIVATION(state.node, tpl);
      }
   }
#endif
#ifdef DEBUG_SENDS
   cout << "\tadd persistent ";
   tpl->print(cout, pred);
   cout << endl;
#endif

   assert(pred->is_persistent_pred() || pred->is_reused_pred());
   simple_tuple *stuple(new simple_tuple(tpl, pred, state.count, state.depth));
   state.store->persistent_tuples.push_back(stuple);
   state.persistent_facts_generated++;
#ifdef INSTRUMENTATION
   state.instr_facts_derived++;
#endif
}

static inline void
execute_add_persistent(pcounter& pc, state& state)
{
   const reg_num r(pcounter_reg(pc + instr_size));

   // tuple is either persistent or linear reused
   execute_add_persistent0(state.get_tuple(r), state.preds[r], state);
}

static inline void
execute_run_action0(tuple *tpl, predicate *pred, state& state)
{
   assert(pred->is_action_pred());
   if(state.count > 0)
      All->MACHINE->run_action(state.sched, state.node, tpl, pred);
   else
      vm::tuple::destroy(tpl, pred);
}

static inline void
execute_run_action(pcounter& pc, state& state)
{
   const reg_num r(pcounter_reg(pc + instr_size));
   execute_run_action0(state.get_tuple(r), state.preds[r], state);
}

static inline void
execute_enqueue_linear0(tuple *tuple, predicate *pred, state& state)
{
   assert(pred->is_linear_pred());
#ifdef DEBUG_SENDS
   cout << "\tenqueue ";
   tuple->print(cout, pred);
   cout << endl;
#endif

   state.store->add_generated(tuple, pred);
   state.store->register_tuple_fact(pred, 1);
   state.generated_facts = true;
   state.linear_facts_generated++;
#ifdef INSTRUMENTATION
   state.instr_facts_derived++;
#endif
}

static inline void
execute_enqueue_linear(pcounter& pc, state& state)
{
   const reg_num r(pcounter_reg(pc + instr_size));

   execute_enqueue_linear0(state.get_tuple(r), state.preds[r], state);
}

static inline void
execute_send(const pcounter& pc, state& state)
{
   const reg_num msg(send_msg(pc));
   const reg_num dest(send_dest(pc));
   const node_val dest_val(state.get_node(dest));
   predicate *pred(state.preds[msg]);
   tuple *tuple(state.get_tuple(msg));

   if(state.count < 0 && pred->is_linear_pred() && !pred->is_reused_pred()) {
      vm::tuple::destroy(tuple, pred);
      return;
   }

#ifdef CORE_STATISTICS
   state.stat.stat_predicate_proven[pred->get_id()]++;
#endif

   assert(msg != dest);
#ifdef DEBUG_SENDS
   print_mtx.lock();
   ostringstream ss;
   node_val print_val(dest_val);
#ifdef USE_REAL_NODES
   print_val = ((db::node*)print_val)->get_id();
#endif
   ss << "\t";
   tuple->print(ss, pred);
   ss << " " << state.count << " -> " << print_val << " (" << state.depth << ")" << endl;
   cout << ss.str();
   print_mtx.unlock();
#endif
#ifdef USE_UI
   if(state::UI) {
      LOG_TUPLE_SEND(state.node, All->DATABASE->find_node((node::node_id)dest_val), tuple);
   }
#endif
#ifdef USE_REAL_NODES
   if(state.node == (db::node*)dest_val)
#else
   if(state.node->get_id() == dest_val)
#endif
   {
      // same node
      if(pred->is_action_pred()) {
         execute_run_action0(tuple, pred, state);
      } else if(pred->is_persistent_pred() || pred->is_reused_pred()) {
         execute_add_persistent0(tuple, pred, state);
      } else {
         execute_enqueue_linear0(tuple, pred, state);
      }
   } else {
      All->MACHINE->route(state.node, state.sched, (node::node_id)dest_val, tuple, pred, state.count, state.depth);
#ifdef INSTRUMENTATION
      state.instr_facts_derived++;
#endif
   }
}

static inline void
execute_send_delay(const pcounter& pc, state& state)
{
   const reg_num msg(send_delay_msg(pc));
   const reg_num dest(send_delay_dest(pc));
   const node_val dest_val(state.get_node(dest));
   predicate *pred(state.preds[msg]);
   tuple *tuple(state.get_tuple(msg));

#ifdef CORE_STATISTICS
   state.stat.stat_predicate_proven[pred->get_id()]++;
#endif

   if(msg == dest) {
#ifdef DEBUG_SENDS
      cout << "\t";
      tuple->print(cout, pred);
      cout << " -> self " << state.node->get_id() << endl;
#endif
      state.sched->new_work_delay(state.node, state.node, tuple, pred, state.count, state.depth, send_delay_time(pc));
   } else {
#ifdef DEBUG_SENDS
      cout << "\t";
      tuple->print(cout, pred);
      cout << " -> " << dest_val << endl;
#endif
#ifdef USE_UI
      if(state::UI) {
         LOG_TUPLE_SEND(state.node, All->DATABASE->find_node((node::node_id)dest_val), tuple);
      }
#endif
      All->MACHINE->route(state.node, state.sched, (node::node_id)dest_val, tuple, pred, state.count, state.depth, send_delay_time(pc));
   }
}

static inline void
execute_not(pcounter& pc, state& state)
{
   const reg_num op(not_op(pc));
   const reg_num dest(not_dest(pc));

   state.set_bool(dest, !state.get_bool(op));
}

static inline bool
do_match(predicate *pred, const tuple *tuple, const field_num& field, const instr_val& val,
   pcounter &pc, const state& state)
{
   if(val_is_reg(val)) {
      const reg_num reg(val_reg(val));
      
      switch(pred->get_field_type(field)->get_type()) {
         case FIELD_INT: return tuple->get_int(field) == state.get_int(reg);
         case FIELD_FLOAT: return tuple->get_float(field) == state.get_float(reg);
         case FIELD_NODE: return tuple->get_node(field) == state.get_node(reg);
         default: throw vm_exec_error("matching with non-primitive types in registers is unsupported");
      }
   } else if(val_is_field(val)) {
      const vm::tuple *tuple2(state.get_tuple(val_field_reg(pc)));
      const field_num field2(val_field_num(pc));
      
      pcounter_move_field(&pc);
      
      switch(pred->get_field_type(field)->get_type()) {
         case FIELD_INT: return tuple->get_int(field) == tuple2->get_int(field2);
         case FIELD_FLOAT: return tuple->get_float(field) == tuple2->get_float(field2);
         case FIELD_NODE: return tuple->get_node(field) == tuple2->get_node(field2);
         default: throw vm_exec_error("matching with non-primitive types in fields is unsupported");
      }
   } else if(val_is_nil(val))
      return runtime::cons::is_null(tuple->get_cons(field));
   else if(val_is_host(val))
      return tuple->get_node(field) == state.node->get_id();
   else if(val_is_int(val)) {
      const int_val i(pcounter_int(pc));
      
      pcounter_move_int(&pc);
      
      return tuple->get_int(field) == i;
   } else if(val_is_float(val)) {
      const float_val flt(pcounter_float(pc));
      
      pcounter_move_float(&pc);
      
      return tuple->get_float(field) == flt;
   } else if(val_is_non_nil(val))
      return !runtime::cons::is_null(tuple->get_cons(field));
   else
      throw vm_exec_error("match value in iter is not valid");
}

static bool
do_rec_match(match_field m, tuple_field x, type *t)
{
   switch(t->get_type()) {
      case FIELD_INT:
         if(FIELD_INT(m.field) != FIELD_INT(x))
            return false;
         break;
      case FIELD_FLOAT:
         if(FIELD_FLOAT(m.field) != FIELD_FLOAT(x))
            return false;
         break;
      case FIELD_NODE:
         if(FIELD_NODE(m.field) != FIELD_NODE(x))
            return false;
         break;
      case FIELD_LIST:
         if(FIELD_PTR(m.field) == 0) {
            if(!runtime::cons::is_null(FIELD_CONS(x)))
               return false;
         } else if(FIELD_PTR(m.field) == 1) {
            if(runtime::cons::is_null(FIELD_CONS(x)))
               return false;
         } else {
            runtime::cons *ls(FIELD_CONS(x));
            if(runtime::cons::is_null(ls))
               return false;
            list_match *lm(FIELD_LIST_MATCH(m.field));
            list_type *lt((list_type*)t);
            if(lm->head.exact && !do_rec_match(lm->head, ls->get_head(), lt->get_subtype()))
               return false;
            tuple_field tail;
            SET_FIELD_CONS(tail, ls->get_tail());
            if(lm->tail.exact && !do_rec_match(lm->tail, tail, lt))
               return false;
         }
         break;
      default:
         throw vm_exec_error("can't match this argument");
   }
   return true;
}

static inline bool
do_matches(match* m, const tuple *tuple, predicate *pred)
{
   if(!m->any_exact)
      return true;

   for(size_t i(0); i < pred->num_fields(); ++i) {
      if(m->has_match(i)) {
         type *t = pred->get_field_type(i);
         match_field mf(m->get_match(i));
         if(!do_rec_match(mf, tuple->get_field(i), t))
            return false;
      }
      
   }
   return true;
}

static void
build_match_element(instr_val val, match* m, type *t, match_field *mf, pcounter& pc, state& state, size_t& count)
{
   switch(t->get_type()) {
      case FIELD_INT:
         if(val_is_field(val)) {
            const reg_num reg(val_field_reg(pc));
            const tuple *tuple(state.get_tuple(reg));
            const field_num field(val_field_num(pc));

            pcounter_move_field(&pc);
            const int_val i(tuple->get_int(field));
            m->match_int(mf, i);
            const variable_match_template vmt = {reg, field, mf};
            m->add_variable_match(vmt, count);
            ++count;
         } else if(val_is_int(val)) {
            const int_val i(pcounter_int(pc));
            m->match_int(mf, i);
            pcounter_move_int(&pc);
         } else
            throw vm_exec_error("cannot use value for matching int");
         break;
      case FIELD_FLOAT: {
         if(val_is_field(val)) {
            const reg_num reg(val_field_reg(pc));
            const tuple *tuple(state.get_tuple(reg));
            const field_num field(val_field_num(pc));

            pcounter_move_field(&pc);
            const float_val f(tuple->get_float(field));
            m->match_float(mf, f);
            const variable_match_template vmt = {reg, field, mf};
            m->add_variable_match(vmt, count);
            ++count;
         } else if(val_is_float(val)) {
            const float_val f(pcounter_float(pc));
            m->match_float(mf, f);
            pcounter_move_float(&pc);
         } else
            throw vm_exec_error("cannot use value for matching float");
      }
      break;
      case FIELD_NODE:
         if(val_is_field(val)) {
            const reg_num reg(val_field_reg(pc));
            const tuple *tuple(state.get_tuple(reg));
            const field_num field(val_field_num(pc));

            pcounter_move_field(&pc);
            const node_val n(tuple->get_node(field));
            m->match_node(mf, n);
            const variable_match_template vmt = {reg, field, mf};
            m->add_variable_match(vmt, count);
            ++count;
         } else if(val_is_node(val)) {
            const node_val n(pcounter_node(pc));
            m->match_node(mf, n);
            pcounter_move_node(&pc);
         } else
            throw vm_exec_error("cannot use value for matching node");
         break;
      case FIELD_LIST:
         if(val_is_any(val)) {
            mf->exact = false;
            // do nothing
         } else if(val_is_nil(val)) {
            m->match_nil(mf);
         } else if(val_is_non_nil(val)) {
            m->match_non_nil(mf);
         } else if(val_is_list(val)) {
            list_type *lt((list_type*)t);
            list_match *lm(mem::allocator<list_match>().allocate(1));
            lm->init(lt);
            const instr_val head(val_get(pc, 0));
            pcounter_move_byte(&pc);
            if(!val_is_any(head))
               build_match_element(head, m, lt->get_subtype(), &(lm->head), pc, state, count);
            const instr_val tail(val_get(pc, 0));
            pcounter_move_byte(&pc);
            if(!val_is_any(tail))
               build_match_element(tail, m, t, &(lm->tail), pc, state, count);
            m->match_list(mf, lm, t);
         } else
            throw vm_exec_error("invalid field type for ITERATE/FIELD_LIST");
      break;
      default: throw vm_exec_error("invalid field type for ITERATE");
   }
}

static inline void
build_match_object(match* m, pcounter pc, const predicate *pred, state& state, size_t matches)
{
   type *t;
   size_t count(0);

   for(size_t i(0); i < matches; ++i) {
      const field_num field(iter_match_field(pc));
      const instr_val val(iter_match_val(pc));
      
      pcounter_move_match(&pc);

      t = pred->get_field_type(field);
      build_match_element(val, m, t, m->get_update_match(field), pc, state, count);
   }
}

static size_t
count_var_match_element(instr_val val, type *t, pcounter& pc)
{
   switch(t->get_type()) {
      case FIELD_INT:
         if(val_is_field(val)) {
            pcounter_move_field(&pc);
            return 1;
         } else if(val_is_int(val))
            pcounter_move_int(&pc);
         else
            throw vm_exec_error("cannot use value for matching int");
         break;
      case FIELD_FLOAT: {
         if(val_is_field(val)) {
            pcounter_move_field(&pc);
            return 1;
         } else if(val_is_float(val))
            pcounter_move_float(&pc);
         else
            throw vm_exec_error("cannot use value for matching float");
      }
      break;
      case FIELD_NODE:
         if(val_is_field(val)) {
            pcounter_move_field(&pc);
            return 1;
         } else if(val_is_node(val))
            pcounter_move_node(&pc);
         else
            throw vm_exec_error("cannot use value for matching node");
         break;
      case FIELD_LIST:
         if(val_is_any(val)) {
         } else if(val_is_nil(val)) {
         } else if(val_is_non_nil(val)) {
         } else if(val_is_list(val)) {
            list_type *lt((list_type*)t);
            const instr_val head(val_get(pc, 0));
            pcounter_move_byte(&pc);
            size_t ret(0);
            if(!val_is_any(head))
               ret += count_var_match_element(head, lt->get_subtype(), pc);
            const instr_val tail(val_get(pc, 0));
            pcounter_move_byte(&pc);
            if(!val_is_any(tail))
               ret += count_var_match_element(tail, t, pc);
            return ret;
         } else
            throw vm_exec_error("invalid field type for ITERATE/FIELD_LIST");
      break;
      default: throw vm_exec_error("invalid field type for ITERATE");
   }
   return 0;
}

static inline size_t
count_variable_match_elements(pcounter pc, const predicate *pred, const size_t matches)
{
   type *t;
   size_t ret(0);
   
   for(size_t i(0); i < matches; ++i) {
      const field_num field(iter_match_field(pc));
      const instr_val val(iter_match_val(pc));
      
      pcounter_move_match(&pc);

      t = pred->get_field_type(field);
      ret += count_var_match_element(val, t, pc);
   }
   return ret;
}

static inline bool
sort_tuples(const tuple* t1, const tuple* t2, const field_num field, const predicate *pred)
{
   assert(t1 != NULL && t2 != NULL);

   switch(pred->get_field_type(field)->get_type()) {
      case FIELD_INT:
         return t1->get_int(field) < t2->get_int(field);
      case FIELD_FLOAT:
         return t1->get_float(field) < t2->get_float(field);
      case FIELD_NODE:
         return t1->get_node(field) < t2->get_node(field);
      default:
         throw vm_exec_error("don't know how to compare this field type (tuple_sorter)");
   }
   return false;
}

typedef struct {
   tuple *tpl;
   db::intrusive_list<vm::tuple>::iterator iterator;
} iter_object;
typedef vector<iter_object, mem::allocator<iter_object> > vector_iter;

class tuple_sorter
{
private:
	const predicate *pred;
	const field_num field;
	
public:
	
	inline bool operator()(const iter_object& l1, const iter_object& l2)
	{
      return sort_tuples(l1.tpl, l2.tpl, field, pred);
	}
	
	explicit tuple_sorter(const field_num _field, const predicate *_pred):
		pred(_pred), field(_field)
	{}
};

class tuple_leaf_sorter
{
private:
	const predicate *pred;
	const field_num field;
	
public:
	
	inline bool operator()(const tuple_trie_leaf* l1, const tuple_trie_leaf* l2)
	{
      return sort_tuples(l1->get_underlying_tuple(), l2->get_underlying_tuple(), field, pred);
	}
	
	explicit tuple_leaf_sorter(const field_num _field, const predicate *_pred):
		pred(_pred), field(_field)
	{}
};

static inline match*
retrieve_match_object(state& state, pcounter pc, const predicate *pred, const size_t base)
{
   utils::byte *mdata((utils::byte*)iter_match_object(pc));
   match *mobj(NULL);

   if(mdata == NULL) {
      const size_t size = All->NUM_THREADS;
      const size_t matches = iter_matches_size(pc, base);
      const size_t var_size = count_variable_match_elements(pc + base, pred, matches);
      const size_t mem = match::mem_size(pred, var_size);
      mdata = mem::allocator<utils::byte>().allocate(size * mem);
      for(size_t i(0); i < size; ++i) {
         match *m((match*)(mdata + mem * i));
         m->init(pred, var_size);
         build_match_object(m, pc + base, pred, state, matches);
      }
      state.matches_created.push_back((match*)mdata);
      iter_match_object_set(pc, (ptr_val)mdata);
      mobj = (match*)(mdata + mem * state.sched->get_id());
   } else {
      match *m((match*)mdata);
      mobj = (match*)(mdata + m->mem_size() * state.sched->get_id());
      if(!iter_constant_match(pc)) {
         for(size_t i(0); i < mobj->var_size; ++i) {
            variable_match_template& tmp(mobj->get_variable_match(i));
            tuple *tpl(state.get_tuple(tmp.reg));
            tmp.match->field = tpl->get_field(tmp.field);
         }
      }
   }
   assert(mobj);
#ifdef DYNAMIC_INDEXING
   if(state.sched && state.sched->get_id() == 0 && mobj->size() > 0 && pred->is_linear_pred()) {
      // increment statistics for matching
      const int start(pred->get_argument_position());
      const int end(start + pred->num_fields() - 1);
      for(int i(0); i < (int)mobj->size(); ++i) {
         if(mobj->has_match(i)) {
            state.match_counter->increment_count(start + i, start, end);
         }
      }
   }
#endif
   return mobj;
}

// iterate macros
#define PUSH_CURRENT_STATE(TUPLE, TUPLE_LEAF, TUPLE_QUEUE, NEW_DEPTH)		\
	state.is_linear = this_is_linear || state.is_linear;                    \
   state.depth = !pred->is_cycle_pred() ? old_depth : max((NEW_DEPTH)+1, old_depth)
#define POP_STATE()								\
   state.is_linear = old_is_linear;       \
   state.depth = old_depth
#define TO_FINISH(ret) ((ret) == RETURN_LINEAR || (ret) == RETURN_DERIVED)

static inline return_type
execute_pers_iter(const reg_num reg, match* m, const pcounter first, state& state, predicate *pred)
{
   const depth_t old_depth(state.depth);
   const bool old_is_linear(state.is_linear);
   const bool this_is_linear(false);

   tuple_trie::tuple_search_iterator tuples_it = state.node->match_predicate(pred->get_id(), m);
   for(tuple_trie::tuple_search_iterator end(tuple_trie::match_end());
         tuples_it != end;
         ++tuples_it)
   {
      tuple_trie_leaf *tuple_leaf(*tuples_it);

      // we get the tuple later since the previous leaf may have been deleted
      tuple *match_tuple(tuple_leaf->get_underlying_tuple());
      assert(match_tuple != NULL);

#ifdef TRIE_MATCHING_ASSERT
      assert(do_matches(m, match_tuple, pred));
#endif

      PUSH_CURRENT_STATE(match_tuple, tuple_leaf, NULL, tuple_leaf->get_min_depth());
#ifdef DEBUG_ITERS
      cout << "\t use ";
      match_tuple->print(cout, pred);
      cout << "\n";
#endif

      return_type ret = execute(first, state, reg, match_tuple, pred);

      POP_STATE();

      if(ret == RETURN_LINEAR) return ret;
      else if(ret == RETURN_DERIVED && state.is_linear)
         return RETURN_DERIVED;
   }

   return RETURN_NO_RETURN;
}

static inline return_type
execute_olinear_iter(const reg_num reg, match* m, const pcounter pc, const pcounter first, state& state, predicate *pred)
{
   const depth_t old_depth(state.depth);
   const bool old_is_linear(state.is_linear);
   const bool this_is_linear(true);
   const utils::byte options(iter_options(pc));
   const utils::byte options_arguments(iter_options_argument(pc));

   vector_iter tpls;

   db::intrusive_list<vm::tuple> *local_tuples(state.lstore->get_linked_list(pred->get_id()));
#ifdef CORE_STATISTICS
   execution_time::scope s(state.stat.ts_search_time_predicate[pred->get_id()]);
#endif
   for(db::intrusive_list<vm::tuple>::iterator it(local_tuples->begin()), end(local_tuples->end());
         it != end; ++it)
   {
      tuple *tpl(*it);
      if(!tpl->must_be_deleted() && do_matches(m, tpl, pred)) {
         iter_object obj;
         obj.tpl = tpl;
         obj.iterator = it;
         tpls.push_back(obj);
      }
   }

   if(iter_options_random(options))
      utils::shuffle_vector(tpls, state.randgen);
   else if(iter_options_min(options)) {
      const field_num field(iter_options_min_arg(options_arguments));

      sort(tpls.begin(), tpls.end(), tuple_sorter(field, pred));
   } else throw vm_exec_error("do not know how to use this selector");

   for(vector_iter::iterator it(tpls.begin()); it != tpls.end(); ) {
      iter_object p(*it);
      return_type ret;

      tuple *match_tuple(p.tpl);

      state::removed_hash::const_iterator found(state.removed.find(match_tuple));
      if(found != state.removed.end()) {
         // tuple already removed
         state.removed.erase(found);
         goto next_tuple;
      }

      if(match_tuple->must_be_deleted())
         goto next_tuple;

      match_tuple->will_delete();

      PUSH_CURRENT_STATE(match_tuple, NULL, match_tuple, (vm::depth_t)0);
      state.hash_removes = true;

      ret = execute(first, state, reg, match_tuple, pred);

      state.hash_removes = false;
      POP_STATE();

      if(TO_FINISH(ret)) {
         db::intrusive_list<vm::tuple>::iterator it(p.iterator);
         local_tuples->erase(it);
         vm::tuple::destroy(match_tuple, pred);
         if(ret == RETURN_LINEAR)
            return RETURN_LINEAR;
         if(ret == RETURN_DERIVED && old_is_linear)
            return RETURN_DERIVED;
      } else {
         match_tuple->will_not_delete();
      }
next_tuple:
      // removed item from the list because it is no longer needed
      it = tpls.erase(it);
   }

   return RETURN_NO_RETURN;
}

static inline return_type
execute_orlinear_iter(const reg_num reg, match* m, const pcounter pc, const pcounter first, state& state, predicate *pred)
{
   const depth_t old_depth(state.depth);
   const bool old_is_linear(state.is_linear);
   const bool this_is_linear(false);
   const utils::byte options(iter_options(pc));
   const utils::byte options_arguments(iter_options_argument(pc));

   vector_iter tpls;

   db::intrusive_list<vm::tuple> *local_tuples(state.lstore->get_linked_list(pred->get_id()));
#ifdef CORE_STATISTICS
   execution_time::scope s(state.stat.ts_search_time_predicate[pred->get_id()]);
#endif
   for(db::intrusive_list<vm::tuple>::iterator it(local_tuples->begin()), end(local_tuples->end());
         it != end; ++it)
   {
      tuple *tpl(*it);
      if(!tpl->must_be_deleted() && do_matches(m, tpl, pred)) {
         iter_object obj;
         obj.tpl = tpl;
         obj.iterator = it;
         tpls.push_back(obj);
      }
   }

   if(iter_options_random(options))
      utils::shuffle_vector(tpls, state.randgen);
   else if(iter_options_min(options)) {
      const field_num field(iter_options_min_arg(options_arguments));

      sort(tpls.begin(), tpls.end(), tuple_sorter(field, pred));
   } else throw vm_exec_error("do not know how to use this selector");

   for(vector_iter::iterator it(tpls.begin()); it != tpls.end(); ) {
      iter_object p(*it);
      return_type ret;

      tuple *match_tuple(p.tpl);

      state::removed_hash::const_iterator found(state.removed.find(match_tuple));
      if(found != state.removed.end()) {
         // tuple already removed
         state.removed.erase(found);
         goto next_tuple;
      }

      if(match_tuple->must_be_deleted())
         goto next_tuple;

      match_tuple->will_delete();

      PUSH_CURRENT_STATE(match_tuple, NULL, match_tuple, (vm::depth_t)0);
      state.hash_removes = true;

      ret = execute(first, state, reg, match_tuple, pred);

      state.hash_removes = false;
      POP_STATE();

      match_tuple->will_not_delete();

      if(ret == RETURN_LINEAR)
         return RETURN_LINEAR;
      if(ret == RETURN_DERIVED && old_is_linear)
         return RETURN_DERIVED;
next_tuple:
      // removed item from the list because it is no longer needed
      it = tpls.erase(it);
   }

   return RETURN_NO_RETURN;
}

static inline return_type
execute_opers_iter(const reg_num reg, match* m, const pcounter pc, const pcounter first, state& state, predicate *pred)
{
   const depth_t old_depth(state.depth);
   const bool old_is_linear(state.is_linear);
   const bool this_is_linear(false);
   const utils::byte options(iter_options(pc));
   const utils::byte options_arguments(iter_options_argument(pc));

   typedef vector<tuple_trie_leaf*, mem::allocator<tuple_trie_leaf*> > vector_leaves;
   vector_leaves leaves;

   tuple_trie::tuple_search_iterator tuples_it = state.node->match_predicate(pred->get_id(), m);

   for(tuple_trie::tuple_search_iterator end(tuple_trie::match_end());
         tuples_it != end; ++tuples_it)
   {
      tuple_trie_leaf *tuple_leaf(*tuples_it);
#ifdef TRIE_MATCHING_ASSERT
      assert(do_matches(m, tuple_leaf->get_underlying_tuple(), pred));
#endif
      leaves.push_back(tuple_leaf);
   }

   if(iter_options_random(options))
      utils::shuffle_vector(leaves, state.randgen);
   else if(iter_options_min(options)) {
      const field_num field(iter_options_min_arg(options_arguments));

      sort(leaves.begin(), leaves.end(), tuple_leaf_sorter(field, pred));
   } else throw vm_exec_error("do not know how to use this selector");

   for(vector_leaves::iterator it(leaves.begin());
         it != leaves.end(); )
   {
      tuple_trie_leaf *tuple_leaf(*it);

      tuple *match_tuple(tuple_leaf->get_underlying_tuple());
      assert(match_tuple != NULL);

      PUSH_CURRENT_STATE(match_tuple, tuple_leaf, NULL, tuple_leaf->get_min_depth());

      return_type ret(execute(first, state, reg, match_tuple, pred));

      POP_STATE();

      if(ret == RETURN_LINEAR)
         return RETURN_LINEAR;
      else if(ret == RETURN_DERIVED && old_is_linear)
         return RETURN_DERIVED;
      else
         it = leaves.erase(it);
   }

   return RETURN_NO_RETURN;
}

static inline return_type
execute_linear_iter_list(const reg_num reg, match* m, const pcounter first, state& state, predicate* pred, db::intrusive_list<vm::tuple> *local_tuples, hash_table *tbl = NULL)
{
   if(local_tuples == NULL)
      return RETURN_NO_RETURN;

   const bool old_is_linear(state.is_linear);
   const bool this_is_linear(true);
   const depth_t old_depth(state.depth);

   for(db::intrusive_list<vm::tuple>::iterator it(local_tuples->begin()), end(local_tuples->end());
         it != end; )
   {
      tuple *match_tuple(*it);

      if(match_tuple->must_be_deleted()) {
         it++;
         continue;
      }

      {
#ifdef CORE_STATISTICS
         execution_time::scope s2(state.stat.ts_search_time_predicate[pred->get_id()]);
#endif
         if(!do_matches(m, match_tuple, pred)) {
            it++;
            continue;
         }
      }

      PUSH_CURRENT_STATE(match_tuple, NULL, match_tuple, (vm::depth_t)0);
#ifdef DEBUG_ITERS
      cout << "\tuse ";
      match_tuple->print(cout, pred);
      cout << "\n";
#endif

      match_tuple->will_delete(); // this will avoid future uses of this tuple!

      return_type ret(execute(first, state, reg, match_tuple, pred));

      POP_STATE();

      bool next_iter = true;

      if(TO_FINISH(ret)) {
         if(match_tuple->is_updated()) {
            match_tuple->will_not_delete();
            match_tuple->set_not_updated();
            if(reg > 0) {
               // if this is the first iterate, we do not need to send this to the generate list
               it = local_tuples->erase(it);
               state.store->add_generated(match_tuple, pred);
               state.generated_facts = true;
               next_iter = false;
            } else {
               if (tbl) {
                  // may need to re hash tuple
                  // it is not a problem if the tuple gets in the same bucket (will appear at the end of the list)
                  it = local_tuples->erase(it);
                  // add the tuple to the back of the bucket
                  // that way, we do not see it again if it's added to the same bucket
                  tbl->insert_front(match_tuple);
                  next_iter = false;
               }
            }
         } else {
            it = local_tuples->erase(it);
            vm::tuple::destroy(match_tuple, pred);
            next_iter = false;
         }
      }

      if(ret == RETURN_LINEAR)
         return RETURN_LINEAR;
      else if(old_is_linear && ret == RETURN_DERIVED)
         return RETURN_DERIVED;
      else if(next_iter) {
         match_tuple->will_not_delete();
         it++;
      }
   }
   return RETURN_NO_RETURN;
}

static inline return_type
execute_linear_iter(const reg_num reg, match* m, const pcounter first, state& state, predicate *pred)
{
   if(state.lstore->stored_as_hash_table(pred)) {
      const field_num hashed(pred->get_hashed_field());
      hash_table *table(state.lstore->get_hash_table(pred->get_id()));

      if(table == NULL)
         return RETURN_NO_RETURN;

#if 0
      cout << "Table" << endl;
      table->dump(cout, pred);
#endif

      if(m->has_match(hashed)) {
         const match_field mf(m->get_match(hashed));
         db::intrusive_list<vm::tuple> *local_tuples(table->lookup_list(mf.field));
         return_type ret(execute_linear_iter_list(reg, m, first, state, pred, local_tuples, table));
         return ret;
      } else {
         // go through hash table
         for(hash_table::iterator it(table->begin()); !it.end(); ++it) {
            db::intrusive_list<vm::tuple> *local_tuples(*it);
            return_type ret(execute_linear_iter_list(reg, m, first, state, pred, local_tuples, table));
            if(ret != RETURN_NO_RETURN)
               return ret;
         }
         return RETURN_NO_RETURN;
      }
   } else {
      db::intrusive_list<vm::tuple> *local_tuples(state.lstore->get_linked_list(pred->get_id()));
      return execute_linear_iter_list(reg, m, first, state, pred, local_tuples);
   }

   return RETURN_NO_RETURN;
}

static inline return_type
execute_rlinear_iter_list(const reg_num reg, match* m, const pcounter first, state& state, predicate *pred, db::intrusive_list<vm::tuple> *local_tuples)
{
   const bool old_is_linear(state.is_linear);
   const bool this_is_linear(false);
   const depth_t old_depth(state.depth);

   for(db::intrusive_list<vm::tuple>::iterator it(local_tuples->begin()), end(local_tuples->end());
         it != end; ++it)
   {
      tuple *match_tuple(*it);

      if(match_tuple->must_be_deleted())
         continue;

      {
#ifdef CORE_STATISTICS
         execution_time::scope s2(state.stat.ts_search_time_predicate[pred->get_id()]);
#endif
         if(!do_matches(m, match_tuple, pred))
            continue;
      }

      PUSH_CURRENT_STATE(match_tuple, NULL, match_tuple, (vm::depth_t)0);

      match_tuple->will_delete(); // this will avoid future uses of this tuple!

      return_type ret(execute(first, state, reg, match_tuple, pred));

      POP_STATE();

      match_tuple->will_not_delete();

      if(ret == RETURN_LINEAR)
         return RETURN_LINEAR;
      else if(old_is_linear && ret == RETURN_DERIVED)
         return RETURN_DERIVED;
   }

   return RETURN_NO_RETURN;
}

static inline return_type
execute_rlinear_iter(const reg_num reg, match* m, const pcounter first, state& state, predicate *pred)
{
   if(state.lstore->stored_as_hash_table(pred)) {
      const field_num hashed(pred->get_hashed_field());
      hash_table *table(state.lstore->get_hash_table(pred->get_id()));

      if(m->has_match(hashed)) {
         const match_field mf(m->get_match(hashed));
         db::intrusive_list<vm::tuple> *local_tuples(table->lookup_list(mf.field));
         return_type ret(execute_rlinear_iter_list(reg, m, first, state, pred, local_tuples));
         return ret;
      } else {
         // go through hash table
         for(hash_table::iterator it(table->begin()); !it.end(); ++it) {
            db::intrusive_list<vm::tuple> *local_tuples(*it);
            return_type ret(execute_rlinear_iter_list(reg, m, first, state, pred, local_tuples));
            if(ret != RETURN_NO_RETURN)
               return ret;
         }
         return RETURN_NO_RETURN;
      }
   } else {
      db::intrusive_list<vm::tuple> *local_tuples(state.lstore->get_linked_list(pred->get_id()));
      return execute_rlinear_iter_list(reg, m, first, state, pred, local_tuples);
   }
}

static inline void
execute_testnil(pcounter pc, state& state)
{
   const reg_num op(test_nil_op(pc));
   const reg_num dest(test_nil_dest(pc));

   runtime::cons *x(state.get_cons(op));

   state.set_bool(dest, runtime::cons::is_null(x));
}

static inline void
execute_float(pcounter& pc, state& state)
{
   const reg_num src(pcounter_reg(pc + instr_size));
   const reg_num dst(pcounter_reg(pc + instr_size + reg_val_size));

   state.set_float(dst, static_cast<float_val>(state.get_int(src)));
}

static inline pcounter
execute_select(pcounter pc, state& state)
{
   if(state.node->get_id() > All->DATABASE->static_max_id())
      return pc + select_size(pc);

   const pcounter hash_start(select_hash_start(pc));
   const code_size_t hashed(select_hash(hash_start, state.node->get_id()));
   
   if(hashed == 0) // no specific code
      return pc + select_size(pc);
   else
      return select_hash_code(hash_start, select_hash_size(pc), hashed);
}

#if 0
static inline void
execute_delete(const pcounter pc, state& state)
{
   const predicate_id id(delete_predicate(pc));
   const predicate *pred(All->PROGRAM->get_predicate(id));
   pcounter m(pc + DELETE_BASE);
   const size_t num_args(delete_num_args(pc));
   match mobj(pred);
   
   assert(state.node != NULL);
   assert(num_args > 0);
   int_val idx;
   
   for(size_t i(0); i < num_args; ++i) {
      const field_num fil_ind(delete_index(m));
      const instr_val fil_val(delete_val(m));
      
      assert(fil_ind == i);
      
      m += index_size + val_size;
      
      switch(pred->get_field_type(fil_ind)->get_type()) {
         case FIELD_INT:
            idx = get_op_function<int_val>(fil_val, m, state);
            mobj.match_int(mobj.get_update_match(fil_ind), idx);
            break;
         case FIELD_FLOAT:
            mobj.match_float(mobj.get_update_match(fil_ind), get_op_function<float_val>(fil_val, m, state));
            break;
         case FIELD_NODE:
            mobj.match_node(mobj.get_update_match(fil_ind), get_op_function<node_val>(fil_val, m, state));
            break;
         default: assert(false);
      }
   }
   
   //cout << "Removing from " << pred->get_name() << " iteration " << idx << " node " << state.node->get_id() << endl;
   
   state.node->delete_by_index(pred, mobj);
}
#endif

static inline void
execute_remove(pcounter pc, state& state)
{
   const reg_num reg(pcounter_reg(pc + instr_size));
   vm::tuple *tpl(state.get_tuple(reg));
   vm::predicate *pred(state.preds[reg]);

#ifdef USE_UI
   if(state::UI) {
      LOG_LINEAR_CONSUMPTION(state.node, tpl);
   }
#endif
#ifdef CORE_STATISTICS
   state.stat.stat_predicate_success[pred->get_id()]++;
#endif

#ifdef DEBUG_REMOVE
   cout << "\tdelete ";
   tpl->print(cout, pred);
   cout << endl;
#endif
   assert(tpl != NULL);
		
   // the else case for deregistering the tuple is done in execute_iter
   if(state.hash_removes)
      state.removed.insert(tpl);

   if(state.count > 0) {
      state.store->deregister_tuple_fact(pred, state.count);
      if(pred->is_reused_pred())
         state.store->persistent_tuples.push_back(new simple_tuple(tpl, pred, -1, state.depth));
      state.linear_facts_consumed++;
#ifdef INSTRUMENTATION
      state.instr_facts_consumed++;
#endif
   }
}

static inline void
execute_update(pcounter pc, state& state)
{
   const reg_num reg(pcounter_reg(pc + instr_size));
   vm::tuple *tpl(state.get_tuple(reg));
   vm::predicate *pred(state.preds[reg]);

   tpl->set_updated();
#ifdef DEBUG_SENDS
   cout << "\tupdate ";
   tpl->print(cout, pred);
   cout << endl;
#endif
   state.store->matcher.mark(pred);
}

static inline void
set_call_return(const reg_num reg, const tuple_field ret, external_function* f, state& state)
{
   type *ret_type(f->get_return_type());
   assert(ret_type);
   switch(ret_type->get_type()) {
      case FIELD_INT:
         state.set_int(reg, FIELD_INT(ret));
         break;
      case FIELD_FLOAT:
         state.set_float(reg, FIELD_FLOAT(ret));
         break;
      case FIELD_NODE:
         state.set_node(reg, FIELD_NODE(ret));
         break;
		case FIELD_STRING: {
			rstring::ptr s(FIELD_STRING(ret));
			
			state.set_string(reg, s);
			state.add_string(s);
			
			break;
		}
      case FIELD_LIST: {
         cons *l(FIELD_CONS(ret));

         state.set_cons(reg, l);
         if(!cons::is_null(l))
            state.add_cons(l);
         break;
      }
      case FIELD_STRUCT: {
         struct1 *s(FIELD_STRUCT(ret));

         state.set_struct(reg, s);
         state.add_struct(s);
         break;
      }
      default:
         throw vm_exec_error("invalid return type in call (set_call_return)");
   }
}

static inline void
execute_call0(pcounter& pc, state& state)
{
   const external_function_id id(call_extern_id(pc));
   external_function *f(lookup_external_function(id));

   assert(f->get_num_args() == 0);

   argument ret = f->get_fun_ptr()();
   set_call_return(call_dest(pc), ret, f, state);
}

static inline void
execute_call1(pcounter& pc, state& state)
{
   const external_function_id id(call_extern_id(pc));
   external_function *f(lookup_external_function(id));

   assert(f->get_num_args() == 1);

   argument ret = ((external_function_ptr1)f->get_fun_ptr())(state.get_reg(pcounter_reg(pc + call_size)));
   set_call_return(call_dest(pc), ret, f, state);
}

static inline void
execute_call2(pcounter& pc, state& state)
{
   const external_function_id id(call_extern_id(pc));
   external_function *f(lookup_external_function(id));

   assert(f->get_num_args() == 2);

   argument ret = ((external_function_ptr2)f->get_fun_ptr())(state.get_reg(pcounter_reg(pc + call_size)),
         state.get_reg(pcounter_reg(pc + call_size + reg_val_size)));
   set_call_return(call_dest(pc), ret, f, state);
}

static inline void
execute_call3(pcounter& pc, state& state)
{
   const external_function_id id(call_extern_id(pc));
   external_function *f(lookup_external_function(id));

   assert(f->get_num_args() == 3);

   argument ret = ((external_function_ptr3)f->get_fun_ptr())(state.get_reg(pcounter_reg(pc + call_size)),
         state.get_reg(pcounter_reg(pc + call_size + reg_val_size)),
         state.get_reg(pcounter_reg(pc + call_size + 2 * reg_val_size)));
   set_call_return(call_dest(pc), ret, f, state);
}

static inline argument
do_call(external_function *f, argument *args)
{
   switch(f->get_num_args()) {
      case 0:
         return f->get_fun_ptr()();
      case 1:
         return ((external_function_ptr1)f->get_fun_ptr())(args[0]);
      case 2:
         return ((external_function_ptr2)f->get_fun_ptr())(args[0], args[1]);
      case 3:
         return ((external_function_ptr3)f->get_fun_ptr())(args[0], args[1], args[2]);
      default:
         throw vm_exec_error("vm does not support external functions with more than 3 arguments");
   }
   
   // undefined
   argument ret;
   return ret;
}

static inline void
execute_call(pcounter& pc, state& state)
{
   const external_function_id id(call_extern_id(pc));
   external_function *f(lookup_external_function(id));
   const size_t num_args(call_num_args(pc));
   argument args[num_args];
   
   pcounter m(pc + CALL_BASE);
   for(size_t i(0); i < num_args; ++i) {
      args[i] = state.get_reg(pcounter_reg(m));
      m += reg_val_size;
   }
   
   assert(num_args == f->get_num_args());
   
   set_call_return(call_dest(pc), do_call(f, args), f, state);
}

static inline void
execute_calle(pcounter pc, state& state)
{
   const external_function_id id(calle_extern_id(pc) + first_custom_external_function());
   const size_t num_args(calle_num_args(pc));
   external_function *f(lookup_external_function(id));
   argument args[num_args];

   pcounter m(pc + CALLE_BASE);
   for(size_t i(0); i < num_args; ++i) {
      args[i] = state.get_reg(pcounter_reg(m));
      m += reg_val_size;
   }
   
   assert(num_args == f->get_num_args());

   set_call_return(calle_dest(pc), do_call(f, args), f, state);
}

static inline void
execute_set_priority(pcounter& pc, state& state)
{
   const reg_num prio_reg(pcounter_reg(pc + instr_size));
   const reg_num node_reg(pcounter_reg(pc + instr_size + reg_val_size));
   const float_val prio(state.get_float(prio_reg));
   const node_val node(state.get_node(node_reg));

#ifdef USE_REAL_NODES
   state.sched->set_node_priority((db::node*)node, prio);
#else
   state.sched->set_node_priority(All->DATABASE->find_node(node), prio);
#endif
}

static inline void
execute_set_priority_here(pcounter& pc, state& state)
{
   const reg_num prio_reg(pcounter_reg(pc + instr_size));
   const float_val prio(state.get_float(prio_reg));

   state.sched->set_node_priority(state.node, prio);
}

static inline void
execute_add_priority(pcounter& pc, state& state)
{
   const reg_num prio_reg(pcounter_reg(pc + instr_size));
   const reg_num node_reg(pcounter_reg(pc + instr_size + reg_val_size));
   const float_val prio(state.get_float(prio_reg));
   const node_val node(state.get_node(node_reg));

#ifdef USE_REAL_NODES
   state.sched->add_node_priority((db::node*)node, prio);
#else
   state.sched->add_node_priority(All->DATABASE->find_node(node), prio);
#endif
}

static inline void
execute_add_priority_here(pcounter& pc, state& state)
{
   const reg_num prio_reg(pcounter_reg(pc + instr_size));
   const float_val prio(state.get_float(prio_reg));

   state.sched->add_node_priority(state.node, prio);
}

static inline void
execute_cpu_id(pcounter& pc, state& state)
{
   const reg_num node_reg(pcounter_reg(pc + instr_size));
   const reg_num dest_reg(pcounter_reg(pc + instr_size + reg_val_size));
   const node_val nodeval(state.get_node(node_reg));
#ifdef USE_REAL_NODES
   db::node *node((db::node*)nodeval);
#else
   db::node *node(All->DATABASE->find_node(nodeval));
#endif

   state.set_int(dest_reg, node->get_owner()->get_id());
}

static inline void
execute_node_priority(pcounter& pc, state& state)
{
   const reg_num node_reg(pcounter_reg(pc + instr_size));
   const reg_num dest_reg(pcounter_reg(pc + instr_size + reg_val_size));
   const node_val nodeval(state.get_node(node_reg));
#ifdef USE_REAL_NODES
   db::node *node((db::node*)nodeval);
#else
   db::node *node(All->DATABASE->find_node(nodeval));
#endif

   sched::thread_intrusive_node *tn((sched::thread_intrusive_node *)node);
   state.set_float(dest_reg, tn->get_float_priority_level());
}

static inline void
execute_rule(const pcounter& pc, state& state)
{
   const size_t rule_id(rule_get_id(pc));

   state.current_rule = rule_id;

#ifdef USE_UI
   if(state::UI) {
      vm::rule *rule(theProgram->get_rule(state.current_rule));
      LOG_RULE_START(state.node, rule);
   }
#endif

#ifdef CORE_STATISTICS
	if(state.stat.stat_rules_activated == 0 && state.stat.stat_inside_rule) {
		state.stat.stat_rules_failed++;
	}
	state.stat.stat_inside_rule = true;
	state.stat.stat_rules_activated = 0;
#endif
}

static inline void
execute_rule_done(const pcounter& pc, state& state)
{
   (void)pc;
   (void)state;

#ifdef USE_UI
   if(state::UI) {
      vm::rule *rule(theProgram->get_rule(state.current_rule));
      LOG_RULE_APPLIED(state.node, rule);
   }
#endif

#ifdef CORE_STATISTICS
	state.stat.stat_rules_ok++;
	state.stat.stat_rules_activated++;
#endif

#if 0
   const string rule_str(state.PROGRAM->get_rule_string(state.current_rule));

   cout << "Rule applied " << rule_str << endl;
#endif
}

static inline void
execute_new_node(const pcounter& pc, state& state)
{
   const reg_num reg(new_node_reg(pc));
   node *new_node(All->DATABASE->create_node());

   state.sched->init_node(new_node);

   state.set_node(reg, new_node->get_id());

#ifdef USE_UI
   if(state::UI) {
      LOG_NEW_NODE(new_node);
   }
#endif
}

static inline tuple_field
axiom_read_data(pcounter& pc, type *t)
{
   tuple_field f;

   switch(t->get_type()) {
      case FIELD_INT:
         SET_FIELD_INT(f, pcounter_int(pc));
         pcounter_move_int(&pc);
         break;
      case FIELD_FLOAT:
         SET_FIELD_FLOAT(f, pcounter_float(pc));
         pcounter_move_float(&pc);
         break;
      case FIELD_NODE:
         SET_FIELD_NODE(f, pcounter_node(pc));
         pcounter_move_node(&pc);
         break;
      case FIELD_LIST:
         if(*pc == 0) {
            pc++;
            SET_FIELD_CONS(f, runtime::cons::null_list());
         } else if(*pc == 1) {
            pc++;
            list_type *lt((list_type*)t);
            tuple_field head(axiom_read_data(pc, lt->get_subtype()));
            tuple_field tail(axiom_read_data(pc, t));
            runtime::cons *c(FIELD_CONS(tail));
            runtime::cons *nc(cons::create(c, head, lt));
            SET_FIELD_CONS(f, nc);
         } else {
            assert(false);
         }
         break;

      default: assert(false);
   }

   return f;
}

static inline void
execute_new_axioms(pcounter pc, state& state)
{
   const pcounter end(pc + new_axioms_jump(pc));
   pc += NEW_AXIOMS_BASE;

   while(pc < end) {
      // read axions until the end!
      predicate_id pid(predicate_get(pc, 0));
      predicate *pred(theProgram->get_predicate(pid));
      tuple *tpl(vm::tuple::create(pred));

      pc++;

      for(size_t i(0), num_fields(pred->num_fields());
            i != num_fields;
            ++i)
      {
         tuple_field field(axiom_read_data(pc, pred->get_field_type(i)));

         switch(pred->get_field_type(i)->get_type()) {
            case FIELD_LIST:
               tpl->set_cons(i, FIELD_CONS(field));
               break;
            case FIELD_STRUCT:
               tpl->set_struct(i, FIELD_STRUCT(field));
               break;
            case FIELD_INT:
            case FIELD_FLOAT:
            case FIELD_NODE:
               tpl->set_field(i, field);
               break;
            case FIELD_STRING:
               tpl->set_string(i, FIELD_STRING(field));
               break;
            default:
               throw vm_exec_error("don't know how to handle this type (execute_new_axioms)");
         }
      }

      if(pred->is_action_pred())
         execute_run_action0(tpl, pred, state);
      else if(pred->is_reused_pred() || pred->is_persistent_pred())
         execute_add_persistent0(tpl, pred, state);
      else
         execute_add_linear0(tpl, pred, state);
   }
}

static inline void
execute_make_structr(pcounter& pc, state& state)
{
   struct_type *st((struct_type*)theProgram->get_type(make_structr_type(pc)));
   const reg_num to(pcounter_reg(pc + instr_size + type_size));

   struct1 *s(struct1::create(st));

   for(size_t i(0); i < st->get_size(); ++i)
      s->set_data(i, *state.stack.get_stack_at(i));
   state.stack.pop(st->get_size());

   state.set_struct(to, s);
   state.add_struct(s);
}

static inline void
execute_make_structf(pcounter& pc, state& state)
{
   predicate *pred(state.preds[val_field_reg(pc + instr_size)]);
   tuple *tuple(get_tuple_field(state, pc + instr_size));
   const field_num field(val_field_num(pc + instr_size));
   struct_type *st((struct_type*)pred->get_field_type(field));

   struct1 *s(struct1::create(st));

   for(size_t i(0); i < st->get_size(); ++i)
      s->set_data(i, *state.stack.get_stack_at(i));
   state.stack.pop(st->get_size());

   tuple->set_struct(field, s);
}

static inline void
execute_struct_valrr(pcounter& pc, state& state)
{
   const size_t idx(struct_val_idx(pc));
   const reg_num src(pcounter_reg(pc + instr_size + count_size));
   const reg_num dst(pcounter_reg(pc + instr_size + count_size + reg_val_size));
   state.set_reg(dst, state.get_struct(src)->get_data(idx));
}

static inline void
execute_struct_valfr(pcounter& pc, state& state)
{
   const size_t idx(struct_val_idx(pc));
   tuple *tuple(get_tuple_field(state, pc + instr_size + count_size));
   const field_num field(val_field_num(pc + instr_size + count_size));
   const reg_num dst(pcounter_reg(pc + instr_size + count_size + field_size));
   struct1 *s(tuple->get_struct(field));
   state.set_reg(dst, s->get_data(idx));
}

static inline void
execute_struct_valrf(pcounter& pc, state& state)
{
   const size_t idx(struct_val_idx(pc));
   const reg_num src(pcounter_reg(pc + instr_size + count_size));
   tuple *tuple(get_tuple_field(state, pc + instr_size + count_size + reg_val_size));
   const field_num field(val_field_num(pc + instr_size + count_size + reg_val_size));
   struct1 *s(state.get_struct(src));
   tuple->set_field(field, s->get_data(idx));
}

static inline void
execute_struct_valrfr(pcounter& pc, state& state)
{
   const size_t idx(struct_val_idx(pc));
   const reg_num src(pcounter_reg(pc + instr_size + count_size));
   tuple *tuple(get_tuple_field(state, pc + instr_size + count_size + reg_val_size));
   const field_num field(val_field_num(pc + instr_size + count_size + reg_val_size));
   struct1 *s(state.get_struct(src));
   tuple->set_field(field, s->get_data(idx));
   do_increment_runtime(tuple->get_field(field));
}

static inline void
execute_struct_valff(pcounter& pc, state& state)
{
   tuple *src(get_tuple_field(state, pc + instr_size + count_size));
   const field_num field_src(val_field_num(pc + instr_size + count_size));
   tuple *dst(get_tuple_field(state, pc + instr_size + count_size + field_size));
   const field_num field_dst(val_field_num(pc + instr_size + count_size + field_size));

   dst->set_field(field_dst, src->get_field(field_src));
}

static inline void
execute_struct_valffr(pcounter& pc, state& state)
{
   tuple *src(get_tuple_field(state, pc + instr_size + count_size));
   const field_num field_src(val_field_num(pc + instr_size + count_size));
   tuple *dst(get_tuple_field(state, pc + instr_size + count_size + field_size));
   const field_num field_dst(val_field_num(pc + instr_size + count_size + field_size));

   dst->set_field(field_dst, src->get_field(field_src));

   do_increment_runtime(dst->get_field(field_dst));
}

static inline void
execute_mvintfield(pcounter pc, state& state)
{
   tuple *tuple(get_tuple_field(state, pc + instr_size + int_size));
   
   tuple->set_int(val_field_num(pc + instr_size + int_size), pcounter_int(pc + instr_size));
}

static inline void
execute_mvintreg(pcounter pc, state& state)
{
   state.set_int(pcounter_reg(pc + instr_size + int_size), pcounter_int(pc + instr_size));
}

static inline void
execute_mvfieldfield(pcounter pc, state& state)
{
   tuple *tuple_from(get_tuple_field(state, pc + instr_size));
   tuple *tuple_to(get_tuple_field(state, pc + instr_size + field_size));
   const field_num from(val_field_num(pc + instr_size));
   const field_num to(val_field_num(pc + instr_size + field_size));
   tuple_to->set_field(to, tuple_from->get_field(from));
}

static inline void
execute_mvfieldfieldr(pcounter pc, state& state)
{
   tuple *tuple_from(get_tuple_field(state, pc + instr_size));
   tuple *tuple_to(get_tuple_field(state, pc + instr_size + field_size));
   predicate *pred_to(state.preds[val_field_reg(pc + instr_size + field_size)]);
   const field_num from(val_field_num(pc + instr_size));
   const field_num to(val_field_num(pc + instr_size + field_size));
   const tuple_field old(tuple_to->get_field(to));
   tuple_to->set_field(to, tuple_from->get_field(from));

   do_increment_runtime(tuple_to->get_field(to));
   do_decrement_runtime(old, pred_to->get_field_type(to));
}

static inline void
execute_mvfieldreg(pcounter pc, state& state)
{
   tuple *tuple_from(get_tuple_field(state, pc + instr_size));
   const field_num from(val_field_num(pc + instr_size));

   state.set_reg(pcounter_reg(pc + instr_size + field_size), tuple_from->get_field(from));
}

static inline void
execute_mvptrreg(pcounter pc, state& state)
{
   const ptr_val p(pcounter_ptr(pc + instr_size));
   const reg_num reg(pcounter_reg(pc + instr_size + ptr_size));

   state.set_ptr(reg, p);
}

static inline void
execute_mvnilfield(pcounter& pc, state& state)
{
   tuple *tuple(get_tuple_field(state, pc + instr_size));

   tuple->set_nil(val_field_num(pc + instr_size));
}

static inline void
execute_mvnilreg(pcounter& pc, state& state)
{
   state.set_nil(pcounter_reg(pc + instr_size));
}

static inline void
execute_mvregfield(pcounter& pc, state& state)
{
   const reg_num reg(pcounter_reg(pc + instr_size));
   tuple *tuple(get_tuple_field(state, pc + instr_size + reg_val_size));
   const field_num field(val_field_num(pc + instr_size + reg_val_size));
   tuple->set_field(field, state.get_reg(reg));
}

static inline void
execute_mvregfieldr(pcounter& pc, state& state)
{
   const reg_num reg(pcounter_reg(pc + instr_size));
   predicate *pred(state.preds[val_field_reg(pc + instr_size + reg_val_size)]);
   tuple *tuple(get_tuple_field(state, pc + instr_size + reg_val_size));
   const field_num field(val_field_num(pc + instr_size + reg_val_size));

   const tuple_field old(tuple->get_field(field));

   tuple->set_field(field, state.get_reg(reg));

   assert(reference_type(pred->get_field_type(field)->get_type()));

   do_increment_runtime(tuple->get_field(field));
   do_decrement_runtime(old, pred->get_field_type(field));
}

static inline void
execute_mvhostfield(pcounter& pc, state& state)
{
   tuple *tuple(get_tuple_field(state, pc + instr_size));
   const field_num field(val_field_num(pc + instr_size));

#ifdef USE_REAL_NODES
   tuple->set_node(field, (node_val)state.node);
#else
   tuple->set_node(field, state.node->get_id());
#endif
}

static inline void
execute_mvregconst(pcounter& pc, state& state)
{
   const const_id id(pcounter_const_id(pc + instr_size + reg_val_size));
   All->set_const(id, state.get_reg(pcounter_reg(pc + instr_size)));
   if(reference_type(theProgram->get_const_type(id)->get_type()))
      do_increment_runtime(All->get_const(id));
}

static inline void
execute_mvconstfield(pcounter& pc, state& state)
{
   const const_id id(pcounter_const_id(pc + instr_size));
   const field_num field(val_field_num(pc + instr_size + const_id_size));
   tuple *tuple(get_tuple_field(state, pc + instr_size + const_id_size));

   tuple->set_field(field, All->get_const(id));
}

static inline void
execute_mvconstfieldr(pcounter& pc, state& state)
{
   const const_id id(pcounter_const_id(pc + instr_size));
   const field_num field(val_field_num(pc + instr_size + const_id_size));
   tuple *tuple(get_tuple_field(state, pc + instr_size + const_id_size));

   tuple->set_field(field, All->get_const(id));
   do_increment_runtime(tuple->get_field(field));
}

static inline void
execute_mvconstreg(pcounter& pc, state& state)
{
   const const_id id(pcounter_const_id(pc + instr_size));
   const reg_num reg(pcounter_reg(pc + instr_size + const_id_size));

   state.set_reg(reg, All->get_const(id));
}

static inline void
execute_mvintstack(pcounter& pc, state& state)
{
   const int_val i(pcounter_int(pc + instr_size));
   const offset_num off(pcounter_stack(pc + instr_size + int_size));
   state.stack.get_stack_at(off)->int_field = i;
}

static inline void
execute_mvargreg(pcounter& pc, state& state)
{
   const argument_id arg(pcounter_argument_id(pc + instr_size));
   const reg_num reg(pcounter_reg(pc + instr_size + argument_size));

   state.set_string(reg, All->get_argument(arg));
}

static inline void
execute_mvfloatstack(pcounter& pc, state& state)
{
   const float_val f(pcounter_float(pc + instr_size));
   const offset_num off(pcounter_stack(pc + instr_size + float_size));
   state.stack.get_stack_at(off)->float_field = f;
}

static inline void
execute_mvaddrfield(pcounter& pc, state& state)
{
   const node_val n(pcounter_node(pc + instr_size));
   const field_num field(val_field_num(pc + instr_size + node_size));
   tuple *tuple(get_tuple_field(state, pc + instr_size + node_size));

   tuple->set_node(field, n);
}

static inline void
execute_mvfloatfield(pcounter& pc, state& state)
{
   const float_val f(pcounter_float(pc + instr_size));
   const field_num field(val_field_num(pc + instr_size + float_size));
   tuple *tuple(get_tuple_field(state, pc + instr_size + float_size));

   tuple->set_float(field, f);
}

static inline void
execute_mvfloatreg(pcounter& pc, state& state)
{
   const float_val f(pcounter_float(pc + instr_size));
   const reg_num reg(pcounter_reg(pc + instr_size + float_size));

   state.set_float(reg, f);
}

static inline void
execute_mvintconst(pcounter& pc)
{
   const int_val i(pcounter_int(pc + instr_size));
   const const_id id(pcounter_const_id(pc + instr_size + int_size));

   All->set_const_int(id, i);
}

static inline void
execute_mvworldfield(pcounter& pc, state& state)
{
   const field_num field(val_field_num(pc + instr_size));
   tuple *tuple(get_tuple_field(state, pc + instr_size));

   tuple->set_int(field, All->DATABASE->nodes_total);
}

static inline void
execute_mvworldreg(pcounter& pc, state& state)
{
   const reg_num reg(pcounter_reg(pc + instr_size));

   state.set_int(reg, All->DATABASE->nodes_total);
}

static inline pcounter
execute_mvstackpcounter(pcounter& pc, state& state)
{
   const offset_num off(pcounter_stack(pc + instr_size));

   return (pcounter)FIELD_PCOUNTER(*(state.stack.get_stack_at(off))) + CALLF_BASE;
}

static inline void
execute_mvpcounterstack(pcounter& pc, state& state)
{
   const offset_num off(pcounter_stack(pc + instr_size));

   SET_FIELD_PTR(*(state.stack.get_stack_at(off)), pc + MVPCOUNTERSTACK_BASE);
}

static inline void
execute_mvstackreg(pcounter& pc, state& state)
{
   const offset_num off(pcounter_stack(pc + instr_size));
   const reg_num reg(pcounter_reg(pc + instr_size + stack_val_size));

   state.set_reg(reg, *state.stack.get_stack_at(off));
}

static inline void
execute_mvregstack(pcounter& pc, state& state)
{
   const reg_num reg(pcounter_reg(pc + instr_size));
   const offset_num off(pcounter_stack(pc + instr_size + reg_val_size));

   *(state.stack.get_stack_at(off)) = state.get_reg(reg);
}

static inline void
execute_mvaddrreg(pcounter& pc, state& state)
{
   const node_val n(pcounter_node(pc + instr_size));
   const reg_num reg(pcounter_reg(pc + instr_size + node_size));

   state.set_node(reg, n);
}

static inline void
execute_mvhostreg(pcounter& pc, state& state)
{
   const reg_num reg(pcounter_reg(pc + instr_size));
#ifdef USE_REAL_NODES
   state.set_node(reg, (node_val)state.node);
#else
   state.set_node(reg, state.node->get_id());
#endif
}

static inline void
execute_mvregreg(pcounter& pc, state& state)
{
   state.copy_reg(pcounter_reg(pc + instr_size), pcounter_reg(pc + instr_size + reg_val_size));
}

#define DO_OPERATION(SET_FUNCTION, GET_FUNCTION, OP)                    \
   const reg_num op1(pcounter_reg(pc + instr_size));                    \
   const reg_num op2(pcounter_reg(pc + instr_size + reg_val_size));     \
   const reg_num dst(pcounter_reg(pc + instr_size + 2 * reg_val_size)); \
   state.SET_FUNCTION(dst, state.GET_FUNCTION(op1) OP state.GET_FUNCTION(op2))
#define BOOL_OPERATION(GET_FUNCTION, OP)                                \
   DO_OPERATION(set_bool, GET_FUNCTION, OP)

static inline void
execute_addrnotequal(pcounter& pc, state& state)
{
   BOOL_OPERATION(get_node, !=);
}

static inline void
execute_addrequal(pcounter& pc, state& state)
{
   BOOL_OPERATION(get_node, ==);
}

static inline void
execute_intminus(pcounter& pc, state& state)
{
   DO_OPERATION(set_int, get_int, -);
}

static inline void
execute_intequal(pcounter& pc, state& state)
{
   DO_OPERATION(set_int, get_int, ==);
}

static inline void
execute_intnotequal(pcounter& pc, state& state)
{
   DO_OPERATION(set_int, get_int, !=);
}

static inline void
execute_intplus(pcounter& pc, state& state)
{
   DO_OPERATION(set_int, get_int, +);
}

static inline void
execute_intlesser(pcounter& pc, state& state)
{
   DO_OPERATION(set_bool, get_int, <);
}

static inline void
execute_intgreaterequal(pcounter& pc, state& state)
{
   DO_OPERATION(set_bool, get_int, >=);
}

static inline void
execute_boolor(pcounter& pc, state& state)
{
   DO_OPERATION(set_bool, get_bool, ||);
}

static inline void
execute_intlesserequal(pcounter& pc, state& state)
{
   DO_OPERATION(set_bool, get_int, <=);
}

static inline void
execute_intgreater(pcounter& pc, state& state)
{
   DO_OPERATION(set_bool, get_int, >);
}

static inline void
execute_intmul(pcounter& pc, state& state)
{
   DO_OPERATION(set_int, get_int, *);
}

static inline void
execute_intdiv(pcounter& pc, state& state)
{
   DO_OPERATION(set_int, get_int, /);
}

static inline void
execute_intmod(pcounter& pc, state& state)
{
   DO_OPERATION(set_int, get_int, %);
}

static inline void
execute_floatplus(pcounter& pc, state& state)
{
   DO_OPERATION(set_float, get_float, +);
}

static inline void
execute_floatminus(pcounter& pc, state& state)
{
   DO_OPERATION(set_float, get_float, -);
}

static inline void
execute_floatmul(pcounter& pc, state& state)
{
   DO_OPERATION(set_float, get_float, *);
}

static inline void
execute_floatdiv(pcounter& pc, state& state)
{
   DO_OPERATION(set_float, get_float, /);
}

static inline void
execute_floatequal(pcounter& pc, state& state)
{
   DO_OPERATION(set_bool, get_float, ==);
}

static inline void
execute_floatnotequal(pcounter& pc, state& state)
{
   DO_OPERATION(set_bool, get_float, !=);
}

static inline void
execute_floatlesser(pcounter& pc, state& state)
{
   DO_OPERATION(set_bool, get_float, <);
}

static inline void
execute_floatlesserequal(pcounter& pc, state& state)
{
   DO_OPERATION(set_bool, get_float, <=);
}

static inline void
execute_floatgreater(pcounter& pc, state& state)
{
   DO_OPERATION(set_bool, get_float, >);
}

static inline void
execute_floatgreaterequal(pcounter& pc, state& state)
{
   DO_OPERATION(set_bool, get_float, >=);
}

static inline void
execute_boolequal(pcounter& pc, state& state)
{
   DO_OPERATION(set_bool, get_bool, ==);
}

static inline void
execute_boolnotequal(pcounter& pc, state& state)
{
   DO_OPERATION(set_bool, get_bool, !=);
}

static inline void
execute_headrr(pcounter& pc, state& state)
{
   const reg_num ls(pcounter_reg(pc + instr_size));
   const reg_num dst(pcounter_reg(pc + instr_size + reg_val_size));

   runtime::cons *l(state.get_cons(ls));

   state.set_reg(dst, l->get_head());
}

static inline void
execute_headfr(pcounter& pc, state& state)
{
   tuple *tuple(get_tuple_field(state, pc + instr_size));
   const field_num field(val_field_num(pc + instr_size));
   runtime::cons *l(tuple->get_cons(field));
   const reg_num dst(pcounter_reg(pc + instr_size + field_size));

   state.set_reg(dst, l->get_head());
}

static inline void
execute_headff(pcounter& pc, state& state)
{
   tuple *tsrc(get_tuple_field(state, pc + instr_size));
   const field_num src(val_field_num(pc + instr_size));
   tuple *tdst(get_tuple_field(state, pc + instr_size + field_size));
   const field_num dst(val_field_num(pc + instr_size + field_size));

   runtime::cons *l(tsrc->get_cons(src));

   tdst->set_field(dst, l->get_head());
}

static inline void
execute_headffr(pcounter& pc, state& state)
{
   tuple *tsrc(get_tuple_field(state, pc + instr_size));
   const field_num src(val_field_num(pc + instr_size));
   tuple *tdst(get_tuple_field(state, pc + instr_size + field_size));
   const field_num dst(val_field_num(pc + instr_size + field_size));

   runtime::cons *l(tsrc->get_cons(src));

   tdst->set_field(dst, l->get_head());
   do_increment_runtime(tdst->get_field(dst));
}

static inline void
execute_headrf(pcounter& pc, state& state)
{
   const reg_num reg(pcounter_reg(pc + instr_size));
   tuple *tdst(get_tuple_field(state, pc + instr_size + reg_val_size));
   const field_num dst(val_field_num(pc + instr_size + reg_val_size));

   runtime::cons *l(state.get_cons(reg));

   tdst->set_field(dst, l->get_head());
}

static inline void
execute_headrfr(pcounter& pc, state& state)
{
   const reg_num reg(pcounter_reg(pc + instr_size));
   tuple *tdst(get_tuple_field(state, pc + instr_size + reg_val_size));
   const field_num dst(val_field_num(pc + instr_size + reg_val_size));

   runtime::cons *l(state.get_cons(reg));

   tdst->set_field(dst, l->get_head());

   do_increment_runtime(tdst->get_field(dst));
}

static inline void
execute_tailrr(pcounter& pc, state& state)
{
   const reg_num src(pcounter_reg(pc + instr_size));
   const reg_num dst(pcounter_reg(pc + instr_size + reg_val_size));

   runtime::cons *l(state.get_cons(src));

   state.set_cons(dst, l->get_tail());
}

static inline void
execute_tailfr(pcounter& pc, state& state)
{
   tuple *tuple(get_tuple_field(state, pc + instr_size));
   const field_num field(val_field_num(pc + instr_size));
   const reg_num dst(pcounter_reg(pc + instr_size + field_size));

   state.set_cons(dst, tuple->get_cons(field)->get_tail());
}

static inline void
execute_tailff(pcounter& pc, state& state)
{
   tuple *tsrc(get_tuple_field(state, pc + instr_size));
   const field_num src(val_field_num(pc + instr_size));
   tuple *tdst(get_tuple_field(state, pc + instr_size + field_size));
   const field_num dst(val_field_num(pc + instr_size + field_size));

   tdst->set_cons(dst, tsrc->get_cons(src)->get_tail());
}

static inline void
execute_tailrf(pcounter& pc, state& state)
{
   const reg_num src(pcounter_reg(pc + instr_size));
   tuple *tuple(get_tuple_field(state, pc + instr_size + reg_val_size));
   const field_num field(val_field_num(pc + instr_size + reg_val_size));

   tuple->set_cons(field, state.get_cons(src)->get_tail());
}

static inline void
execute_consrrr(pcounter& pc, state& state)
{
   const reg_num head(pcounter_reg(pc + instr_size + type_size));
   const reg_num tail(pcounter_reg(pc + instr_size + type_size + reg_val_size));
   const reg_num dest(pcounter_reg(pc + instr_size + type_size + 2 * reg_val_size));

   list_type *ltype((list_type*)theProgram->get_type(cons_type(pc)));
   cons *new_list(cons::create(state.get_cons(tail), state.get_reg(head), ltype));
	state.add_cons(new_list);
   state.set_cons(dest, new_list);
}

static inline void
execute_consrff(pcounter& pc, state& state)
{
   const reg_num head(pcounter_reg(pc + instr_size));
   predicate *pred(state.preds[val_field_reg(pc + instr_size + reg_val_size)]);
   tuple *tail(get_tuple_field(state, pc + instr_size + reg_val_size));
   const field_num tail_field(val_field_num(pc + instr_size + reg_val_size));
   tuple *dest(get_tuple_field(state, pc + instr_size + reg_val_size + field_size));
   const field_num dest_field(val_field_num(pc + instr_size + reg_val_size + field_size));

   cons *new_list(cons::create(tail->get_cons(tail_field), state.get_reg(head), (list_type*)pred->get_field_type(tail_field)));
   dest->set_cons(dest_field, new_list);
}

static inline void
execute_consfrf(pcounter& pc, state& state)
{
   tuple *head(get_tuple_field(state, pc + instr_size));
   const field_num head_field(val_field_num(pc + instr_size));
   const reg_num tail(pcounter_reg(pc + instr_size + field_size));
   predicate *pred(state.preds[val_field_reg(pc + instr_size + field_size + reg_val_size)]);
   tuple *dest(get_tuple_field(state, pc + instr_size + field_size + reg_val_size));
   const field_num dest_field(val_field_num(pc + instr_size + field_size + reg_val_size));

   cons *new_list(cons::create(state.get_cons(tail), head->get_field(head_field), (list_type*)pred->get_field_type(dest_field)));
   dest->set_cons(dest_field, new_list);
}

static inline void
execute_consffr(pcounter& pc, state& state)
{
   tuple *head(get_tuple_field(state, pc + instr_size));
   const field_num head_field(val_field_num(pc + instr_size));
   predicate *pred(state.preds[val_field_reg(pc + instr_size + field_size)]);
   tuple *tail(get_tuple_field(state, pc + instr_size + field_size));
   const field_num tail_field(val_field_num(pc + instr_size + field_size));
   const reg_num dest(pcounter_reg(pc + instr_size + 2 * field_size));

   cons *new_list(cons::create(tail->get_cons(tail_field), head->get_field(head_field), (list_type*)pred->get_field_type(tail_field)));
	state.add_cons(new_list);
   state.set_cons(dest, new_list);
}

static inline void
execute_consrrf(pcounter& pc, state& state)
{
   const reg_num head(pcounter_reg(pc + instr_size));
   const reg_num tail(pcounter_reg(pc + instr_size + reg_val_size));
   tuple *dest(get_tuple_field(state, pc + instr_size + 2 * reg_val_size));
   predicate *pred(state.preds[val_field_reg(pc + instr_size + 2 * reg_val_size)]);
   const field_num field(val_field_num(pc + instr_size + 2 * reg_val_size));

   cons *new_list(cons::create(state.get_cons(tail), state.get_reg(head), (list_type*)pred->get_field_type(field)));
   dest->set_cons(field, new_list);
}

static inline void
execute_consrfr(pcounter& pc, state& state)
{
   const reg_num head(pcounter_reg(pc + instr_size));
   tuple *tail(get_tuple_field(state, pc + instr_size + reg_val_size));
   predicate *pred(state.preds[val_field_reg(pc + instr_size + reg_val_size)]);
   const field_num field(val_field_num(pc + instr_size + reg_val_size));
   const reg_num dest(pcounter_reg(pc + instr_size + reg_val_size + field_size));

   cons *new_list(cons::create(tail->get_cons(field), state.get_reg(head), (list_type*)pred->get_field_type(field)));
   state.add_cons(new_list);
   state.set_cons(dest, new_list);
}

static inline void
execute_consfrr(pcounter& pc, state& state)
{
   tuple *head(get_tuple_field(state, pc + instr_size + type_size));
   const field_num field(val_field_num(pc + instr_size + type_size));
   const reg_num tail(pcounter_reg(pc + instr_size + type_size + field_size));
   const reg_num dest(pcounter_reg(pc + instr_size + type_size + field_size + reg_val_size));

   list_type *ltype((list_type*)theProgram->get_type(cons_type(pc)));
   cons *new_list(cons::create(state.get_cons(tail), head->get_field(field), ltype));
   state.add_cons(new_list);
   state.set_cons(dest, new_list);
}

static inline void
execute_consfff(pcounter& pc, state& state)
{
   tuple *head(get_tuple_field(state, pc + instr_size));
   const field_num field_head(val_field_num(pc + instr_size));
   tuple *tail(get_tuple_field(state, pc + instr_size + field_size));
   predicate *pred(state.preds[val_field_reg(pc + instr_size + field_size)]);
   const field_num field_tail(val_field_num(pc + instr_size + field_size));
   tuple *dest(get_tuple_field(state, pc + instr_size + 2 * field_size));
   const field_num field_dest(val_field_num(pc + instr_size + 2 * field_size));

   cons *new_list(cons::create(tail->get_cons(field_tail), head->get_field(field_head), (list_type*)pred->get_field_type(field_tail)));
   dest->set_cons(field_dest, new_list);
}

#ifdef COMPUTED_GOTOS
#define CASE(X)
#define JUMP_NEXT() goto *jump_table[fetch(pc)]
#define JUMP(label, jump_offset) label: { const pcounter npc = pc + jump_offset; register void *to_go = (void*)jump_table[fetch(npc)];
#define COMPLEX_JUMP(label) label: {
#define ADVANCE() pc = npc; goto *to_go;
#define ENDOP() }
#else
#define JUMP_NEXT() goto eval_loop
#define CASE(INSTR) case INSTR:
#define JUMP(label, jump_offset) { const pcounter npc = pc + jump_offset; assert(npc == advance(pc));
#define COMPLEX_JUMP(label) {
#define ADVANCE() pc = npc; JUMP_NEXT();
#define ENDOP() }
#endif

static inline return_type
execute(pcounter pc, state& state, const reg_num reg, tuple *tpl, predicate *pred)
{
	if(tpl != NULL) {
      state.set_tuple(reg, tpl);
      state.preds[reg] = pred;
#ifdef CORE_STATISTICS
		state.stat.stat_tuples_used++;
      if(tpl->is_linear()) {
         state.stat.stat_predicate_applications[pred->get_id()]++;
      }
#endif
   }

#ifdef COMPUTED_GOTOS
#include "vm/jump_table.hpp"
#endif

#ifdef COMPUTED_GOTOS
   JUMP_NEXT();
#else
   while(true)
   {
eval_loop:

#ifdef DEBUG_INSTRS
      instr_print_simple(pc, 0, theProgram, cout);
#endif

#ifdef USE_SIM
      if(state::SIM)
         ++state.sim_instr_counter;
#endif

#ifdef CORE_STATISTICS
		state.stat.stat_instructions_executed++;
#endif
		
      switch(fetch(pc)) {
#endif // !COMPUTED_GOTOS
         CASE(RETURN_INSTR)
            COMPLEX_JUMP(return_instr)
            return RETURN_OK;
         ENDOP()
         
         CASE(NEXT_INSTR)
            COMPLEX_JUMP(next_instr)
            return RETURN_NEXT;
         ENDOP()

         CASE(RETURN_LINEAR_INSTR)
            COMPLEX_JUMP(return_linear)
            return RETURN_LINEAR;
         ENDOP()
         
         CASE(RETURN_DERIVED_INSTR)
            COMPLEX_JUMP(return_derived)
            return RETURN_DERIVED;
         ENDOP()
         
         CASE(RETURN_SELECT_INSTR)
            COMPLEX_JUMP(return_select)
            pc += return_select_jump(pc);
            JUMP_NEXT();
         ENDOP()
         
         CASE(IF_INSTR)
            JUMP(if_instr, IF_BASE)
#ifdef CORE_STATISTICS
				state.stat.stat_if_tests++;
#endif
            if(!state.get_bool(if_reg(pc))) {
#ifdef CORE_STATISTICS
					state.stat.stat_if_failed++;
#endif
               pc += if_jump(pc);
               JUMP_NEXT();
            }
            ADVANCE();
         ENDOP()

         CASE(IF_ELSE_INSTR)
            JUMP(if_else, IF_ELSE_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_if_tests++;
#endif
            if(!state.get_bool(if_reg(pc))) {
#ifdef CORE_STATISTICS
               state.stat.stat_if_failed++;
#endif
               pc += if_else_jump_else(pc);
               JUMP_NEXT();
            }
            ADVANCE();
         ENDOP()

         CASE(JUMP_INSTR)
            COMPLEX_JUMP(jump)
            pc += jump_get(pc, instr_size) + JUMP_BASE;

            JUMP_NEXT();
        ENDOP()
         
			CASE(END_LINEAR_INSTR)
            COMPLEX_JUMP(end_linear)
				return RETURN_END_LINEAR;
         ENDOP()
			
         CASE(RESET_LINEAR_INSTR)
            JUMP(reset_linear, RESET_LINEAR_BASE)
            {
               const bool old_is_linear(state.is_linear);
               
               state.is_linear = false;
               
               return_type ret(execute(pc + RESET_LINEAR_BASE, state, 0, NULL, NULL));

					assert(ret == RETURN_END_LINEAR);
               (void)ret;

               state.is_linear = old_is_linear;
               
               pc += reset_linear_jump(pc);

               JUMP_NEXT();
            }
            ADVANCE();
         ENDOP()
            
#define DECIDE_NEXT_ITER_INSTR() \
            if(ret == RETURN_LINEAR) return RETURN_LINEAR;                       \
            if(ret == RETURN_DERIVED && state.is_linear) return RETURN_DERIVED;  \
            pc += iter_outer_jump(pc);                                           \
            JUMP_NEXT()

         CASE(PERS_ITER_INSTR)
            COMPLEX_JUMP(pers_iter)
            {
               predicate *pred(theProgram->get_predicate(iter_predicate(pc)));
               const reg_num reg(iter_reg(pc));
               match *mobj(retrieve_match_object(state, pc, pred, PERS_ITER_BASE));

               const return_type ret(execute_pers_iter(reg, mobj, pc + iter_inner_jump(pc), state, pred));

               DECIDE_NEXT_ITER_INSTR();
            }
         ENDOP()

         CASE(LINEAR_ITER_INSTR)
            COMPLEX_JUMP(linear_iter)
            {
               predicate *pred(theProgram->get_predicate(iter_predicate(pc)));
               const reg_num reg(iter_reg(pc));
               match *mobj(retrieve_match_object(state, pc, pred, LINEAR_ITER_BASE));

               const return_type ret(execute_linear_iter(reg, mobj, pc + iter_inner_jump(pc), state, pred));

               DECIDE_NEXT_ITER_INSTR();
            }
         ENDOP()

         CASE(RLINEAR_ITER_INSTR)
            COMPLEX_JUMP(rlinear_iter)
            {
               predicate *pred(theProgram->get_predicate(iter_predicate(pc)));
               const reg_num reg(iter_reg(pc));
               match *mobj(retrieve_match_object(state, pc, pred, RLINEAR_ITER_BASE));

               const return_type ret(execute_rlinear_iter(reg, mobj, pc + iter_inner_jump(pc), state, pred));

               DECIDE_NEXT_ITER_INSTR();
            }
         ENDOP()

         CASE(OPERS_ITER_INSTR)
            COMPLEX_JUMP(opers_iter)
            {
               predicate *pred(theProgram->get_predicate(iter_predicate(pc)));
               const reg_num reg(iter_reg(pc));
               match *mobj(retrieve_match_object(state, pc, pred, OPERS_ITER_BASE));

               const return_type ret(execute_opers_iter(reg, mobj, pc, pc + iter_inner_jump(pc), state, pred));

               DECIDE_NEXT_ITER_INSTR();
            }
         ENDOP()

         CASE(OLINEAR_ITER_INSTR)
            COMPLEX_JUMP(olinear_iter)
            {
               predicate *pred(theProgram->get_predicate(iter_predicate(pc)));
               const reg_num reg(iter_reg(pc));
               match *mobj(retrieve_match_object(state, pc, pred, OLINEAR_ITER_BASE));

               const return_type ret(execute_olinear_iter(reg, mobj, pc, pc + iter_inner_jump(pc), state, pred));

               DECIDE_NEXT_ITER_INSTR();
            }
         ENDOP()

         CASE(ORLINEAR_ITER_INSTR)
            COMPLEX_JUMP(orlinear_iter)
            {
               predicate *pred(theProgram->get_predicate(iter_predicate(pc)));
               const reg_num reg(iter_reg(pc));
               match *mobj(retrieve_match_object(state, pc, pred, ORLINEAR_ITER_BASE));

               const return_type ret(execute_orlinear_iter(reg, mobj, pc, pc + iter_inner_jump(pc), state, pred));

               DECIDE_NEXT_ITER_INSTR();
            }
         ENDOP()
            
         CASE(REMOVE_INSTR)
            JUMP(remove, REMOVE_BASE)
            execute_remove(pc, state);
            ADVANCE()
         ENDOP()

         CASE(UPDATE_INSTR)
            JUMP(update, UPDATE_BASE)
            execute_update(pc, state);
            ADVANCE()
         ENDOP()
            
         CASE(ALLOC_INSTR)
            JUMP(alloc, ALLOC_BASE)
            execute_alloc(pc, state);
            ADVANCE()
         ENDOP()
            
         CASE(SEND_INSTR)
            JUMP(send, SEND_BASE)
            execute_send(pc, state);
            ADVANCE()
         ENDOP()

         CASE(ADDLINEAR_INSTR)
            JUMP(addlinear, ADDLINEAR_BASE)
            execute_add_linear(pc, state);
            ADVANCE()
         ENDOP()

         CASE(ADDPERS_INSTR)
            JUMP(addpers, ADDPERS_BASE)
            execute_add_persistent(pc, state);
            ADVANCE()
         ENDOP()

         CASE(RUNACTION_INSTR)
            JUMP(runaction, RUNACTION_BASE)
            execute_run_action(pc, state);
            ADVANCE()
         ENDOP()

         CASE(ENQUEUE_LINEAR_INSTR)
            JUMP(enqueue_linear, ENQUEUE_LINEAR_BASE)
            execute_enqueue_linear(pc, state);
            ADVANCE()
         ENDOP()

         CASE(SEND_DELAY_INSTR)
            JUMP(send_delay, SEND_DELAY_BASE)
            execute_send_delay(pc, state);
            ADVANCE()
         ENDOP()
            
         CASE(NOT_INSTR)
            JUMP(not_instr, NOT_BASE)
            execute_not(pc, state);
            ADVANCE()
         ENDOP()
            
         CASE(TESTNIL_INSTR)
            JUMP(testnil, TESTNIL_BASE)
            execute_testnil(pc, state);
            ADVANCE()
         ENDOP()
            
         CASE(FLOAT_INSTR)
            JUMP(float_instr, FLOAT_BASE)
            execute_float(pc, state);
            ADVANCE()
         ENDOP()
            
         CASE(SELECT_INSTR)
            COMPLEX_JUMP(select)
            pc = execute_select(pc, state);
            JUMP_NEXT();
         ENDOP()
            
         CASE(DELETE_INSTR)
            JUMP(delete_instr, DELETE_BASE + instr_delete_args_size(pc + DELETE_BASE, delete_num_args(pc)))
            //execute_delete(pc, state);
            ADVANCE()
         ENDOP()
            
         CASE(CALL_INSTR)
            JUMP(call, CALL_BASE + call_num_args(pc) * reg_val_size)
            execute_call(pc, state);
            ADVANCE()
         ENDOP()
         CASE(CALL0_INSTR)
            JUMP(call0, CALL0_BASE)
            execute_call0(pc, state);
            ADVANCE()
         ENDOP()
         CASE(CALL1_INSTR)
            JUMP(call1, CALL1_BASE)
            execute_call1(pc, state);
            ADVANCE()
         ENDOP()
         CASE(CALL2_INSTR)
            JUMP(call2, CALL2_BASE)
            execute_call2(pc, state);
            ADVANCE()
         ENDOP()
         CASE(CALL3_INSTR)
            JUMP(call3, CALL3_BASE)
            execute_call3(pc, state);
            ADVANCE()
         ENDOP()

         CASE(RULE_INSTR)
            JUMP(rule, RULE_BASE)
            execute_rule(pc, state);
            ADVANCE()
         ENDOP()

         CASE(RULE_DONE_INSTR)
            JUMP(rule_done, RULE_DONE_BASE)
            execute_rule_done(pc, state);
            ADVANCE()
         ENDOP()

         CASE(NEW_NODE_INSTR)
            JUMP(new_node, NEW_NODE_BASE)
            execute_new_node(pc, state);
            ADVANCE()
         ENDOP()

         CASE(NEW_AXIOMS_INSTR)
            JUMP(new_axioms, new_axioms_jump(pc))
            execute_new_axioms(pc, state);
            ADVANCE()
         ENDOP()

         CASE(PUSH_INSTR)
            JUMP(push, PUSH_BASE)
            state.stack.push();
            ADVANCE()
         ENDOP()

         CASE(PUSHN_INSTR)
            JUMP(pushn, PUSHN_BASE)
            state.stack.push(push_n(pc));
            ADVANCE()
         ENDOP()

         CASE(POP_INSTR)
            JUMP(pop, POP_BASE)
            state.stack.pop();
            ADVANCE()
         ENDOP()

         CASE(PUSH_REGS_INSTR)
            JUMP(push_regs, PUSH_REGS_BASE)
            state.stack.push_regs(state.regs);
            ADVANCE()
         ENDOP()

         CASE(POP_REGS_INSTR)
            JUMP(pop_regs, POP_REGS_BASE)
            state.stack.pop_regs(state.regs);
            ADVANCE()
         ENDOP()

         CASE(CALLF_INSTR)
            COMPLEX_JUMP(callf)
            {
              const vm::callf_id id(callf_get_id(pc));
              function *fun(theProgram->get_function(id));

              pc = fun->get_bytecode();
              JUMP_NEXT();
            }
         ENDOP()

         CASE(MAKE_STRUCTR_INSTR)
            JUMP(make_structr, MAKE_STRUCTR_BASE)
            execute_make_structr(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MAKE_STRUCTF_INSTR)
            JUMP(make_structf, MAKE_STRUCTF_BASE)
            execute_make_structf(pc, state);
            ADVANCE()
         ENDOP()

         CASE(STRUCT_VALRR_INSTR)
            JUMP(struct_valrr, STRUCT_VALRR_BASE)
            execute_struct_valrr(pc, state);
            ADVANCE()
         ENDOP()
         CASE(STRUCT_VALFR_INSTR)
            JUMP(struct_valfr, STRUCT_VALFR_BASE)
            execute_struct_valfr(pc, state);
            ADVANCE()
         ENDOP()
         CASE(STRUCT_VALRF_INSTR)
            JUMP(struct_valrf, STRUCT_VALRF_BASE)
            execute_struct_valrf(pc, state);
            ADVANCE()
         ENDOP()
         CASE(STRUCT_VALRFR_INSTR)
            JUMP(struct_valrfr, STRUCT_VALRFR_BASE)
            execute_struct_valrfr(pc, state);
            ADVANCE()
         ENDOP()
         CASE(STRUCT_VALFF_INSTR)
            JUMP(struct_valff, STRUCT_VALFF_BASE)
            execute_struct_valff(pc, state);
            ADVANCE()
         ENDOP()
         CASE(STRUCT_VALFFR_INSTR)
            JUMP(struct_valffr, STRUCT_VALFFR_BASE)
            execute_struct_valffr(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVINTFIELD_INSTR)
            JUMP(mvintfield, MVINTFIELD_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvintfield(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVINTREG_INSTR)
            JUMP(mvintreg, MVINTREG_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvintreg(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVFIELDFIELD_INSTR)
            JUMP(mvfieldfield, MVFIELDFIELD_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvfieldfield(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVFIELDFIELDR_INSTR)
            JUMP(mvfieldfieldr, MVFIELDFIELD_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvfieldfieldr(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVFIELDREG_INSTR)
            JUMP(mvfieldreg, MVFIELDREG_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvfieldreg(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVPTRREG_INSTR)
            JUMP(mvptrreg, MVPTRREG_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvptrreg(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVNILFIELD_INSTR)
            JUMP(mvnilfield, MVNILFIELD_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvnilfield(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVNILREG_INSTR)
            JUMP(mvnilreg, MVNILREG_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvnilreg(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVREGFIELD_INSTR)
            JUMP(mvregfield, MVREGFIELD_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvregfield(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVREGFIELDR_INSTR)
            JUMP(mvregfieldr, MVREGFIELD_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvregfieldr(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVHOSTFIELD_INSTR)
            JUMP(mvhostfield, MVHOSTFIELD_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvhostfield(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVREGCONST_INSTR)
            JUMP(mvregconst, MVREGCONST_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvregconst(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVCONSTFIELD_INSTR)
            JUMP(mvconstfield, MVCONSTFIELD_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvconstfield(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVCONSTFIELDR_INSTR)
            JUMP(mvconstfieldr, MVCONSTFIELD_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvconstfieldr(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVADDRFIELD_INSTR)
            JUMP(mvaddrfield, MVADDRFIELD_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvaddrfield(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVFLOATFIELD_INSTR)
            JUMP(mvfloatfield, MVFLOATFIELD_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvfloatfield(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVFLOATREG_INSTR)
            JUMP(mvfloatreg, MVFLOATREG_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvfloatreg(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVINTCONST_INSTR)
            JUMP(mvintconst, MVINTCONST_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvintconst(pc);
            ADVANCE()
         ENDOP()

         CASE(MVWORLDFIELD_INSTR)
            JUMP(mvworldfield, MVWORLDFIELD_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvworldfield(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVSTACKPCOUNTER_INSTR)
            COMPLEX_JUMP(mvstackpcounter)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            pc = execute_mvstackpcounter(pc, state);
            JUMP_NEXT();
         ENDOP()

         CASE(MVPCOUNTERSTACK_INSTR)
            JUMP(mvpcounterstack, MVPCOUNTERSTACK_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvpcounterstack(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVSTACKREG_INSTR)
            JUMP(mvstackreg, MVSTACKREG_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvstackreg(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVREGSTACK_INSTR)
            JUMP(mvregstack, MVREGSTACK_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvregstack(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVADDRREG_INSTR)
            JUMP(mvaddrreg, MVADDRREG_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvaddrreg(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVHOSTREG_INSTR)
            JUMP(mvhostreg, MVHOSTREG_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvhostreg(pc, state);
            ADVANCE()
         ENDOP()

         CASE(ADDRNOTEQUAL_INSTR)
            JUMP(addrnotequal, operation_size)
            execute_addrnotequal(pc, state);
            ADVANCE()
         ENDOP()

         CASE(ADDREQUAL_INSTR)
            JUMP(addrequal, operation_size)
            execute_addrequal(pc, state);
            ADVANCE()
         ENDOP()

         CASE(INTMINUS_INSTR)
            JUMP(intminus, operation_size)
            execute_intminus(pc, state);
            ADVANCE()
         ENDOP()

         CASE(INTEQUAL_INSTR)
            JUMP(intequal, operation_size)
            execute_intequal(pc, state);
            ADVANCE()
         ENDOP()

         CASE(INTNOTEQUAL_INSTR)
            JUMP(intnotequal, operation_size)
            execute_intnotequal(pc, state);
            ADVANCE()
         ENDOP()

         CASE(INTPLUS_INSTR)
            JUMP(intplus, operation_size)
            execute_intplus(pc, state);
            ADVANCE()
         ENDOP()

         CASE(INTLESSER_INSTR)
            JUMP(intlesser, operation_size)
            execute_intlesser(pc, state);
            ADVANCE()
         ENDOP()

         CASE(INTGREATEREQUAL_INSTR)
            JUMP(intgreaterequal, operation_size)
            execute_intgreaterequal(pc, state);
            ADVANCE()
         ENDOP()

         CASE(BOOLOR_INSTR)
            JUMP(boolor, operation_size)
            execute_boolor(pc, state);
            ADVANCE()
         ENDOP()

         CASE(INTLESSEREQUAL_INSTR)
            JUMP(intlesserequal, operation_size)
            execute_intlesserequal(pc, state);
            ADVANCE()
         ENDOP()

         CASE(INTGREATER_INSTR)
            JUMP(intgreater, operation_size)
            execute_intgreater(pc, state);
            ADVANCE()
         ENDOP()

         CASE(INTMUL_INSTR)
            JUMP(intmul, operation_size)
            execute_intmul(pc, state);
            ADVANCE()
         ENDOP()

         CASE(INTDIV_INSTR)
            JUMP(intdiv, operation_size)
            execute_intdiv(pc, state);
            ADVANCE()
         ENDOP()

         CASE(INTMOD_INSTR)
            JUMP(intmod, operation_size)
            execute_intmod(pc, state);
            ADVANCE()
         ENDOP()

         CASE(FLOATPLUS_INSTR)
            JUMP(floatplus, operation_size)
            execute_floatplus(pc, state);
            ADVANCE()
         ENDOP()

         CASE(FLOATMINUS_INSTR)
            JUMP(floatminus, operation_size)
            execute_floatminus(pc, state);
            ADVANCE()
         ENDOP()

         CASE(FLOATMUL_INSTR)
            JUMP(floatmul, operation_size)
            execute_floatmul(pc, state);
            ADVANCE()
         ENDOP()

         CASE(FLOATDIV_INSTR)
            JUMP(floatdiv, operation_size)
            execute_floatdiv(pc, state);
            ADVANCE()
         ENDOP()

         CASE(FLOATEQUAL_INSTR)
            JUMP(floatequal, operation_size)
            execute_floatequal(pc, state);
            ADVANCE()
         ENDOP()

         CASE(FLOATNOTEQUAL_INSTR)
            JUMP(floatnotequal, operation_size)
            execute_floatnotequal(pc, state);
            ADVANCE()
         ENDOP()

         CASE(FLOATLESSER_INSTR)
            JUMP(floatlesser, operation_size)
            execute_floatlesser(pc, state);
            ADVANCE()
         ENDOP()

         CASE(FLOATLESSEREQUAL_INSTR)
            JUMP(floatlesserequal, operation_size)
            execute_floatlesserequal(pc, state);
            ADVANCE()
         ENDOP()

         CASE(FLOATGREATER_INSTR)
            JUMP(floatgreater, operation_size)
            execute_floatgreater(pc, state);
            ADVANCE()
         ENDOP()

         CASE(FLOATGREATEREQUAL_INSTR)
            JUMP(floatgreaterequal, operation_size)
            execute_floatgreaterequal(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVREGREG_INSTR)
            JUMP(mvregreg, MVREGREG_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvregreg(pc, state);
            ADVANCE()
         ENDOP()

         CASE(BOOLEQUAL_INSTR)
            JUMP(boolequal, operation_size)
            execute_boolequal(pc, state);
            ADVANCE()
         ENDOP()

         CASE(BOOLNOTEQUAL_INSTR)
            JUMP(boolnotequal, operation_size)
            execute_boolnotequal(pc, state);
            ADVANCE()
         ENDOP()

         CASE(HEADRR_INSTR)
            JUMP(headrr, HEADRR_BASE)
            execute_headrr(pc, state);
            ADVANCE()
         ENDOP()

         CASE(HEADFR_INSTR)
            JUMP(headfr, HEADFR_BASE)
            execute_headfr(pc, state);
            ADVANCE()
         ENDOP()

         CASE(HEADFF_INSTR)
            JUMP(headff, HEADFF_BASE)
            execute_headff(pc, state);
            ADVANCE()
         ENDOP()

         CASE(HEADRF_INSTR)
            JUMP(headrf, HEADRF_BASE)
            execute_headrf(pc, state);
            ADVANCE()
         ENDOP()

         CASE(HEADFFR_INSTR)
            JUMP(headffr, HEADFF_BASE)
            execute_headffr(pc, state);
            ADVANCE()
         ENDOP()

         CASE(HEADRFR_INSTR)
            JUMP(headrfr, HEADRF_BASE)
            execute_headrfr(pc, state);
            ADVANCE()
         ENDOP()

         CASE(TAILRR_INSTR)
            JUMP(tailrr, TAILRR_BASE)
            execute_tailrr(pc, state);
            ADVANCE()
         ENDOP()

         CASE(TAILFR_INSTR)
            JUMP(tailfr, TAILFR_BASE)
            execute_tailfr(pc, state);
            ADVANCE()
         ENDOP()

         CASE(TAILFF_INSTR)
            JUMP(tailff, TAILFF_BASE)
            execute_tailff(pc, state);
            ADVANCE()
         ENDOP()

         CASE(TAILRF_INSTR)
            JUMP(tailrf, TAILRF_BASE)
            execute_tailrf(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVWORLDREG_INSTR)
            JUMP(mvworldreg, MVWORLDREG_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvworldreg(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVCONSTREG_INSTR)
            JUMP(mvconstreg, MVCONSTREG_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvconstreg(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVINTSTACK_INSTR)
            JUMP(mvintstack, MVINTSTACK_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvintstack(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVFLOATSTACK_INSTR)
            JUMP(mvfloatstack, MVFLOATSTACK_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvfloatstack(pc, state);
            ADVANCE()
         ENDOP()

         CASE(MVARGREG_INSTR)
            JUMP(mvargreg, MVARGREG_BASE)
#ifdef CORE_STATISTICS
            state.stat.stat_moves_executed++;
#endif
            execute_mvargreg(pc, state);
            ADVANCE()
         ENDOP()
            
         CASE(CONSRRR_INSTR)
            JUMP(consrrr, CONSRRR_BASE)
            execute_consrrr(pc, state);
            ADVANCE()
         ENDOP()

         CASE(CONSRFF_INSTR)
            JUMP(consrff, CONSRFF_BASE)
            execute_consrff(pc, state);
            ADVANCE()
         ENDOP()

         CASE(CONSFRF_INSTR)
            JUMP(consfrf, CONSFRF_BASE)
            execute_consfrf(pc, state);
            ADVANCE()
         ENDOP()

         CASE(CONSFFR_INSTR)
            JUMP(consffr, CONSFFR_BASE)
            execute_consffr(pc, state);
            ADVANCE()
         ENDOP()

         CASE(CONSRRF_INSTR)
            JUMP(consrrf, CONSRRF_BASE)
            execute_consrrf(pc, state);
            ADVANCE()
         ENDOP()

         CASE(CONSRFR_INSTR)
            JUMP(consrfr, CONSRFR_BASE)
            execute_consrfr(pc, state);
            ADVANCE()
         ENDOP()

         CASE(CONSFRR_INSTR)
            JUMP(consfrr, CONSFRR_BASE)
            execute_consfrr(pc, state);
            ADVANCE()
         ENDOP()

         CASE(CONSFFF_INSTR)
            JUMP(consfff, CONSFFF_BASE)
            execute_consfff(pc, state);
            ADVANCE()
         ENDOP()

         CASE(CALLE_INSTR)
            JUMP(calle, CALLE_BASE * calle_num_args(pc))
            execute_calle(pc, state);
            ADVANCE()
         ENDOP()

         CASE(SET_PRIORITY_INSTR)
            JUMP(set_priority, SET_PRIORITY_BASE)
            execute_set_priority(pc, state);
            ADVANCE()
         ENDOP()

         CASE(SET_PRIORITYH_INSTR)
            JUMP(set_priorityh, SET_PRIORITYH_BASE)
            execute_set_priority_here(pc, state);
            ADVANCE()
         ENDOP()

         CASE(ADD_PRIORITY_INSTR)
            JUMP(add_priority, ADD_PRIORITY_BASE)
            execute_add_priority(pc, state);
            ADVANCE()
         ENDOP()

         CASE(ADD_PRIORITYH_INSTR)
            JUMP(add_priorityh, ADD_PRIORITYH_BASE)
            execute_add_priority_here(pc, state);
            ADVANCE()
         ENDOP()

         CASE(STOP_PROG_INSTR)
            JUMP(stop_program, STOP_PROG_BASE)
            sched::base::stop_flag = true;
            ADVANCE()
         ENDOP()

         CASE(CPU_ID_INSTR)
            JUMP(cpu_id, CPU_ID_BASE)
            execute_cpu_id(pc, state);
            ADVANCE()
         ENDOP()

         CASE(NODE_PRIORITY_INSTR)
            JUMP(node_priority, NODE_PRIORITY_BASE)
            execute_node_priority(pc, state);
            ADVANCE()
         ENDOP()

         COMPLEX_JUMP(not_found)
#ifndef COMPUTED_GOTOS
         default:
#endif
            throw vm_exec_error("unsupported instruction");
         ENDOP()
#ifndef COMPUTED_GOTOS
      }
   }
#endif
}

static inline return_type
do_execute(byte_code code, state& state, const reg_num reg, vm::tuple *tpl, predicate *pred)
{
   assert(state.stack.empty());
   assert(state.removed.empty());

   state.hash_removes = false;
   state.generated_facts = false;
   state.linear_facts_generated = 0;
   state.persistent_facts_generated = 0;
   state.linear_facts_consumed = 0;

   const return_type ret(execute((pcounter)code, state, reg, tpl, pred));

   state.cleanup();
   assert(state.removed.empty());
   assert(state.stack.empty());
   return ret;
}
   
execution_return
execute_process(byte_code code, state& state, vm::tuple *tpl, predicate *pred)
{
   state.running_rule = false;
   const return_type ret(do_execute(code, state, 0, tpl, pred));
	
#ifdef CORE_STATISTICS
#endif
   
   if(ret == RETURN_LINEAR)
      return EXECUTION_CONSUMED;
	else
      return EXECUTION_OK;
}

void
execute_rule(const rule_id rule_id, state& state)
{
#ifdef DEBUG_RULES
   cout << "================> NODE " << state.node->get_id() << " ===============\n";
	cout << "Running rule " << theProgram->get_rule(rule_id)->get_string() << endl;
#endif
#ifdef CORE_STATISTICS
   execution_time::scope s(state.stat.rule_times[rule_id]);
#endif
#ifdef INSTRUMENTATION
   state.instr_rules_run++;
#endif
   
   //   state.node->print(cout);
   //   All->DATABASE->print_db(cout);
	
	vm::rule *rule(theProgram->get_rule(rule_id));

   state.running_rule = true;
#ifdef USE_JIT
   if(rule_id > 0)
      jit::compile_bytecode(rule->get_bytecode(), rule->get_codesize(), state);
   else
#endif
   {
      do_execute(rule->get_bytecode(), state, 0, NULL, NULL);
   }

#ifdef CORE_STATISTICS
   if(state.stat.stat_rules_activated == 0)
      state.stat.stat_rules_failed++;
#endif

}

}
