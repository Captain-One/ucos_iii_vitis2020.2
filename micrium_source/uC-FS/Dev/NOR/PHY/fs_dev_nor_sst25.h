/*
*********************************************************************************************************
*                                                uC/FS
*                                      The Embedded File System
*
*                    Copyright 2008-2021 Silicon Laboratories Inc. www.silabs.com
*
*                                 SPDX-License-Identifier: APACHE-2.0
*
*               This software is subject to an open source license and is distributed by
*                Silicon Laboratories Inc. pursuant to the terms of the Apache License,
*                    Version 2.0 available at www.apache.org/licenses/LICENSE-2.0.
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                      FILE SYSTEM DEVICE DRIVER
*
*                                          NOR FLASH DEVICES
*                             SST SST25 SERIAL NOR PHYSICAL-LAYER DRIVER
*
* Filename : fs_dev_nor_sst25.h
* Version  : V4.08.01
*********************************************************************************************************
* Note(s)  : (1) Supports Numonyx/ST's M29 parallel NOR flash memories, as described in various
*                datasheets at Numonyx (http://www.numonyx.com).  This driver has been tested with
*                or should work with the following devices :
*
*                    M29W320EB               M29W640GH  [+]
*                    M29W320ET [=]           M29W640GL  [+]
*                    M29W064FB               M29W128FH  [+]
*                    M29W064FT [=]           M29W128FL  [+]
*                    M29W640FB               M29W128GH  [+]
*                    M29W640FT [=] [*]       M29W128GL  [+]     [*]
*                                            M29W640GB  [+]
*                                            M29W640GT  [+] [=]
*
*                          [*} Devices tested
*                          [+] These devices will be accessed more efficiently with the generic AMD
*                              1x16 driver.
*                          [=] These devices are top boot-block devices, which have several small
*                              blocks at the top of the memory.  However, the CFI device geometry
*                              lists the boot block region before the regular block region.  To
*                         reverse the block regions logically, define NOR_REVERSE_CFI in fs_cfg.h.
*
*            (2) Fast programming command "double word program", supported by these flash devices,
*                is used in this driver.  For other operations, the standard AMD command set is used.
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  FS_DEV_NOR_SST25_PRESENT
#define  FS_DEV_NOR_SST25_PRESENT


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/

#ifdef   FS_DEV_NOR_SST25_MODULE
#define  FS_DEV_NOR_SST25_EXT
#else
#define  FS_DEV_NOR_SST25_EXT  extern
#endif


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../../Source/fs_dev.h"
#include  "../fs_dev_nor.h"


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/

extern  const  FS_DEV_NOR_PHY_API  FSDev_NOR_SST25;


/*
*********************************************************************************************************
*                                               MACRO'S
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

CPU_BOOLEAN  FSDev_NOR_BSP_SPI_WaitWhileBusy (FS_QTY                 unit_nbr,
                                              FS_DEV_NOR_PHY_DATA   *p_phy_data,
                                              CPU_BOOLEAN          (*poll_fnct)(FS_DEV_NOR_PHY_DATA  *p_phy_data_arg),
                                              CPU_INT32U             to_us);

/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif                                                          /* End of NOR SST25 module include.                     */
