
#include <iostream>

#include <cstdlib>
#include <cstring>
#include <assert.h>

#include "interface.hpp"
#include "process/router.hpp"
#include "stat/stat.hpp"
#include "utils/time.hpp"
#include "utils/fs.hpp"
#include "process/machine.hpp"
#include "ui/manager.hpp"
#include "vm/reader.hpp"

using namespace process;
using namespace sched;
using namespace std;
using namespace utils;
using namespace vm;

scheduler_type sched_type = SCHED_UNKNOWN;
size_t num_threads = 0;
bool show_database = false;
bool dump_database = false;
bool time_execution = false;
bool memory_statistics = false;

void
parse_sched(char *sched)
{
}

void
help_schedulers(void)
{
}

static inline void
finish(void)
{
}

bool
run_program(int argc, char **argv, const char *program, const vm::machine_arguments& margs, const char *data_file)
{
}
