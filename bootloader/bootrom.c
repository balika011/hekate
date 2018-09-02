/*
 * Copyright (c) 2018 balika011
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdlib.h>

#include "soc/t210.h"
#include "sec/se.h"
#include "sec/se_t210.h"
#include "soc/fuse.h"
#include "soc/pmc.h"
#include "storage/sdmmc.h"
//#include "storage/nx_emmc.h"

int pk1_entry;

enum boot_type
{
  BOOT_TYPE_COLD = 0x1,
  BOOT_TYPE_RECOVERY = 0x2,
  BOOT_TYPE_UART = 0x3,
  BOOT_TYPE_EXIT_RCM = 0x4,
};

typedef struct _BBT
{
  u32 numEntries;
  u8 virtualBlockSize;
  u8 blockSize;
  u8 badBlocks[512];
  u8 reserved[10];
} BBT;

typedef struct _KB
{
  u8 cmac[16];
  u8 ctr[16];
  u8 keys[9][16];
} KB;

typedef struct _BI
{
  u32 version;
  u32 start_block;
  u32 start_page;
  u32 length;
  u32 loadAddress;
  u32 entryPoint;
  u32 attribute;
  u8 cmacSignature[16];
  u8 rsaSignature[256];
} BI;

typedef struct _BCT
{
  BBT badBlockTable;
  u8 PKCModulus[256];
  u8 aesMacSignature[16];
  u8 rsaPssSignature[256];
  u32 secProvisioningKeyNumInsecure;
  u8 secProvisioningKey[32];
  u8 _0x444[12];
  KB keyblob;
  u8 _0x500[8];
  u32 odmData;
  u32 reserved0;
  u8 randomAesBlock[16];
  u8 uniqueChipId[16];
  u32 bootDataVersion;
  u32 blockSizeLog2;
  u32 pageSizeLog2;
  u32 partitionSize;
  u32 numParamSets;
  u32 devType;
  u32 devParams[16];
  u32 numSdramSets;
  u8 sdramParams[4][1896];
  u32 numBootloaders;
  BI bootloaders[4];
  u32 enableFailBack;
  u32 secureDebugControl;
  u32 secProvisioningKeyNumSecure;
  u8 reserved2[12];
  u8 padding[8];
} BCT;

typedef struct _BLI
{
  u32 active;
  u32 field_4;
  u32 field_8;
  u32 len;
  u32 signed_start;
  u32 signature;
} BLI;

typedef struct _BIT
{
  u8 gap0[12];
  enum boot_type boottype;
  u8 gap10[56];
  u32 bct_len;
  BCT *bct;
  BLI bli[4];
  u8 gapB0[80];
} BIT;

#define bit ((BIT *) 0x40000000)

void bootrom()
{
	// Bootrom part we skipped.
	u32 sbk[4] = { FUSE(0x1A4), FUSE(0x1A8), FUSE(0x1AC), FUSE(0x1B0) };
	se_aes_key_set(14, sbk, 0x10);
	
	// Lock SBK from being read.
	SE(SE_KEY_TABLE_ACCESS_REG_OFFSET + 14 * 4) = 0x7E;
	PMC(APBDEV_PMC_CRYPTO_OP) = 0;
	SE(SE_INT_STATUS_REG_OFFSET) = 0x1F;
	// Lock SSK (although it's not set and unused anyways).
	SE(SE_KEY_TABLE_ACCESS_REG_OFFSET + 15 * 4) = 0x7E;
	// Clear the boot reason to avoid problems later
	PMC(APBDEV_PMC_SCRATCH200) = 0x0;
	PMC(APBDEV_PMC_RST_STATUS) = 0x0;
	
	// This memset needs to happen here, else TZRAM will behave weirdly later on.
	memset((void *)0x7C010000, 0, 0x10000);
	
	FUSE(FUSE_PRIVATEKEYDISABLE) = 0x1;
	SYSREG(AHB_AHB_SPARE_REG) &= 0xFFFFFF9F;
	PMC(APBDEV_PMC_SCRATCH49) = ((PMC(APBDEV_PMC_SCRATCH49) >> 1) << 1) & 0xFFFFFFFD;
	
	memset((u8 *) 0x40000000, 0, 0x8000);
	//memset((u8 *) 0x40010000, 0, 0x3FFFF);
	
	sdmmc_storage_t storage;
	sdmmc_t sdmmc;

	sdmmc_storage_init_mmc(&storage, &sdmmc, SDMMC_4, SDMMC_BUS_WIDTH_8, 4);

	// Read BCT.
	sdmmc_storage_set_mmc_partition(&storage, 1);
	BCT *bct = (BCT *) 0x40000100;
	sdmmc_storage_read(&storage, 0, 0x3000 / 0x200, bct);
		
	// Read package1.
	u32 block_size = 1 << bct->blockSizeLog2;
	u32 pk1_block = bct->bootloaders[0].start_block;
	u32 pk1_len = bct->bootloaders[0].length;
	void *pkg1 = (void *) bct->bootloaders[0].loadAddress;
	pk1_entry = bct->bootloaders[0].entryPoint;
	
	sdmmc_storage_set_mmc_partition(&storage, 1);
	sdmmc_storage_read(&storage, (pk1_block * block_size) / 0x200, pk1_len / 0x200, pkg1);

	sdmmc_storage_end(&storage);
	
	bit->boottype = BOOT_TYPE_COLD;
	bit->bct_len = sizeof(BCT);
	bit->bct = bct;
	bit->bli[0].active = 1;
}
