#!/bin/sh
#
# Replace all the C++ types by C equivalents
#for s in */*.hpp */*.cpp; do

for s in db/tuple.hpp; do
   
   # Set output variables
   dir=c-${s%/*.h}/
   out=c-$s

   # Create destination folder if does not exist
   if [ ! -d $dir ]; then
       echo "creating $dir"
       mkdir $dir;
   fi
   
   # Copy output file in it
   echo "creating output file: $out"
   cp $s $out

   # Start replacing
   echo "replacing types"
   sed -i 's/vm::tuple/VM_tuple/g' $out
   sed -i 's/vm::predicate/VM_predicate/g' $out
   sed -i 's/vm::strat_level/VM_strat_level/g' $out
   sed -i 's/vm::predicate_id/VM_predicate_id/g' $out
   sed -i 's/vm::depth_t/VM_depth_t/g' $out
   sed -i 's/vm::derivation_count/VM_derivation_count/g' $out
   sed -i 's/utils::byte/UTILS_byte/g' $out
   sed -i 's/vm::program/VM_program/g' $out
   sed -i 's/::destroy/_destroy/g' $out
   
   echo "done"
done
