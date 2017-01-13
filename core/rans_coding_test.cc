#include "core/rans_coding.h"
#include "core/adaptive_rans_coding.h"
#include "core/draco_test_base.h"

// Just including rans_coding.h and adaptive_rans_coding.h gets an asan error
// when compiling (blaze test :rans_coding_test --config=asan)
TEST(RansCodingTest, LinkerTest) {}
