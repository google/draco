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
'use strict';

// |dracoPath| sets the path for the Draco decoder source files. The default
// path is "./". If |dracoDecoderType|.type is set to "js", then DRACOLoader
// will load the Draco JavaScript decoder.
THREE.DRACOLoader = function(dracoPath, dracoDecoderType, manager) {
    this.timeLoaded = 0;
    this.manager = (manager !== undefined) ? manager :
        THREE.DefaultLoadingManager;
    this.materials = null;
    this.verbosity = 0;
    this.attributeOptions = {};
    if (dracoDecoderType !== undefined) {
      THREE.DRACOLoader.dracoDecoderType = dracoDecoderType;
    }
    this.drawMode = THREE.TrianglesDrawMode;
    this.dracoSrcPath = (dracoPath !== undefined) ? dracoPath : './';
    // If draco_decoder.js or wasm code is already loaded/included, then do
    // not dynamically load the decoder.
    if (typeof DracoDecoderModule === 'undefined') {
      THREE.DRACOLoader.loadDracoDecoder(this);
    }
    // User defined unique id for attributes.
    this.attributeUniqueIdMap = {};
    // Native Draco attribute type to Three.JS attribute type.
    this.nativeAttributeMap = {
      'position' : 'POSITION',
      'normal' : 'NORMAL',
      'color' : 'COLOR',
      'uv' : 'TEX_COORD'
    };
};

THREE.DRACOLoader.dracoDecoderType = {};

