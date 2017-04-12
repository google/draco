#include "core/adaptive_rans_bit_decoder.h"
#include "core/adaptive_rans_bit_encoder.h"
#include "core/draco_test_base.h"
#include "core/rans_bit_decoder.h"
#include "core/rans_bit_encoder.h"

// Just including rans_coding.h and adaptive_rans_coding.h gets an asan error
// when compiling (blaze test :rans_coding_test --config=asan)
TEST(RansCodingTest, LinkerTest) {}
