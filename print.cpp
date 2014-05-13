
#include <cstdlib>
#include <iostream>

#include "vm/program.hpp"

using namespace vm;
using namespace process;
using namespace std;

int
main(int argc, char **argv)
{
   if(argc < 2) {
      fprintf(stderr, "usage: print <bytecode file> [code | rules | info | prog]\n");
      return EXIT_FAILURE;
   }
   
   const string file(argv[1]);

   try {
      program prog(file);
      if(argc == 2)
         prog.print_bytecode(cout);
      if(argc == 3) {
         const string arg(argv[2]);

         if(arg == "code") {
            prog.print_bytecode(cout);
         } else if(arg == "rules") {
            prog.print_rules(cout);
         } else if(arg == "info") {
            prog.print_predicates(cout);
         } else if(arg == "prog") {
            prog.print_program(cout);
         } else {
            cerr << "Don't know what to do" << endl;
         }
      }
   } catch(vm::load_file_error& err) {
      cerr << "File error: " << err.what() << endl;
      exit(EXIT_FAILURE);
   }
   
   return EXIT_SUCCESS;
}