THREE.DRACOLoader.prototype = {

    constructor: THREE.DRACOLoader,

    load: function(url, onLoad, onProgress, onError) {
        var scope = this;
        var loader = new THREE.FileLoader(scope.manager);
        loader.setPath(this.path);
        loader.setResponseType('arraybuffer');
        if (this.crossOrigin !== undefined) {
          loader.crossOrigin = this.crossOrigin;
        }
        loader.load(url, function(blob) {
            scope.decodeDracoFile(blob, onLoad);
        }, onProgress, onError);
    },

    setPath: function(value) {
        this.path = value;
    },

    setCrossOrigin: function(value) {
        this.crossOrigin = value;
    },

    setVerbosity: function(level) {
        this.verbosity = level;
    },

    /**
     *  Sets desired mode for generated geometry indices.
     *  Can be either:
     *      THREE.TrianglesDrawMode
     *      THREE.TriangleStripDrawMode
     */
    setDrawMode: function(drawMode) {
        this.drawMode = drawMode;
    },

    /**
     * Skips dequantization for a specific attribute.
     * |attributeName| is the THREE.js name of the given attribute type.
     * The only currently supported |attributeName| is 'position', more may be
     * added in future.
     */
    setSkipDequantization: function(attributeName, skip) {
        var skipDequantization = true;
        if (typeof skip !== 'undefined')
          skipDequantization = skip;
        this.getAttributeOptions(attributeName).skipDequantization =
            skipDequantization;
    },

    /**
     * |attributeUniqueIdMap| specifies attribute unique id for an attribute in
     * the geometry to be decoded. The name of the attribute must be one of the
     * supported attribute type in Three.JS, including:
     *     'position',
     *     'color',
     *     'normal',
     *     'uv',
     *     'uv2',
     *     'skinIndex',
     *     'skinWeight'.
     * The format is:
     *     attributeUniqueIdMap[attributeName] = attributeId
     */
    decodeDracoFile: function(rawBuffer, callback, attributeUniqueIdMap) {
      var scope = this;
      this.attributeUniqueIdMap = (attributeUniqueIdMap !== undefined) ?
          attributeUniqueIdMap : {};
      THREE.DRACOLoader.getDecoderModule(
          function(dracoDecoder) {
            scope.decodeDracoFileInternal(rawBuffer, dracoDecoder, callback);
      });
    },

    decodeDracoFileInternal: function(rawBuffer, dracoDecoder, callback) {
      /*
       * Here is how to use Draco Javascript decoder and get the geometry.
       */
      var buffer = new dracoDecoder.DecoderBuffer();
      buffer.Init(new Int8Array(rawBuffer), rawBuffer.byteLength);
      var decoder = new dracoDecoder.Decoder();

      /*
       * Determine what type is this file: mesh or point cloud.
       */
      var geometryType = decoder.GetEncodedGeometryType(buffer);
      if (geometryType == dracoDecoder.TRIANGULAR_MESH) {
        if (this.verbosity > 0) {
          console.log('Loaded a mesh.');
        }
      } else if (geometryType == dracoDecoder.POINT_CLOUD) {
        if (this.verbosity > 0) {
          console.log('Loaded a point cloud.');
        }
      } else {
        var errorMsg = 'THREE.DRACOLoader: Unknown geometry type.'
        console.error(errorMsg);
        throw new Error(errorMsg);
      }
      callback(this.convertDracoGeometryTo3JS(dracoDecoder, decoder,
          geometryType, buffer));
    },

    addAttributeToGeometry: function(dracoDecoder, decoder, dracoGeometry,
                                     attributeName, attribute, geometry,
                                     geometryBuffer) {
      if (attribute.ptr === 0) {
        var errorMsg = 'THREE.DRACOLoader: No attribute ' + attributeName;
        console.error(errorMsg);
        throw new Error(errorMsg);
      }
      var numComponents = attribute.num_components();
      var attributeData = new dracoDecoder.DracoFloat32Array();
      decoder.GetAttributeFloatForAllPoints(
          dracoGeometry, attribute, attributeData);
      var numPoints = dracoGeometry.num_points();
      var numValues = numPoints * numComponents;
      // Allocate space for attribute.
      geometryBuffer[attributeName] = new Float32Array(numValues);
      // Copy data from decoder.
      for (var i = 0; i < numValues; i++) {
        geometryBuffer[attributeName][i] = attributeData.GetValue(i);
      }
      // Add attribute to THREEJS geometry for rendering.
      geometry.addAttribute(attributeName,
          new THREE.Float32BufferAttribute(geometryBuffer[attributeName],
            numComponents));
      dracoDecoder.destroy(attributeData);
    },

    convertDracoGeometryTo3JS: function(dracoDecoder, decoder, geometryType,
                                        buffer) {
        if (this.getAttributeOptions('position').skipDequantization === true) {
          decoder.SkipAttributeTransform(dracoDecoder.POSITION);
        }
        var dracoGeometry;
        var decodingStatus;
        const start_time = performance.now();
        if (geometryType === dracoDecoder.TRIANGULAR_MESH) {
          dracoGeometry = new dracoDecoder.Mesh();
          decodingStatus = decoder.DecodeBufferToMesh(buffer, dracoGeometry);
        } else {
          dracoGeometry = new dracoDecoder.PointCloud();
          decodingStatus =
              decoder.DecodeBufferToPointCloud(buffer, dracoGeometry);
        }
        if (!decodingStatus.ok() || dracoGeometry.ptr == 0) {
          var errorMsg = 'THREE.DRACOLoader: Decoding failed: ';
          errorMsg += decodingStatus.error_msg();
          console.error(errorMsg);
          dracoDecoder.destroy(decoder);
          dracoDecoder.destroy(dracoGeometry);
          throw new Error(errorMsg);
        }

        var decode_end = performance.now();
        dracoDecoder.destroy(buffer);
        /*
         * Example on how to retrieve mesh and attributes.
         */
        var numFaces;
        if (geometryType == dracoDecoder.TRIANGULAR_MESH) {
          numFaces = dracoGeometry.num_faces();
          if (this.verbosity > 0) {
            console.log('Number of faces loaded: ' + numFaces.toString());
          }
        } else {
          numFaces = 0;
        }

        var numPoints = dracoGeometry.num_points();
        var numAttributes = dracoGeometry.num_attributes();
        if (this.verbosity > 0) {
          console.log('Number of points loaded: ' + numPoints.toString());
          console.log('Number of attributes loaded: ' +
              numAttributes.toString());
        }

        // Verify if there is position attribute.
        var posAttId = decoder.GetAttributeId(dracoGeometry,
                                              dracoDecoder.POSITION);
        if (posAttId == -1) {
          var errorMsg = 'THREE.DRACOLoader: No position attribute found.';
          console.error(errorMsg);
          dracoDecoder.destroy(decoder);
          dracoDecoder.destroy(dracoGeometry);
          throw new Error(errorMsg);
        }
        var posAttribute = decoder.GetAttribute(dracoGeometry, posAttId);

        // Structure for converting to THREEJS geometry later.
        var geometryBuffer = {};
        // Import data to Three JS geometry.
        var geometry = new THREE.BufferGeometry();

        // Add native Draco attribute type to geometry.
        for (var attributeName in this.nativeAttributeMap) {
          // The native attribute type is only used when no unique Id is
          // provided. For example, loading .drc files.
          if (this.attributeUniqueIdMap[attributeName] === undefined) {
            var attId = decoder.GetAttributeId(dracoGeometry,
                dracoDecoder[this.nativeAttributeMap[attributeName]]);
            if (attId !== -1) {
              if (this.verbosity > 0) {
                console.log('Loaded ' + attributeName + ' attribute.');
              }
              var attribute = decoder.GetAttribute(dracoGeometry, attId);
              this.addAttributeToGeometry(dracoDecoder, decoder, dracoGeometry,
                  attributeName, attribute, geometry, geometryBuffer);
            }
          }
        }

        // Add attributes of user specified unique id. E.g. GLTF models.
        for (var attributeName in this.attributeUniqueIdMap) {
          var attributeId = this.attributeUniqueIdMap[attributeName];
          var attribute = decoder.GetAttributeByUniqueId(dracoGeometry,
                                                         attributeId);
          this.addAttributeToGeometry(dracoDecoder, decoder, dracoGeometry,
              attributeName, attribute, geometry, geometryBuffer);
        }

        // For mesh, we need to generate the faces.
        if (geometryType == dracoDecoder.TRIANGULAR_MESH) {
          if (this.drawMode === THREE.TriangleStripDrawMode) {
            var stripsArray = new dracoDecoder.DracoInt32Array();
            var numStrips = decoder.GetTriangleStripsFromMesh(
                dracoGeometry, stripsArray);
            geometryBuffer.indices = new Uint32Array(stripsArray.size());
            for (var i = 0; i < stripsArray.size(); ++i) {
              geometryBuffer.indices[i] = stripsArray.GetValue(i);
            }
            dracoDecoder.destroy(stripsArray);
          } else {
            var numIndices = numFaces * 3;
            geometryBuffer.indices = new Uint32Array(numIndices);
            var ia = new dracoDecoder.DracoInt32Array();
            for (var i = 0; i < numFaces; ++i) {
              decoder.GetFaceFromMesh(dracoGeometry, i, ia);
              var index = i * 3;
              geometryBuffer.indices[index] = ia.GetValue(0);
              geometryBuffer.indices[index + 1] = ia.GetValue(1);
              geometryBuffer.indices[index + 2] = ia.GetValue(2);
            }
            dracoDecoder.destroy(ia);
         }
        }

        geometry.drawMode = this.drawMode;
        if (geometryType == dracoDecoder.TRIANGULAR_MESH) {
          geometry.setIndex(new(geometryBuffer.indices.length > 65535 ?
                THREE.Uint32BufferAttribute : THREE.Uint16BufferAttribute)
              (geometryBuffer.indices, 1));
        }
        var posTransform = new dracoDecoder.AttributeQuantizationTransform();
        if (posTransform.InitFromAttribute(posAttribute)) {
          // Quantized attribute. Store the quantization parameters into the
          // THREE.js attribute.
          geometry.attributes['position'].isQuantized = true;
          geometry.attributes['position'].maxRange = posTransform.range();
          geometry.attributes['position'].numQuantizationBits =
              posTransform.quantization_bits();
          geometry.attributes['position'].minValues = new Float32Array(3);
          for (var i = 0; i < 3; ++i) {
            geometry.attributes['position'].minValues[i] =
                posTransform.min_value(i);
          }
        }
        dracoDecoder.destroy(posTransform);
        dracoDecoder.destroy(decoder);
        dracoDecoder.destroy(dracoGeometry);

        this.decode_time = decode_end - start_time;
        this.import_time = performance.now() - decode_end;

        if (this.verbosity > 0) {
          console.log('Decode time: ' + this.decode_time);
          console.log('Import time: ' + this.import_time);
        }
        return geometry;
    },

    isVersionSupported: function(version, callback) {
        THREE.DRACOLoader.getDecoderModule(
            function(decoder) {
              callback(decoder.isVersionSupported(version));
            });
    },

    getAttributeOptions: function(attributeName) {
        if (typeof this.attributeOptions[attributeName] === 'undefined')
          this.attributeOptions[attributeName] = {};
        return this.attributeOptions[attributeName];
    }
};

