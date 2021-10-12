// Copyright 2017 The Draco Authors.
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

// Store all pre-main callbacks into the module to make them accessible later in
// the |onRuntimeInitialized| function. See more details in
// "prepareCallbacks.js".
Module['mainCallbacks'] = __ATMAIN__;

// Calls the 'onModuleParsed' callback if provided. This file is included as the
// last one in the generated javascript and it gives the caller a way to check
// that all previous content was successfully processed.
// Note: emscripten's |onRuntimeInitialized| is called before any --post-js
// files are included which is not equivalent to this callback.
if (typeof Module['onModuleParsed'] === 'function') {
  Module['onModuleParsed']();
}
