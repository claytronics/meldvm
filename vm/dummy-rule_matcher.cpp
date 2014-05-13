
#include <algorithm>
#include <iostream>

#include "vm/rule_matcher.hpp"
#include "vm/program.hpp"
#include "vm/state.hpp"

using namespace std;

//#define DEBUG_RULES 1

namespace vm
{

/* returns true if we did not have any tuples of this predicate */
bool
rule_matcher::register_tuple(predicate *pred, const derivation_count count, const bool is_new)
{
}

/* returns true if now we do not have any tuples of this predicate */
bool
rule_matcher::deregister_tuple(predicate *pred, const derivation_count count)
{
}

rule_matcher::rule_matcher(void)
{
}

rule_matcher::~rule_matcher(void)
{
}

}
