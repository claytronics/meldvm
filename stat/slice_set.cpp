
#include <fstream>
#include <iostream>

#include "stat/slice_set.hpp"
#include "vm/state.hpp"
#include "process/machine.hpp"
#include "stat/stat.hpp"
#include "utils/macros.hpp"

using namespace vm;
using namespace sched;
using namespace std;
using namespace utils;

namespace statistics
{

static void
write_header(ofstream& out, const string& header, vm::all *all)
{
   csv_line line;

   line.set_header();

   line << header;

   for(size_t i(0); i < all->NUM_THREADS; ++i)
      line << (string("thread") + to_string<size_t>(i));

   line.print(out);
}
  
void
slice_set::write_general(const string& file, const string& title,
   print_fn fun, vm::all *all) const
{
   ofstream out(file.c_str(), ios_base::out | ios_base::trunc);
   
   write_header(out, title, all);
   
   vector<iter> iters(all->NUM_THREADS, iter());

	 assert(slices.size() == all->NUM_THREADS);

   for(size_t i(0); i < all->NUM_THREADS; ++i) {
		  assert(num_slices == slices[i].size());
      iters[i] = slices[i].begin();
	 }
      
   for(size_t slice(0); slice < num_slices; ++slice) {
      csv_line line;
      line << to_string<unsigned long>(slice * SLICE_PERIOD);
      for(size_t proc(0); proc < all->NUM_THREADS; ++proc) {
         assert(iters[proc] != slices[proc].end());
         const statistics::slice& sl(*iters[proc]);
         CALL_MEMBER_FN(sl, fun)(line);
         iters[proc]++;
      }
      line.print(out);
   }
}

void
slice_set::write_state(const string& file, vm::all *all) const
{
   write_general(file + ".state", "state", &slice::print_state, all);
}

void
slice_set::write_derived_facts(const string& file, vm::all *all) const
{
   write_general(file + ".derived_facts", "derivedfacts", &slice::print_derived_facts, all);
}

void
slice_set::write_consumed_facts(const string& file, vm::all *all) const
{
   write_general(file + ".consumed_facts", "consumedfacts", &slice::print_consumed_facts, all);
}

void
slice_set::write_rules_run(const string& file, vm::all *all) const
{
   write_general(file + ".rules_run", "rulesrun", &slice::print_rules_run, all);
}

void
slice_set::write_stolen_nodes(const string& file, vm::all *all) const
{
   write_general(file + ".stolen_nodes", "stolennodes", &slice::print_stolen_nodes, all);
}

void
slice_set::write_sent_facts_same_thread(const string& file, vm::all *all) const
{
   write_general(file + ".sent_facts_same_thread", "sentfactssamethread", &slice::print_sent_facts_same_thread, all);
}

void
slice_set::write_sent_facts_other_thread(const string& file, vm::all *all) const
{
   write_general(file + ".sent_facts_other_thread", "sentfactsotherthread", &slice::print_sent_facts_other_thread, all);
}

void
slice_set::write_sent_facts_other_thread_now(const string& file, vm::all *all) const
{
   write_general(file + ".sent_facts_other_thread_now", "sentfactsotherthreadnow", &slice::print_sent_facts_other_thread_now, all);
}

void
slice_set::write(const string& file, const scheduler_type type, vm::all *all) const
{
   (void)type;
   write_state(file, all);
   write_derived_facts(file, all);
   write_consumed_facts(file, all);
   write_rules_run(file, all);
   write_stolen_nodes(file, all);
   write_sent_facts_same_thread(file, all);
   write_sent_facts_other_thread(file, all);
   write_sent_facts_other_thread_now(file, all);
#if 0
   if(is_priority_sched(type))
      write_priority_queue(file, all);
#endif
}
   
void
slice_set::beat_thread(const process_id id, slice& sl, vm::all *all)
{
   all->MACHINE->get_scheduler(id)->write_slice(sl);
}

void
slice_set::beat(vm::all *all)
{
   for(size_t i(0); i < all->NUM_THREADS; ++i) {
      slice sl;
      
      beat_thread(i, sl, all);
      
      slices[i].push_back(sl);
   }
   
   ++num_slices;
}
   
slice_set::slice_set(const size_t num_threads):
   num_slices(0),
   slices(num_threads, list_slices())
{
}

}
