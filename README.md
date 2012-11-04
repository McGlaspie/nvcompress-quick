nvcompress-quick
================

nvcompress-quick is a fork of [NVIDIA Texture Tools](http://code.google.com/p/nvidia-texture-tools/), v2.0.8-1.

It contains an example of using CrystalDXT library for DXT compression. As CrystalDXT is distributed only for Windows platform, the example is included only for building in Visual Studio as nvtt-quick project.

CrystalDXT is designed to be used for quick compression of DXT1/DXT5 formats on the platforms where CUDA compression is not available. The library itself is closed source and can be found in lib/ directory.

nvtt-quick uses the same interface as original nvcompress from NVIDIA Texture Tools with two additional parameters:

* -nocrystal don't use CrystalDXT for DXT1/DXT5

* -datamode use real data in CrystalDXT compression (forced use for DXT1nm and DXT5nm formats)