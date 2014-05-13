
#ifndef PREDICATE_HPP
#define PREDICATE_HPP

#include <string>
#include <vector>
#include <assert.h>

//#ifdef BLINKYBLOCKS
#include "db/linked_list.hpp"
//#endif
#include "vm/types.hpp"
#include "vm/defs.hpp"
#include "utils/types.hpp"
#include "vm/reader.hpp"
#include "vm/bitmap.hpp"

namespace vm {

const size_t PRED_DESCRIPTOR_BASE_SIZE = 4;
const size_t PRED_ARGS_MAX = 32;
const size_t PRED_NAME_SIZE_MAX = 32;
const size_t PRED_AGG_INFO_MAX = 32;

class program;

typedef enum {
   LINKED_LIST,
   HASH_TABLE
} store_type_t;

class predicate {
private:
   friend class program;
   
   static predicate_id current_id;
   
   predicate_id id;
   std::string name;
   strat_level level;
   
   std::vector<type*> types;
   std::vector<size_t> fields_size;
   std::vector<size_t> fields_offset;
   
   size_t tuple_size;
   
   typedef struct {
      field_num field;
      aggregate_type type;
      aggregate_safeness safeness;
      strat_level local_level;
      predicate_id remote_pred_id;
      predicate *remote_pred;
   } aggregate_info;
   
   aggregate_info *agg_info;
   
   bool is_route;
   bool is_linear;
   bool is_reverse_route;
   bool is_action;
   bool is_reused;
   bool is_cycle;

   std::vector<rule_id> affected_rules;
   bitmap rule_map;

   inline void add_affected_rule(const rule_id rule)
   {
      affected_rules.push_back(rule);
      rule_map.set_bit(rule);
   }

   store_type_t store_type;
   field_num hash_argument;

   // Linked tuple list to store tuples on Blinky Block version of vm
   //#ifdef BLINKYBLOCKS
   linked_list *tpl_list;
   //#endif

   // index of this predicate's arguments in the whole set of program's predicates
   size_t argument_position;
   
   void build_field_info(void);
   void build_aggregate_info(vm::program *);
   void cache_info(vm::program *);
   
   explicit predicate(void);

   void destroy(const size_t);
   ~predicate(void);
   
public:

   typedef std::vector<rule_id>::const_iterator rule_iterator;

   inline bitmap& get_rules_map(void) { return rule_map; }

   inline rule_iterator begin_rules(void) const
   {
      return affected_rules.begin();
   }

   inline rule_iterator end_rules(void) const
   {
      return affected_rules.end();
   }
   
   inline bool is_aggregate_pred(void) const { return agg_info != NULL; }
   
   inline aggregate_safeness get_agg_safeness(void) const { return agg_info->safeness; }
   inline bool is_unsafe_agg(void) const { return get_agg_safeness() == AGG_UNSAFE || get_agg_safeness() == AGG_IMMEDIATE; }
   inline const predicate *get_remote_pred(void) const { return agg_info->remote_pred; }
   strat_level get_agg_strat_level(void) const { return agg_info->local_level; }
   
   inline bool is_route_pred(void) const { return is_route || is_reverse_route; }

	inline bool is_reverse_route_pred(void) const { return is_reverse_route; }

   inline bool is_linear_pred(void) const { return is_linear; }
	inline bool is_persistent_pred(void) const { return !is_linear_pred(); }

   inline bool is_action_pred(void) const { return is_action; }

   inline bool is_reused_pred(void) const { return is_reused; }

   inline bool is_cycle_pred(void) const { return is_cycle; }
   
   inline field_num get_aggregate_field(void) const { return agg_info->field; }
   inline aggregate_type get_aggregate_type(void) const { return agg_info->type; }
   
   inline predicate_id get_id(void) const { return id; }
   
   inline size_t num_fields(void) const { return types.size(); }
   
   inline type* get_field_type(const field_num field) const { return types[field]; }
   inline size_t get_field_size(const field_num field) const { return fields_size[field]; }
   
   inline std::string get_name(void) const { return name; }
   
   inline size_t get_size(void) const { return tuple_size; }
   
   inline strat_level get_strat_level(void) const { return level; }

   inline void store_as_hash_table(const field_num field) {
      store_type = HASH_TABLE;
      hash_argument = field;
   }

   inline field_num get_hashed_field(void) const
   {
      assert(is_hash_table());
      return hash_argument;
   }

   inline bool is_hash_table(void) const
   {
      return store_type == HASH_TABLE;
   }

   inline void set_argument_position(const size_t arg)
   {
      argument_position = arg;
   }

   inline size_t get_argument_position(void) const
   {
      return argument_position;
   }
   
   void print_simple(std::ostream&) const;
   void print(std::ostream&) const;

   bool operator==(const predicate&) const;
   bool operator!=(const predicate& other) const
   {
      return !operator==(other);
   }
   
   static predicate* make_predicate_from_reader(code_reader&, code_size_t *,
         const predicate_id, const uint32_t, const uint32_t, const std::vector<type*>&,
         const size_t);
};

type* read_type_from_reader(code_reader&);
type* read_type_id_from_reader(code_reader&, const std::vector<type*>&);
std::ostream& operator<<(std::ostream&, const predicate&);

}

#endif
