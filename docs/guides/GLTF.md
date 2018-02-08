# glTF

Using [glTF](https://www.khronos.org/gltf/) with Draco.

## Encode Draco glTF Using gltf-pipeline

### Prerequisites

Install [node.js](https://nodejs.org) 6.0 or greater. See the node.js [downloads](https://nodejs.org/en/download/) for details.

### Installing

Clone this [forked repository](https://github.com/FrankGalligan/gltf-pipeline) of gltf-pipeline using this command line:
```
git clone git@github.com:FrankGalligan/gltf-pipeline.git
cd gltf-pipeline
```
_**Note** that we **strongly** recommend [using SSH] with GitHub, not HTTPS._

Check out the Draco compression extension branch.
```
git checkout -b draco_compression_extension origin/draco_compression_extension
```

Install all the dependencies.
```
npm install
```


### Encode Draco glTF

Encode the Draco glTF using default settings.
```
node ./bin/gltf-pipeline.js -i <in>.gltf -d -s -o <out>.gltf
```

The encoded glTF data will be written to the `output` folder.


## Render Draco glTF Using three-gltf-viewer


