#include <assert.h>
#include <iostream>
#include <sstream>
#include <limits>

#include "vm/tuple.hpp"
#include "db/node.hpp"
#include "utils/utils.hpp"
#include "vm/state.hpp"
#include "utils/serialization.hpp"
#ifdef USE_UI
#include "ui/macros.hpp"
#endif
#include "db/node.hpp"

using namespace vm;
using namespace std;
using namespace runtime;
using namespace utils;
using namespace boost;

namespace vm
{
   
  void
  tuple::destructor(predicate *pred)
  {
  }
  
}
