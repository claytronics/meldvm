#include <iostream>
#include <signal.h>

#include "ui/manager.hpp"
#include "process/machine.hpp"
#include "vm/program.hpp"
#include "vm/state.hpp"
#include "vm/exec.hpp"
#include "mem/thread.hpp"
#include "mem/stat.hpp"
#include "stat/stat.hpp"
#include "utils/fs.hpp"
#include "interface.hpp"
#include "sched/serial.hpp"
#include "sched/serial_ui.hpp"
#include "sched/sim.hpp"
#include "thread/threads.hpp"
#include "runtime/objs.hpp"
#include "thread/prio.hpp"

using namespace process;
using namespace db;
using namespace std;
using namespace vm;
using namespace boost;
using namespace sched;
using namespace mem;
using namespace utils;
using namespace statistics;

namespace process
{

void
machine::run_action(sched::base *sched, node* node, vm::tuple *tpl, vm::predicate *pred)
{
}

void
machine::execute_const_code(void)
{
}

}
