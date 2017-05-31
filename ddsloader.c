#include "ddsloader.h"
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <GL/glew.h>

/*#ifdef _WIN32
#include <ddraw.h>
#else*/
// Structures from Direct3D 9
typedef struct _DDPIXELFORMAT {
	int dwSize;
	int dwFlags;
	int dwFourCC;
	int dwRGBBitCount;
	int dwRBitMask, dwGBitMask, dwBBitMask;
	int dwRGBAlphaBitMask;
} DDPIXELFORMAT;

typedef struct _DDSCAPS2
{
	int dwCaps1;
	int dwCaps2;
	int Reserved[2];
} DDSCAPS2;

typedef struct _DDSURFACEDESC2
{
	int dwSize;
	int dwFlags;
	int dwHeight;
	int dwWidth;
	int dwPitchOrLinearSize;
	int dwDepth;
	int dwMipMapCount;
	int dwReserved1[11];
	DDPIXELFORMAT ddpfPixelFormat;
	DDSCAPS2 ddsCaps;
	int dwReserved2;
} DDSURFACEDESC2;

#define MAKEFOURCC(a, b, c, d) ((unsigned int)(a) | (unsigned int)(b) << 8 | (unsigned int)(c) << 16 | (unsigned int)(d) << 24)

#define DDSD_CAPS 0x00000001
#define DDSD_HEIGHT 0x00000002
#define DDSD_WIDTH 0x00000004
#define DDSD_PITCH 0x00000008
#define DDSD_PIXELFORMAT 0x00001000
#define DDSD_MIPMAPCOUNT 0x00020000
#define DDSD_LINEARSIZE 0x00080000
#define DDSD_DEPTH 0x00800000

#define DDPF_ALPHAPIXELS 0x00000001
#define DDPF_ALPHA 0x00000002
#define DDPF_FOURCC 0x00000004
#define DDPF_PALETTEINDEXED8 0x00000020
#define DDPF_RGB 0x00000040
#define DDPF_LUMINANCE 0x00020000

#define DDSCAPS_COMPLEX 0x00000008
#define DDSCAPS_TEXTURE 0x00001000
#define DDSCAPS_MIPMAP 0x00400000

#define DDSCAPS2_CUBEMAP 0x00000200
#define DDSCAPS2_CUBEMAP_POSITIVEX 0x00000400
#define DDSCAPS2_CUBEMAP_NEGATIVEX 0x00000800
#define DDSCAPS2_CUBEMAP_POSITIVEY 0x00001000
#define DDSCAPS2_CUBEMAP_NEGATIVEY 0x00002000
#define DDSCAPS2_CUBEMAP_POSITIVEZ 0x00004000
#define DDSCAPS2_CUBEMAP_NEGATIVEZ 0x00008000
#define DDSCAPS2_CUBEMAP_ALLFACES (DDSCAPS2_CUBEMAP_POSITIVEX | DDSCAPS2_CUBEMAP_NEGATIVEX | DDSCAPS2_CUBEMAP_POSITIVEY | DDSCAPS2_CUBEMAP_NEGATIVEY | DDSCAPS2_CUBEMAP_POSITIVEZ | DDSCAPS2_CUBEMAP_NEGATIVEZ)
#define DDSCAPS2_VOLUME 0x00200000
// #endif

#define DDS_MAGIC 0x20534444 // "DDS "
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MAX_LEVELS (1 + log2(GL_MAX_TEXTURE_SIZE))
#define ISBITMASK(r, g, b, a) (ddpf.dwRBitMask == r && ddpf.dwGBitMask == g && ddpf.dwBBitMask == b && ddpf.dwRGBAlphaBitMask == a)

