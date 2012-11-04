#include <stdio.h>
#include <nvmath/Color.h>
#include "CrystalCompressor.h"
#include <nvimage/Image.h>

#include <nvtt/Compressor.h>
#include <nvtt/CompressionOptions.h>
#include <nvtt/OutputOptions.h>
#include <nvtt/InputOptions.h>

// simple read
void readCallback(void* handle, unsigned char* bgra, int x, int y);
// alpha = 255
void readCallbackBc1(void* handle, unsigned char* bgra, int x, int y);
// alpha = 255, blue = 0
void readCallbackBc1n(void* handle, unsigned char* bgra, int x, int y);
// alpha = red, red = 255, blue = 0
void readCallbackBc3n(void* handle, unsigned char* bgra, int x, int y);

CrystalCompressor::CrystalCompressor(): Compressor(), nocrystal(true), datamode(false), bc1n(false)
{
	crystallLib = LoadLibrary(TEXT("crystaldxt_compress.dll"));
	if(crystallLib != NULL)
	{
		printf("Found CrystalDXT library: it will be used for DXT1/DXT5 if CUDA is not present\n");
	}
	else
	{
		printf("warning: crystaldxt_compress.dll not found.\nOptions -nocrystal, -datamode won't have any effect\n\n");
	}
}
bool CrystalCompressor::process(const nvtt::InputOptions & inputOptions, 
								const nvtt::CompressionOptions & compressionOptions, 
								const nvtt::OutputOptions & outputOptions) const
{
	using namespace nvtt;

	bool dm = datamode; // dancing around const method
	Format format = compressionOptions.m.format;
	if (!isCudaAccelerationEnabled() && 
		!nocrystal && crystallLib &&
		(format == nvtt::Format_BC1 || format == nvtt::Format_BC3 ||
		format == nvtt::Format_BC1a || format == nvtt::Format_BC3n) &&
		compressionOptions.m.quality != Quality_Fastest)
	{
		printf("Using CrystalDXT library\n\n");
		CLCompressFunc compress = (CLCompressFunc)GetProcAddress(crystallLib, "DxtImageCompressParallel");

		bool isDxt1;
		// defining appropriate callback for reading image
		CLDxtCallback callback;
		if (format == Format_BC1 && bc1n)
		{
			callback = readCallbackBc1n;
			dm = true;
			isDxt1 = true;
		}
		else if (format == Format_BC1 && !bc1n)
		{
			callback = readCallbackBc1;
			isDxt1 = true;
		}
		else if (format == Format_BC1a)
		{
			callback = readCallback;
			isDxt1 = true;
		}
		else if (format == Format_BC3)
		{
			callback = readCallback;
			isDxt1 = false;
		}
		else //if (format == Format_BC3n)
		{
			callback = readCallbackBc3n;
			dm = true;
			isDxt1 = false;
		}

		if (!outputOptions.m.openFile())
		{
			if (outputOptions.m.errorHandler) outputOptions.m.errorHandler->error(Error_FileOpen);
			return false;
		}

		inputOptions.m.computeTargetExtents();
	
		// Output DDS header.
		if (!this->m.outputHeader(inputOptions.m, compressionOptions.m, outputOptions.m))
		{
			return false;
		}

		uint w = inputOptions.m.targetWidth;
		uint h = inputOptions.m.targetHeight;
		uint d = inputOptions.m.targetDepth;
		Mipmap mipmap;
		const uint mipmapCount = inputOptions.m.realMipmapCount();

		for (uint f = 0; f < inputOptions.m.faceCount; f++)
			for (uint m = 0; m < mipmapCount; m++) 
			{
				int size = ((w+3)/4) * ((h+3)/4) * (isDxt1 ? 8 : 16);
				if (outputOptions.m.outputHandler)
					outputOptions.m.outputHandler->beginImage(size, w, h, d, f, m);

				void* dest_dxt = (void*)malloc(size * sizeof(uint8));
				if (!dest_dxt)
					return false;

				if (!this->m.initMipmap(mipmap, inputOptions.m, w, h, d, f, m))
				{
					if (outputOptions.m.errorHandler != NULL)
					{
						outputOptions.m.errorHandler->error(Error_InvalidInput);
						return false;
					}
				}

				this->m.quantizeMipmap(mipmap, compressionOptions.m);
				nv::Image image (*mipmap.asFixedImage());
				compress(dest_dxt, isDxt1, image.width(), image.height(), &image, callback, dm);
				outputOptions.m.outputHandler->writeData(dest_dxt, size);

				free(dest_dxt);
				// Compute extents of next mipmap:
				w = max(1U, w / 2);
				h = max(1U, h / 2);
				d = max(1U, d / 2);
			}

		outputOptions.m.closeFile();
		return true;
	}
	else
		return Compressor::process(inputOptions, compressionOptions, outputOptions);
}

