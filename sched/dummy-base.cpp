
#include <algorithm>

#include "sched/base.hpp"
#include "process/work.hpp"
#include "db/tuple.hpp"
#include "vm/exec.hpp"
#include "process/machine.hpp"

using namespace std;
using namespace boost;
using namespace vm;
using namespace process;
using namespace db;

namespace sched
{
  
  volatile bool base::stop_flag(false);
  
}