static GLenum getFormat(const DDPIXELFORMAT ddpf, int *bitsPerPixel, int *compressed) {
	*compressed = 0;
	if (ddpf.dwFlags & DDPF_RGB) {
		switch (*bitsPerPixel = ddpf.dwRGBBitCount) {
		case 32:
			if (ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000)) return GL_RGBA8;
			if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000)) return GL_BGRA;

			// Note that many common DDS reader/writers (including D3DX) swap the the RED/BLUE masks for 10:10:10:2 formats. We assumme below that the 'backwards' header mask is being used since it is most likely written by D3DX. The more robust solution is to use the 'DX10' header extension and specify the DXGI_FORMAT_R10G10B10A2_UNORM format directly

			// For 'correct' writers, this should be 0x000003ff, 0x000ffc00, 0x3ff00000 for RGB data
			if (ISBITMASK(0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000)) return GL_RGB10_A2;
			if (ISBITMASK(0x0000ffff, 0xffff0000, 0x00000000, 0x00000000)) return GL_RG16;
			if (ISBITMASK(0xffffffff, 0x00000000, 0x00000000, 0x00000000)) return GL_R32F;
			break;
		case 24:
			return GL_RGB8;
		}
	}
	else if (ddpf.dwFlags & DDPF_FOURCC) {
		// int dxt = (ddpf.dwFourCC >> 24) - '0';
		if (MAKEFOURCC('D', 'X', 'T', '1') == ddpf.dwFourCC) {
			*compressed = 1;
			return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		}
		if (MAKEFOURCC('D', 'X', 'T', '3') == ddpf.dwFourCC) {
			*compressed = 1;
			return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		}
		if (MAKEFOURCC('D', 'X', 'T', '5') == ddpf.dwFourCC) {
			*compressed = 1;
			return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		}
		// Check for D3DFORMAT enums being set here
		switch (ddpf.dwFourCC) {
		case 36: return GL_RGBA16; // D3DFMT_A16B16G16R16
		case 110: return GL_RGBA16_SNORM; // D3DFMT_Q16W16V16U16
		case 111: return GL_R16F; // D3DFMT_R16F
		case 112: return GL_RG16F; // D3DFMT_G16R16F
		case 113: return GL_RGBA16F; // D3DFMT_A16B16G16R16F
		case 114: return GL_R32F; // D3DFMT_R32F
		case 115: return GL_RG32F; // D3DFMT_G32R32F
		case 116: return GL_RGBA32F; // D3DFMT_A32B32G32R32F
		}
	}

	return 0;
}

// From http://src.chromium.org/viewvc/chrome/trunk/src/o3d/core/cross/bitmap_dds.cc?view=markup&pathrev=21227

/** Flips a full DXT1 block in the y direction. */
static void FlipDXT1BlockFull(unsigned char *block) {
	// A DXT1 block layout is:
	// [0-1] color0.
	// [2-3] color1.
	// [4-7] color bitmap, 2 bits per pixel.
	// So each of the 4-7 bytes represents one line, flipping a block is just
	// flipping those bytes.
	unsigned char tmp = block[4];
	block[4] = block[7];
	block[7] = tmp;
	tmp = block[5];
	block[5] = block[6];
	block[6] = tmp;
}

/** Flips a full DXT3 block in the y direction. */
static void FlipDXT3BlockFull(unsigned char *block) {
	// A DXT3 block layout is:
	// [0-7]  alpha bitmap, 4 bits per pixel.
	// [8-15] a DXT1 block.

	// We can flip the alpha bits at the byte level (2 bytes per line).
	unsigned char tmp = block[0];
	block[0] = block[6];
	block[6] = tmp;
	tmp = block[1];
	block[1] = block[7];
	block[7] = tmp;
	tmp = block[2];
	block[2] = block[4];
	block[4] = tmp;
	tmp = block[3];
	block[3] = block[5];
	block[5] = tmp;

	FlipDXT1BlockFull(block + 8); // And flip the DXT1 block using the above function.
}

/** Flips a full DXT5 block in the y direction. */
static void FlipDXT5BlockFull(unsigned char *block) {
	// A DXT5 block layout is:
	// [0]    alpha0.
	// [1]    alpha1.
	// [2-7]  alpha bitmap, 3 bits per pixel.
	// [8-15] a DXT1 block.

	// The alpha bitmap doesn't easily map lines to bytes, so we have to
	// interpret it correctly.  Extracted from
	// http://www.opengl.org/registry/specs/EXT/texture_compression_s3tc.txt :
	//
	//   The 6 "bits" bytes of the block are decoded into one 48-bit integer:
	//
	//     bits = bits_0 + 256 * (bits_1 + 256 * (bits_2 + 256 * (bits_3 +
	//                   256 * (bits_4 + 256 * bits_5))))
	//
	//   bits is a 48-bit unsigned integer, from which a three-bit control code
	//   is extracted for a texel at location (x,y) in the block using:
	//
	//       code(x,y) = bits[3*(4*y+x)+1..3*(4*y+x)+0]
	//
	//   where bit 47 is the most significant and bit 0 is the least
	//   significant bit.

	// From Chromium (source was buggy)
	unsigned int line_0_1 = block[2] + 256 * (block[3] + 256 * block[4]);
	unsigned int line_2_3 = block[5] + 256 * (block[6] + 256 * block[7]);
	// swap lines 0 and 1 in line_0_1.
	unsigned int line_1_0 = ((line_0_1 & 0x000fff) << 12) |
		((line_0_1 & 0xfff000) >> 12);
	// swap lines 2 and 3 in line_2_3.
	unsigned int line_3_2 = ((line_2_3 & 0x000fff) << 12) |
		((line_2_3 & 0xfff000) >> 12);
	block[2] = line_3_2 & 0xff;
	block[3] = (line_3_2 & 0xff00) >> 8;
	block[4] = (line_3_2 & 0xff0000) >> 16;
	block[5] = line_1_0 & 0xff;
	block[6] = (line_1_0 & 0xff00) >> 8;
	block[7] = (line_1_0 & 0xff0000) >> 16;

	FlipDXT1BlockFull(block + 8); // And flip the DXT1 block using the above function.
}

