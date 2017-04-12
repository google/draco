#include "core/shannon_entropy.h"

#include <cmath>
#include <vector>

namespace draco {

int64_t ComputeShannonEntropy(const uint32_t *symbols, int num_symbols,
                              int max_value, int *out_num_unique_symbols) {
  // First find frequency of all unique symbols in the input array.
  int num_unique_symbols = 0;
  std::vector<int> symbol_frequencies(max_value + 1, 0);
  for (int i = 0; i < num_symbols; ++i) {
    ++symbol_frequencies[symbols[i]];
  }
  double total_bits = 0;
  double num_symbols_d = num_symbols;
  for (int i = 0; i < max_value + 1; ++i) {
    if (symbol_frequencies[i] > 0) {
      ++num_unique_symbols;
      // Compute Shannon entropy for the symbol.
      total_bits +=
          symbol_frequencies[i] *
          std::log2(static_cast<double>(symbol_frequencies[i]) / num_symbols_d);
    }
  }
  if (out_num_unique_symbols)
    *out_num_unique_symbols = num_unique_symbols;
  // Entropy is always negative.
  return -total_bits;
}

}  // namespace draco
