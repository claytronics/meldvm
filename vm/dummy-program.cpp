
#include <fstream>
#include <sys/stat.h>
#include <iostream>
#include <boost/static_assert.hpp>
#include <dlfcn.h>

#include "vm/program.hpp"
#include "db/tuple.hpp"
#include "vm/instr.hpp"
#include "db/database.hpp"
#include "utils/types.hpp"
#include "vm/state.hpp"
#include "vm/reader.hpp"
#include "vm/external.hpp"
#include "version.hpp"
#ifdef USE_UI
#include "ui/macros.hpp"
#endif

using namespace std;
using namespace db;
using namespace vm;
using namespace vm::instr;
using namespace process;
using namespace utils;

namespace vm {

  all* All;     // global variable that holds pointer to vm
  // all structure.  Set by process/machine.cpp
  // in constructor.
  program* theProgram;

  // most integers in the byte-code have 4 bytes
  BOOST_STATIC_ASSERT(sizeof(uint_val) == 4);

  program::program(const string& _filename):
    filename(_filename),
    init(NULL)
  {
  }

  program::~program(void)
  {
  }

  predicate*
  program::get_predicate(const predicate_id& id) const
  {
  }

  void
  program::print_bytecode(ostream& out) const
  {
  }

  void
  program::print_program(ostream& out) const
  {
  }

  void
  program::print_rules(ostream& out) const
  {
  }

  void
  program::print_predicates(ostream& cout) const
  {
  }

}
