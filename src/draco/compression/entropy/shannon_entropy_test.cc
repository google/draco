#include "draco/compression/entropy/shannon_entropy.h"

#include "draco/core/draco_test_base.h"

namespace {

TEST(ShannonEntropyTest, TestBinaryEntropy) {
  // Test verifies that computing binary entropy works as expected.
  ASSERT_EQ(draco::ComputeBinaryShannonEntropy(0, 0), 0);
  ASSERT_EQ(draco::ComputeBinaryShannonEntropy(10, 0), 0);
  ASSERT_EQ(draco::ComputeBinaryShannonEntropy(10, 10), 0);
  ASSERT_NEAR(draco::ComputeBinaryShannonEntropy(10, 5), 1.0, 1e-4);
}

TEST(ShannonEntropyTest, TestStreamEntropy) {
  // Test verifies that the entropy of streamed data is computed correctly.
  const std::vector<uint32_t> symbols = {1, 5, 1, 100, 2, 1};

  draco::ShannonEntropyTracker entropy_tracker;

  // Nothing added, 0 entropy.
  ASSERT_EQ(entropy_tracker.GetNumberOfDataBits(), 0);

  // Try to push symbols one by one.
  uint32_t max_symbol = 0;
  for (int i = 0; i < symbols.size(); ++i) {
    if (symbols[i] > max_symbol) {
      max_symbol = symbols[i];
    }
    const auto entropy_data = entropy_tracker.Push(&symbols[i], 1);

    const int64_t stream_entropy_bits = entropy_tracker.GetNumberOfDataBits();
    // Ensure the returned entropy_data is in sync with the stream.
    ASSERT_EQ(draco::ShannonEntropyTracker::GetNumberOfDataBits(entropy_data),
              stream_entropy_bits);

    // Make sure the entropy is approximately the same as the one we compute
    // directly from all symbols.
    const int64_t expected_entropy_bits = draco::ComputeShannonEntropy(
        symbols.data(), i + 1, max_symbol, nullptr);

    // For now hardcoded tolerance of 2 bits.
    ASSERT_NEAR(expected_entropy_bits, stream_entropy_bits, 2);
  }

  // Compare it also to the case when we add all symbols in one call.
  draco::ShannonEntropyTracker entropy_tracker_2;
  entropy_tracker_2.Push(symbols.data(), symbols.size());
  const int64_t stream_2_entropy_bits = entropy_tracker_2.GetNumberOfDataBits();
  ASSERT_EQ(entropy_tracker.GetNumberOfDataBits(), stream_2_entropy_bits);

  // Ensure that peeking does not change the entropy.
  entropy_tracker_2.Peek(symbols.data(), 1);

  ASSERT_EQ(stream_2_entropy_bits, entropy_tracker_2.GetNumberOfDataBits());
}

TEST(ShannonEntropyTest, TestPeekMaximumSymbol) {
  // UINT32_MAX is what a prediction residual near INT32_MIN zig-zags to.
  // Peeking it counts it like any other symbol and leaves the table as it is.
  const std::vector<uint32_t> symbols = {0xffffffffu, 0xffffffffu, 3};

  draco::ShannonEntropyTracker entropy_tracker;
  const auto entropy_data =
      entropy_tracker.Peek(symbols.data(), static_cast<int>(symbols.size()));
  ASSERT_EQ(entropy_data.num_values, 3);
  ASSERT_EQ(entropy_data.num_unique_symbols, 2);
  // One symbol seen twice contributes 2 * log2(2), the other 1 * log2(1).
  ASSERT_NEAR(entropy_data.entropy_norm, 2.0, 1e-9);

  // A peek leaves the stream as it was.
  ASSERT_EQ(entropy_tracker.GetNumberOfDataBits(), 0);
}

}  // namespace
