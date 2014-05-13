
#include <tr1/unordered_set>

#include "mem/center.hpp"

#ifdef ALLOCATOR_ASSERT
extern boost::mutex allocator_mtx;
extern std::tr1::unordered_set<void*> mem_set; 
#endif

namespace mem
{

void*
center::allocate(size_t cnt, size_t sz)
{
}

void
center::deallocate(void *p, size_t cnt, size_t sz)
{
}

}
