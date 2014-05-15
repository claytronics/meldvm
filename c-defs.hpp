#ifndef C_DEFS_HPP
# define C_DEFS_HPP

////////////////////////////////////
// VM:
//----Pointers to objects
typedef void VM_tuple;
typedef void VM_predicate;
typedef void VM_program;

//----Regular types
typedef unsigned char VM_predicate_id;
typedef size_t VM_strat_level;
typedef uint32_t VM_depth_t;
typedef short VM_derivation_count;

////////////////////////////////////
// DB:
//----Pointers to objects
typedef void DB_simple_tuple;

//----Regular types

////////////////////////////////////
// UTILS
//----Pointers to objects

//----Regular types
typedef unsigned char UTILS_byte;

#endif
