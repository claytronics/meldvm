
#include <queue>

#include "vm/state.hpp"
#include "process/machine.hpp"
#include "vm/exec.hpp"
#ifdef USE_SIM
#include "sched/nodes/sim.hpp"
#endif

using namespace vm;
using namespace db;
using namespace process;
using namespace std;
using namespace runtime;
using namespace utils;

//#define DEBUG_RULES
//#define DEBUG_INDEXING

namespace vm
{

void
state::cleanup(void)
{
}

}