void readCallback(void* handle, unsigned char* bgra, int x, int y)
{
	// TODO checking x < width, y < height?

	nv::Image* image = (nv::Image*) handle;
	unsigned char i, j;
	uint xn = image->width() - x, yn = image->height() - y;
	xn = xn < 4 ? xn : 4;
	yn = yn < 4 ? yn : 4;
	for (i = 0; i < xn; ++i)
		for (j = 0; j < yn; ++j) {
			bgra[i*4+j*16] = image->pixel(x + i, y + j).b;
			bgra[i*4+j*16+1] = image->pixel(x + i, y + j).g;
			bgra[i*4+j*16+2] = image->pixel(x + i, y + j).r;
			bgra[i*4+j*16+3] = image->pixel(x + i, y + j).a;
		}
	if (xn < 4 || yn < 4) { 
		for (; i < 4; ++i)
			for (; j < 4; ++j) {
				bgra[i*4+j*16] = 0;
				bgra[i*4+j*16+1] = 0;
				bgra[i*4+j*16+2] = 0;
				bgra[i*4+j*16+3] = 0;
			}
		return;
	}
}
void readCallbackBc1(void* handle, unsigned char* bgra, int x, int y)
{
	nv::Image* image = (nv::Image*) handle;
	unsigned char i, j;
	uint xn = image->width() - x, yn = image->height() - y;
	xn = xn < 4 ? xn : 4;
	yn = yn < 4 ? yn : 4;
	for (i = 0; i < xn; ++i)
		for (j = 0; j < yn; ++j) {
			bgra[i*4+j*16] = image->pixel(x + i, y + j).b;
			bgra[i*4+j*16+1] = image->pixel(x + i, y + j).g;
			bgra[i*4+j*16+2] = image->pixel(x + i, y + j).r;
			bgra[i*4+j*16+3] = 255;
		}
	if (xn < 4 || yn < 4) { 
		for (; i < 4; ++i)
			for (; j < 4; ++j) {
				bgra[i*4+j*16] = 0;
				bgra[i*4+j*16+1] = 0;
				bgra[i*4+j*16+2] = 0;
				bgra[i*4+j*16+3] = 0;
			}
		return;
	}
}
void readCallbackBc1n(void* handle, unsigned char* bgra, int x, int y)
{
	nv::Image* image = (nv::Image*) handle;
	unsigned char i, j;
	uint xn = image->width() - x, yn = image->height() - y;
	xn = xn < 4 ? xn : 4;
	yn = yn < 4 ? yn : 4;
	for (i = 0; i < xn; ++i)
		for (j = 0; j < yn; ++j) {
			bgra[i*4+j*16] = 0;
			bgra[i*4+j*16+1] = image->pixel(x + i, y + j).g;
			bgra[i*4+j*16+2] = image->pixel(x + i, y + j).r;
			bgra[i*4+j*16+3] = 255;
		}
	if (xn < 4 || yn < 4) { 
		for (; i < 4; ++i)
			for (; j < 4; ++j) {
				bgra[i*4+j*16] = 0;
				bgra[i*4+j*16+1] = 0;
				bgra[i*4+j*16+2] = 0;
				bgra[i*4+j*16+3] = 0;
			}
		return;
	}
}
void readCallbackBc3n(void* handle, unsigned char* bgra, int x, int y)
{
	nv::Image* image = (nv::Image*) handle;
	unsigned char i, j;
	uint xn = image->width() - x, yn = image->height() - y;
	xn = xn < 4 ? xn : 4;
	yn = yn < 4 ? yn : 4;
	for (i = 0; i < xn; ++i)
		for (j = 0; j < yn; ++j) {
			bgra[i*4+j*16] = 0;
			bgra[i*4+j*16+1] = image->pixel(x + i, y + j).g;
			bgra[i*4+j*16+2] = 255;
			bgra[i*4+j*16+3] = image->pixel(x + i, y + j).r;
		}
	if (xn < 4 || yn < 4) { 
		for (; i < 4; ++i)
			for (; j < 4; ++j) {
				bgra[i*4+j*16] = 0;
				bgra[i*4+j*16+1] = 0;
				bgra[i*4+j*16+2] = 0;
				bgra[i*4+j*16+3] = 0;
			}
		return;
	}
}