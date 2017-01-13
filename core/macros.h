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
#ifndef DRACO_CORE_MACROS_H_
#define DRACO_CORE_MACROS_H_

#include "assert.h"

#ifdef ANDROID_LOGGING
#include <android/log.h>
#define LOG_TAG "draco"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI printf
#define LOGE printf
#endif

#include <iostream>
namespace draco {

#define CHECK(x) (assert(x));
#define CHECK_EQ(a, b) assert((a) == (b));
#define CHECK_GE(a, b) assert((a) >= (b));
#define CHECK_GT(a, b) assert((a) > (b));
#define CHECK_NE(a, b) assert((a) != (b));
#define CHECK_NOTNULL(x) assert((x) != NULL);

#define DCHECK(x) (assert(x));
#define DCHECK_EQ(a, b) assert((a) == (b));
#define DCHECK_GE(a, b) assert((a) >= (b));
#define DCHECK_GT(a, b) assert((a) > (b));
#define DCHECK_LE(a, b) assert((a) <= (b));
#define DCHECK_LT(a, b) assert((a) < (b));

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName &) = delete;     \
  void operator=(const TypeName &) = delete;

#ifndef FALLTHROUGH_INTENDED
#define FALLTHROUGH_INTENDED void(0);
#endif

#ifndef LOG
#define LOG(...) std::cout
#endif

#ifndef VLOG
#define VLOG(...) std::cout
#endif

}  // namespace draco


#endif  // DRACO_CORE_MACROS_H_
