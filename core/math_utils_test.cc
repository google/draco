#include "core/math_utils.h"
#include "core/draco_test_base.h"

TEST(MathUtils, Mod) { EXPECT_EQ(DRACO_INCREMENT_MOD(1, 1 << 1), 0); }
