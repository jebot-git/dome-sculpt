// Test-only link overlay; NEVER a GPU, SDK, or GPU numerical proof.
// Force the real production aggregate staging/transaction branch while using
// the wholly instrumented native CPU SparseFieldSampler implementation.
#include <string_view>
#include <iostream>
extern "C" std::string_view aggregate_backend_name() asm("__wrap__ZN4dome15compute_backendEv");
extern "C" std::string_view aggregate_backend_name(){return "TEST-HOST-AGGREGATE";}
namespace {struct Notice {Notice(){std::cerr<<"TEST-ONLY HOST-CONTROL-FLOW: CPU sampler, no GPU/SDK/readback numerical claim.\n";}} notice;}
