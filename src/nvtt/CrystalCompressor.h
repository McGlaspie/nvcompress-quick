#include <nvtt/nvtt.h>

#include <windows.h> // LoadLibrary
#include <nvimage/Image.h>
struct CrystalCompressor: public nvtt::Compressor
{
	NVTT_API CrystalCompressor();

	NVTT_API bool process(const nvtt::InputOptions & inputOptions, const nvtt::CompressionOptions & compressionOptions, const nvtt::OutputOptions & outputOptions) const;
	bool nocrystal;
	bool datamode;
	bool bc1n;
private:
	HINSTANCE crystallLib;
};

// definitions for CrystallDXT lib (CL)
typedef void(*CLDxtCallback)(void* handle, unsigned char* bgra, int x, int y);
typedef void (*CLCompressFunc)(void*,bool,int,int,void*,CLDxtCallback,bool);
