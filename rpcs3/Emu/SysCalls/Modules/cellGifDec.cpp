#include "stdafx.h"
#include "Emu/SysCalls/SysCalls.h"
#include "Emu/SysCalls/SC_FUNC.h"

#include "stblib/stb_image.h"
#include "stblib/stb_image.c" // (TODO: Should we put this elsewhere?)

void cellGifDec_init();
Module cellGifDec(0xf010, cellGifDec_init);

//Return Codes
enum
{
	CELL_GIFDEC_ERROR_OPEN_FILE		= 0x80611300,
	CELL_GIFDEC_ERROR_STREAM_FORMAT = 0x80611301,
	CELL_GIFDEC_ERROR_SEQ			= 0x80611302,
	CELL_GIFDEC_ERROR_ARG			= 0x80611303,
	CELL_GIFDEC_ERROR_FATAL			= 0x80611304,
	CELL_GIFDEC_ERROR_SPU_UNSUPPORT = 0x80611305,
	CELL_GIFDEC_ERROR_SPU_ERROR		= 0x80611306,
	CELL_GIFDEC_ERROR_CB_PARAM		= 0x80611307,
};

struct CellGifDecInfo
{
    u32 SWidth;
    u32 SHeight;
    u32 SGlobalColorTableFlag;
    u32 SColorResolution;
    u32 SSortFlag;
	u32 SSizeOfGlobalColorTable;
	u32 SBackGroundColor;
	u32 SPixelAspectRatio;
};

struct CellGifDecSrc
{
    u32 srcSelect;			// CellGifDecStreamSrcSel
    u32 fileName;			// const char*
    u64 fileOffset;			// int64_t
    u32 fileSize;
    u32 streamPtr;
    u32 streamSize;
    u32 spuThreadEnable;	// CellGifDecSpuThreadEna
};

CellGifDecInfo	current_info;
CellGifDecSrc	current_src;


int cellGifDecCreate(u32 mainHandle, u32 threadInParam, u32 threadOutParam)
{
	UNIMPLEMENTED_FUNC(cellGifDec);
	return CELL_OK;
}

int cellGifDecExtCreate(u32 mainHandle, u32 threadInParam, u32 threadOutParam, u32 extThreadInParam, u32 extThreadOutParam)
{
	UNIMPLEMENTED_FUNC(cellGifDec);
	return CELL_OK;
}

int cellGifDecOpen(u32 mainHandle, u32 subHandle_addr, u32 src_addr, u32 openInfo)
{
	//current_src.srcSelect       = Memory.Read32(src_addr);
	current_src.fileName		  = Memory.Read32(src_addr+4);
	//current_src.fileOffset      = Memory.Read32(src_addr+8);
	//current_src.fileSize        = Memory.Read32(src_addr+12);
	//current_src.streamPtr       = Memory.Read32(src_addr+16);
	//current_src.streamSize      = Memory.Read32(src_addr+20);
	//current_src.spuThreadEnable = Memory.Read32(src_addr+24);

	u32& fd_addr = subHandle_addr;						// Set file descriptor as sub handler of the decoder
	int ret = cellFsOpen(current_src.fileName, 0, fd_addr, NULL, 0);
	if(ret != 0) return CELL_GIFDEC_ERROR_OPEN_FILE;

	return CELL_OK;
}

int cellGifDecReadHeader(u32 mainHandle, u32 subHandle, u32 info_addr)
{
	u32& fd = subHandle;

	//Check size of file
	u32 sb_addr = Memory.Alloc(52,1);					// Alloc a CellFsStat struct
	cellFsFstat(fd, sb_addr);
	u64 fileSize = Memory.Read64(sb_addr+36);			// Get CellFsStat.st_size
	Memory.Free(sb_addr);
	
	//Write the header to buffer
	u32 buffer = Memory.Alloc(13,1);					// Alloc buffer for GIF header
	u32 pos_addr = Memory.Alloc(8,1);
	cellFsLseek(fd, 0, 0, pos_addr);
	cellFsRead(fd, buffer, 13, NULL);
	Memory.Free(pos_addr);

	if (Memory.Read32(buffer) != 0x47494638 ||
		(Memory.Read16(buffer+4) != 0x3961 &&
		Memory.Read16(buffer+4) != 0x3761))				// Error: The first 6 bytes are not a valid GIF signature
	{
		Memory.Free(buffer);
		return CELL_GIFDEC_ERROR_STREAM_FORMAT;			// Surprisingly there is no error code related with headerss
	}

	u8 packedField = Memory.Read8(buffer+10);
	current_info.SWidth						= Memory.Read8(buffer+6) + Memory.Read8(buffer+7) * 256;
	current_info.SHeight					= Memory.Read8(buffer+8) + Memory.Read8(buffer+9) * 256;
	current_info.SGlobalColorTableFlag		= packedField >> 7;
	current_info.SColorResolution			= ((packedField >> 4) & 7)+1;
	current_info.SSortFlag					= (packedField >> 3) & 1;
	current_info.SSizeOfGlobalColorTable	= (packedField & 7)+1;
	current_info.SBackGroundColor			= Memory.Read8(buffer+11);
	current_info.SPixelAspectRatio			= Memory.Read8(buffer+12);

	mem_class_t info(info_addr);
	info += current_info.SWidth;
	info += current_info.SHeight;
	info += current_info.SGlobalColorTableFlag;
	info += current_info.SColorResolution;
	info += current_info.SSortFlag;
	info += current_info.SSizeOfGlobalColorTable;
	info += current_info.SBackGroundColor;
	info += current_info.SPixelAspectRatio;
	Memory.Free(buffer);
	
	return CELL_OK;
}