GLuint dds_load_texture_from_memory(const char *data, int *imageWidth, int *imageHeight, int flags) {
	// Validate size of DDS file in memory
	// if (dataSize < sizeof(uint32_t) + sizeof(DDSURFACEDESC2)) return 0;

	// Verify the type
	if (*(const uint32_t *) data != DDS_MAGIC) return 0;

	const DDSURFACEDESC2 header = *(const DDSURFACEDESC2 *) (data + sizeof(uint32_t));
	if (header.dwSize != sizeof(DDSURFACEDESC2) ||
		header.ddpfPixelFormat.dwSize != sizeof(DDPIXELFORMAT)) return 0;

	ptrdiff_t offset = sizeof(uint32_t) + sizeof(DDSURFACEDESC2);

	size_t width = header.dwWidth, height = header.dwHeight, depth = header.dwDepth, mipMapCount = MIN(header.dwMipMapCount, MAX_LEVELS), arraySize = 1;
	if (mipMapCount == 0) mipMapCount = 1;
	if (imageWidth != 0) *imageWidth = width;
	if (imageHeight != 0) *imageHeight = height;

	int bitsPerPixel, compressed, isCubeMap = 0;
	GLenum internalformat = getFormat(header.ddpfPixelFormat, &bitsPerPixel, &compressed);

	GLenum target;
	if (header.dwFlags & DDSD_DEPTH) {
		target = GL_TEXTURE_3D;
	} else {
		if (header.ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP) {
			// We require all six faces to be defined
			if ((header.ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_ALLFACES) != DDSCAPS2_CUBEMAP_ALLFACES) return 0;
			isCubeMap = 1;
			target = GL_TEXTURE_CUBE_MAP;
		} else target = GL_TEXTURE_2D;
		depth = 1;
		// Note there's no way for a legacy Direct3D 9 DDS to express a '1D' texture
	}

	int blockSize = internalformat == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ? 8 : 16;
	// glGetInternalformativ(target, internalformat, GL_TEXTURE_COMPRESSED_BLOCK_SIZE, 1, &blockSize);

	GLuint texture;
	glGenTextures(1, &texture);
	if (!texture) return 0;
	glBindTexture(target, texture);

	// Not all DDS files provide all mipmap levels; define mipmap range
	glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, mipMapCount - 1);

	for (int i = 0; i < mipMapCount; ++i) {
		for (int j = 0; j < (isCubeMap ? 6 : 1); ++j) {
			size_t size = compressed
				? ((width + 3) / 4) * ((height + 3) / 4) * blockSize
				: width * height * bitsPerPixel / 8;
			if (isCubeMap) target = GL_TEXTURE_CUBE_MAP_POSITIVE_X_EXT + j;

			if (!compressed) {
				glTexImage2D(target, i, internalformat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data + offset);
			} else {
				unsigned char *pixels = 0;
				if (flags & DDS_FLIP_UVS) {
					pixels = malloc(size);
					// Flip & copy to actual pixel buffer
					int widBytes = ((width + 3) / 4) * blockSize;
					const unsigned char *s = data + offset;
					unsigned char *d = pixels + ((height + 3) / 4 - 1) * widBytes;
					for (unsigned int j = 0; j < (height + 3) / 4; j++) {
						memcpy(d, s, widBytes);

						for (int k = 0; k < widBytes / blockSize; k++) if (internalformat == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) FlipDXT1BlockFull(d + k * blockSize);
						else if (internalformat == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT) FlipDXT3BlockFull(d + k * blockSize);
						else if (internalformat == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) FlipDXT5BlockFull(d + k * blockSize);

						s += widBytes;
						d -= widBytes;
					}
				}

				glCompressedTexImage2D(target, i, internalformat, width, height, 0, size, pixels ? pixels : (unsigned char *) data + offset); // data + offset
				free(pixels);
				GLint param = 0;
				// glGetTexLevelParameteriv(target, i, GL_TEXTURE_COMPRESSED_ARB, &param);
				// if (param == 0) printf("Mipmap level %d indicated compression failed", i);
			}

			offset += size;
		}

		width /= 2;
		height /= 2;
	}

	return texture;
}
