
#include <string>

#include "utils/utils.hpp"
#include "db/trie.hpp"
#include "db/agg_configuration.hpp"

using namespace vm;
using namespace std;
using namespace runtime;
using namespace std::tr1;

namespace db
{

static const size_t STACK_EXTRA_SIZE(3);
static const size_t TRIE_HASH_LIST_THRESHOLD(8);
static const size_t TRIE_HASH_BASE_BUCKETS(64);
static const size_t TRIE_HASH_MAX_NODES_PER_BUCKET(TRIE_HASH_LIST_THRESHOLD / 2);

void
tuple_trie::tuple_search_iterator::find_next(trie_continuation_frame& frm, const bool force_down)
{
}

   
}
