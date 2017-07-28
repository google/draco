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
#ifndef DRACO_MESH_EDGEBREAKER_OBSERVER_H_
#define DRACO_MESH_EDGEBREAKER_OBSERVER_H_

namespace draco {

// A default implementation of the observer that can be used inside of the
// EdgeBreakerTraverser (edgebreaker_traverser.h). The default implementation
// results in no-op for all calls.
class EdgeBreakerObserver {
 public:
  inline void OnSymbolC(){};
  inline void OnSymbolL(){};
  inline void OnSymbolR(){};
  inline void OnSymbolS(){};
  inline void OnSymbolE(){};
};

}  // namespace draco

#endif  // DRACO_MESH_EDGEBREAKER_OBSERVER_H_