// This function loads a JavaScript file and adds it to the page. "path"
// is the path to the JavaScript file. "onLoadFunc" is the function to be
// called when the JavaScript file has been loaded.
THREE.DRACOLoader.loadJavaScriptFile = function(path, onLoadFunc,
    dracoDecoder) {
  var previous_decoder_script = document.getElementById("decoder_script");
  if (previous_decoder_script !== null) {
    return;
  }
  var head = document.getElementsByTagName('head')[0];
  var element = document.createElement('script');
  element.id = "decoder_script";
  element.type = 'text/javascript';
  element.src = path;
  if (onLoadFunc !== null) {
    element.onload = onLoadFunc(dracoDecoder);
  } else {
    element.onload = function(dracoDecoder) {
      THREE.DRACOLoader.timeLoaded = performance.now();
    };
  }
  head.appendChild(element);
}

THREE.DRACOLoader.loadWebAssemblyDecoder = function(dracoDecoder) {
  THREE.DRACOLoader.dracoDecoderType['wasmBinaryFile'] =
      dracoDecoder.dracoSrcPath + 'draco_decoder.wasm';
  var xhr = new XMLHttpRequest();
  xhr.open('GET', dracoDecoder.dracoSrcPath + 'draco_decoder.wasm', true);
  xhr.responseType = 'arraybuffer';
  xhr.onload = function() {
    // draco_wasm_wrapper.js must be loaded before DracoDecoderModule is
    // created. The object passed into DracoDecoderModule() must contain a
    // property with the name of wasmBinary and the value must be an
    // ArrayBuffer containing the contents of the .wasm file.
    THREE.DRACOLoader.dracoDecoderType['wasmBinary'] = xhr.response;
    THREE.DRACOLoader.timeLoaded = performance.now();
  };
  xhr.send(null)
}