int cellGifDecSetParameter(u32 mainHandle, u32 subHandle, u32 inParam, u32 outParam)
{
	UNIMPLEMENTED_FUNC(cellGifDec);
	return CELL_OK;
}

int cellGifDecDecodeData(u32 mainHandle, u32 subHandle, u32 data_addr, u32 dataCtrlParam_addr, u32 dataOutInfo_addr)
{
	u32& fd = subHandle;

	//Get size of file
	u32 sb_addr = Memory.Alloc(52,1);					// Alloc a CellFsStat struct
	cellFsFstat(fd, sb_addr);
	u64 fileSize = Memory.Read64(sb_addr+36);			// Get CellFsStat.st_size
	Memory.Free(sb_addr);

	//Copy the GIF file to a buffer
	u32 buffer = Memory.Alloc(fileSize,1);
	u32 pos_addr = Memory.Alloc(8,1);
	cellFsLseek(fd, 0, 0, pos_addr);
	cellFsRead(fd, buffer, fileSize, NULL);
	Memory.Free(pos_addr);

	//Decode GIF file. (TODO: Is there any faster alternative? Can we do it without external libraries?)
	int width, height, actual_components;
	unsigned char *gif =  new unsigned char [fileSize];
	for(u32 i = 0; i < fileSize; i++){
		gif[i] = Memory.Read8(buffer+i);
	}

	unsigned char *image = stbi_load_from_memory((const unsigned char*)gif, fileSize, &width, &height, &actual_components, 4);
	if (!image)
	{
		Memory.Free(buffer);
		return CELL_GIFDEC_ERROR_STREAM_FORMAT; 
	}
	u32 image_size = width * height * 4;
	for(u32 i = 0; i < image_size; i+=4){
		Memory.Write8(data_addr+i+0, image[i+3]);
		Memory.Write8(data_addr+i+1, image[i+0]);
		Memory.Write8(data_addr+i+2, image[i+1]);
		Memory.Write8(data_addr+i+3, image[i+2]);
	}
	Memory.Free(buffer);

	//The output data is an image (dataOutInfo.recordType = 1)
	Memory.Write32(dataOutInfo_addr, 1);

	return CELL_OK;
}

int cellGifDecClose(u32 mainHandle, u32 subHandle)
{
	u32& fd = subHandle;
	cellFsClose(fd);

	return CELL_OK;
}

int cellGifDecDestroy(u32 mainHandle)
{
	UNIMPLEMENTED_FUNC(cellGifDec);
	return CELL_OK;
}

void cellGifDec_init()
{
	cellGifDec.AddFunc(0xb60d42a5, cellGifDecCreate);
	cellGifDec.AddFunc(0x4711cb7f, cellGifDecExtCreate);
	cellGifDec.AddFunc(0x75745079, cellGifDecOpen);
	cellGifDec.AddFunc(0xf0da95de, cellGifDecReadHeader);
	cellGifDec.AddFunc(0x41a90dc4, cellGifDecSetParameter);
	cellGifDec.AddFunc(0x44b1bc61, cellGifDecDecodeData);
	cellGifDec.AddFunc(0x116a7da9, cellGifDecClose);
	cellGifDec.AddFunc(0xe74b2cb1, cellGifDecDestroy);
	
	/*cellGifDec.AddFunc(0x17fb83c1, cellGifDecExtOpen);
	cellGifDec.AddFunc(0xe53f91f2, cellGifDecExtReadHeader);
	cellGifDec.AddFunc(0x95cae771, cellGifDecExtSetParameter);
	cellGifDec.AddFunc(0x02e7e03e, cellGifDecExtDecodeData);*/
}