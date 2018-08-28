# OBJ Files with Metadata
  
This guide will show you how to encode and render Draco files encoded from OBJ files with metadata.


## Render three.js Example

Check out the three.js branch that has support for loading Draco files with metadata.

```
git clone git@github.com:FrankGalligan/three.js.git
cd three.js
git checkout render_draco_with_metadata
```

Render the sample Draco file with metadata.

```
python -m SimpleHTTPServer
```

Open `http://localhost:8000/examples/webgl_loader_draco_with_mat.html` in the browser. You should see a spinning cube with different color faces.

## Encode Draco File with Metadata

Encode a mesh file using the default encode settings with metadata. The input to `draco_encoder` must be an OBJ file that contains metadata. The `--metadata` command line option must be set:
```
./draco_encoder -i <input>.obj --metadata -o <output>.drc
```

## Render Encoded Draco File with Metadata

Open `examples/webgl_loader_draco_with_mat.html` and replace `mat_cube.drc` with `<output>.drc`.

Render your Draco file with metadata.

```
python -m SimpleHTTPServer
```

Open `http://localhost:8000/examples/webgl_loader_draco_with_mat.html` in the browser. You should see your encoded Draco file. 
