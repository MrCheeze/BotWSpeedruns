#ifndef NPF_DEFS_H
#define	NPF_DEFS_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct AmiiboSettingsArgs AmiiboSettingsArgs;
	typedef struct NFCResult NFCResult;
	typedef struct NFPRomInfo NFPRomInfo;
	typedef struct NFPTagInfo NFPTagInfo;

	struct AmiiboSettingsArgs
	{
		u8 unk[0x5D];
	};
	static_assert(sizeof(AmiiboSettingsArgs) == 0x5D, "AmiiboSettingsArgs Size");

	struct NFPTagInfo
	{
		u8 serial[8];
		u8 unk1[24];
	};
	static_assert(sizeof(NFPTagInfo) == 32, "NFPTagInfo Size");

	struct NFPRomInfo
	{
		u16 gameID;
		u8 characterVariant;
		u8 figureType;
		u16 modelNumber;
		u16 series;
	};
	static_assert(sizeof(NFPRomInfo) == 0x8, "NFPRomInfo Size");

	struct NFCResult
	{
		int32_t description : 20;
		int32_t module : 9;
		int32_t level : 3;
	};

#ifdef __cplusplus
}
#endif

#endif	/* NFP_DEFS_H */

