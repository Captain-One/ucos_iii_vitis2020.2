/*
*********************************************************************************************************
*                                             uC/USB-Host
*                                     The Embedded USB Host Stack
*
*                    Copyright 2004-2021 Silicon Laboratories Inc. www.silabs.com
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
*                                      MASS STORAGE CLASS (MSC)
*
* Filename : usbh_msc.h
* Version  : V3.42.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  USBH_MSC_MODULE_PRESENT
#define  USBH_MSC_MODULE_PRESENT


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../Source/usbh_os.h"
#include  "../../Source/usbh_class.h"


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/

#ifdef   USBH_MSC_MODULE
#define  USBH_MSC_EXT
#else
#define  USBH_MSC_EXT  extern
#endif


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

#define  USBH_MSC_TIMEOUT                              10000u

#define  USBH_MSC_DEV_NOT_IN_USE                           0u
#define  USBH_MSC_DEV_IN_USE                               1u

#define  USBH_MSC_DATA_DIR_IN                           0x80u
#define  USBH_MSC_DATA_DIR_OUT                          0x00u
#define  USBH_MSC_DATA_DIR_NONE                         0x01u


/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/

typedef  CPU_INT08U  USBH_MSC_DATA_DIR;

                                                                /* -------------------- MSC DEVICE -------------------- */
typedef  struct  usbh_msc_dev {
    USBH_EP       BulkInEP;                                     /* Bulk IN  endpoint.                                   */
    USBH_EP       BulkOutEP;                                    /* Bulk OUT endpoint.                                   */
    USBH_DEV     *DevPtr;                                       /* Pointer to USB device.                               */
    USBH_IF      *IF_Ptr;                                       /* Pointer to interface.                                */
    CPU_INT08U    State;                                        /* State of MSC device.                                 */
    CPU_INT08U    RefCnt;                                       /* Cnt of app ref on this dev.                          */
    USBH_HMUTEX   HMutex;
} USBH_MSC_DEV;

typedef  struct  msc_inquiry_info {
    CPU_INT08U  DevType;
    CPU_INT08U  IsRemovable;
    CPU_INT08U  Vendor_ID[8];
    CPU_INT08U  Product_ID[16];
    CPU_INT32U  ProductRevisionLevel;
} USBH_MSC_INQUIRY_INFO;


/*
*********************************************************************************************************
*                                            GLOBAL VARIABLES
*********************************************************************************************************
*/

USBH_MSC_EXT  USBH_CLASS_DRV  USBH_MSC_ClassDrv;


/*
*********************************************************************************************************
*                                                 MACROS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           FUNCTION PROTOTYPES
*********************************************************************************************************
*/

USBH_ERR    USBH_MSC_Init        (USBH_MSC_DEV           *p_msc_dev,
                                  CPU_INT08U              lun);

CPU_INT08U  USBH_MSC_MaxLUN_Get  (USBH_MSC_DEV           *p_msc_dev,
                                  USBH_ERR               *p_err);

CPU_BOOLEAN  USBH_MSC_UnitRdyTest(USBH_MSC_DEV          *p_msc_dev,
                                  CPU_INT08U             lun,
                                  USBH_ERR              *p_err);

USBH_ERR    USBH_MSC_CapacityRd  (USBH_MSC_DEV           *p_msc_dev,
                                  CPU_INT08U              lun,
                                  CPU_INT32U             *p_nbr_blks,
                                  CPU_INT32U             *p_blk_size);

USBH_ERR    USBH_MSC_StdInquiry  (USBH_MSC_DEV           *p_msc_dev,
                                  USBH_MSC_INQUIRY_INFO  *p_msc_inquiry_info,
                                  CPU_INT08U              lun);

USBH_ERR    USBH_MSC_RefAdd      (USBH_MSC_DEV           *p_msc_dev);

USBH_ERR    USBH_MSC_RefRel      (USBH_MSC_DEV           *p_msc_dev);

CPU_INT32U  USBH_MSC_Rd          (USBH_MSC_DEV           *p_msc_dev,
                                  CPU_INT08U              lun,
                                  CPU_INT32U              blk_addr,
                                  CPU_INT16U              nbr_blks,
                                  CPU_INT32U              blk_size,
                                  void                   *p_arg,
                                  USBH_ERR               *p_err);

CPU_INT32U  USBH_MSC_Wr          (USBH_MSC_DEV           *p_msc_dev,
                                  CPU_INT08U              lun,
                                  CPU_INT32U              blk_addr,
                                  CPU_INT16U              nbr_blks,
                                  CPU_INT32U              blk_size,
                                  const  void            *p_arg,
                                  USBH_ERR               *p_err);


/*
*********************************************************************************************************
*                                          CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               MODULE END
*********************************************************************************************************
*/

#endif
