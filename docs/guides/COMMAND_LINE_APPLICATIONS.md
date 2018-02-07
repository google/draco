# Using Draco Command Line Applications

This guide will show you how to build, and use the Draco command line
applications to encode and decode Draco files.

## Building

Clone the Github repository using this command line:
```
git clone git@github.com:<username>/draco.git
```
_**Note** that we **strongly** recommend [using SSH] with GitHub, not HTTPS._


Build the command line applications with these commands:
```
cd draco
```

It is best to create an isolated build directory.
```
mkdir build
cd build
```

Run cmake to build `draco_encoder` and `draco_decoder`.
```
cmake ..
make
```

## Encode Draco File

Encode a mesh file using the default settings. `draco_encoder` will read OBJ or PLY files as input, and output Draco files. We have included Stanford's Bunny mesh for testing. The basic command line looks like this:
```
./draco_encoder -i ../testdata/bun_zipper.ply -o bunny.drc
```


## Decode Draco File

`draco_decoder` will read Draco files as input, and output OBJ or PLY files. The basic command line looks like this:
```
./draco_decoder -i bunny.drc -o bunny.ply
```
