
#include <tr1/unordered_map>
#include <assert.h>
#include <stdlib.h>

#include "vm/external.hpp"
#include "external/math.hpp"
#include "external/utils.hpp"
#include "external/lists.hpp"
#include "external/structs.hpp"
#include "external/strings.hpp"
#include "external/others.hpp"
#include "external/core.hpp"

using namespace std;
using namespace std::tr1;

namespace vm
{
   
using namespace external;

typedef unordered_map<external_function_id, external_function*> hash_external_type;

static external_function_id external_counter(0);
static external_function_id first_custom(0);
static hash_external_type hash_external;
static bool external_functions_initiated(false);

external_function*
lookup_external_function(const external_function_id id)
{
}

external_function_id first_custom_external_function(void)
{
}

}
