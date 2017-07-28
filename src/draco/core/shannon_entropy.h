// Copyright 2016 The Draco Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#ifndef DRACO_CORE_SHANNON_ENTROPY_H_
#define DRACO_CORE_SHANNON_ENTROPY_H_

#include <stdint.h>

namespace draco {

// Computes an approximate Shannon entropy of symbols stored in the provided
// input array |symbols|. The entropy corresponds to the number of bits that is
// required to represent/store all the symbols using an optimal entropy coding
// algorithm. See for example "A mathematical theory of communication" by
// Shannon'48 (http://ieeexplore.ieee.org/document/6773024/).
//
// |max_value| is a required input that define the maximum value in the input
// |symbols| array.
//
// |out_num_unique_symbols| is an optional output argument that stores the
// number of unique symbols contained within the |symbols| array.
int64_t ComputeShannonEntropy(const uint32_t *symbols, int num_symbols,
                              int max_value, int *out_num_unique_symbols);

}  // namespace draco

#endif  // DRACO_CORE_SHANNON_ENTROPY_H_