// This function will test if the browser has support for WebAssembly. If
// it does it will download the WebAssembly Draco decoder, if not it will
// download the asmjs Draco decoder.
THREE.DRACOLoader.loadDracoDecoder = function(dracoDecoder) {
  if (typeof WebAssembly !== 'object' ||
      THREE.DRACOLoader.dracoDecoderType.type === 'js') {
    // No WebAssembly support
    THREE.DRACOLoader.loadJavaScriptFile(dracoDecoder.dracoSrcPath +
        'draco_decoder.js', null, dracoDecoder);
  } else {
    THREE.DRACOLoader.loadJavaScriptFile(dracoDecoder.dracoSrcPath +
        'draco_wasm_wrapper.js',
        function (dracoDecoder) {
          THREE.DRACOLoader.loadWebAssemblyDecoder(dracoDecoder);
        }, dracoDecoder);
  }
}

THREE.DRACOLoader.decoderCreationCalled = false;

/**
 * Creates and returns a singleton instance of the DracoDecoderModule.
 * The module loading is done asynchronously for WebAssembly. Initialized module
 * can be accessed through the callback function
 * |onDracoDecoderModuleLoadedCallback|.
 */
THREE.DRACOLoader.getDecoderModule = (function() {
    return function(onDracoDecoderModuleLoadedCallback) {
        if (typeof THREE.DRACOLoader.decoderModule !== 'undefined') {
          // Module already initialized.
          if (typeof onDracoDecoderModuleLoadedCallback !== 'undefined') {
            onDracoDecoderModuleLoadedCallback(THREE.DRACOLoader.decoderModule);
          }
        } else {
          if (typeof DracoDecoderModule === 'undefined') {
            // Wait until the Draco decoder is loaded before starting the error
            // timer.
            if (THREE.DRACOLoader.timeLoaded > 0) {
              var waitMs = performance.now() - THREE.DRACOLoader.timeLoaded;

              // After loading the Draco JavaScript decoder file, there is still
              // some time before the DracoDecoderModule is defined. So start a
              // loop to check when the DracoDecoderModule gets defined. If the
              // time is hit throw an error.
              if (waitMs > 5000) {
                throw new Error(
                    'THREE.DRACOLoader: DracoDecoderModule not found.');
              }
            }
          } else {
            if (!THREE.DRACOLoader.decoderCreationCalled) {
              THREE.DRACOLoader.decoderCreationCalled = true;
              THREE.DRACOLoader.dracoDecoderType['onModuleLoaded'] =
                  function(module) {
                    THREE.DRACOLoader.decoderModule = module;
                  };
              DracoDecoderModule(THREE.DRACOLoader.dracoDecoderType);
            }
          }

          // Either the DracoDecoderModule has not been defined or the decoder
          // has not been created yet. Call getDecoderModule() again.
          setTimeout(function() {
            THREE.DRACOLoader.getDecoderModule(
                onDracoDecoderModuleLoadedCallback);
          }, 10);
        }
    };

})();

// Releases the DracoDecoderModule instance associated with the draco loader.
// The module will be automatically re-created the next time a new geometry is
// loaded by THREE.DRACOLoader.
THREE.DRACOLoader.releaseDecoderModule = function() {
  THREE.DRACOLoader.decoderModule = undefined;
  THREE.DRACOLoader.decoderCreationCalled = false;
  if (THREE.DRACOLoader.dracoDecoderType !== undefined &&
      THREE.DRACOLoader.dracoDecoderType.wasmBinary !== undefined) {
    // For WASM build we need to preserve the wasmBinary for future use.
    var wasmBinary = THREE.DRACOLoader.dracoDecoderType.wasmBinary;
    THREE.DRACOLoader.dracoDecoderType = {};
    THREE.DRACOLoader.dracoDecoderType.wasmBinary = wasmBinary;
  } else {
    THREE.DRACOLoader.dracoDecoderType = {};
  }
}
