
#include "vm/defs.hpp"
#include "db/database.hpp"
#include "process/router.hpp"
#include "vm/state.hpp"

using namespace db;
using namespace std;
using namespace vm;
using namespace process;
using namespace utils;

namespace db
{
   
database::database(const string& filename, create_node_fn _create_fn)
{
}

database::~database(void)
{
}

node*
database::create_node(void)
{
}

}
