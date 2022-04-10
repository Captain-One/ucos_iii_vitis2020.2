/*
*********************************************************************************************************
*                                             uCOS XSDK BSP
*
*                    Copyright 2014-2020 Silicon Laboratories Inc. www.silabs.com
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
*                           USB DEVICE MSC CLASS APPLICATION INITIALIZATION
*
* File : app_usbd_msc.c
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                              INCLUDE FILES
*********************************************************************************************************
*/

#include  <Class/MSC/usbd_msc.h>
#include  <Source/os.h>
#include  <ucos_bsp.h>


/*
*********************************************************************************************************
*                                              LOCAL DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                        LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         App_USBD_MSC_Init()
*
* Description : Initialize USB device mass storage class.
*
* Argument(s) : p_dev     Pointer to USB device.
*
*               cfg_hs      Index of high-speed configuration to which this interface will be added to.
*
*               cfg_fs      Index of high-speed configuration to which this interface will be added to.
*
* Return(s)   : DEF_OK,    if the Mass storage interface was added.
*               DEF_FAIL,  if the Mass storage interface could not be added.
*
* Caller(s)   : App_USBD_Init().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN   App_USBD_MSC_Init (CPU_INT08U  dev_nbr,
                                 CPU_INT08U  cfg_hs,
                                 CPU_INT08U  cfg_fs)
{
    USBD_ERR      err;
    CPU_INT08U    msc_nbr;
    CPU_BOOLEAN   valid;

    UCOS_Print("Initializing MSC class\r\n");

    USBD_MSC_Init(&err);
    if (err != USBD_ERR_NONE) {
    	UCOS_Printf("Error initializing MSC class. USBD_MSC_Init() returned error code %d\r\n", err);
        return (DEF_FAIL);
    }

    msc_nbr = USBD_MSC_Add(&err);

    if (cfg_hs != USBD_CFG_NBR_NONE) {
        valid = USBD_MSC_CfgAdd (msc_nbr,
                                 dev_nbr,
                                 cfg_hs,
                                 &err);

        if (valid != DEF_YES) {
        	UCOS_Printf("Error adding MSC instance. USBD_MSC_CfgAdd() returned error code %d\r\n", err);
            return (DEF_FAIL);
        }
    }

    if (cfg_fs != USBD_CFG_NBR_NONE) {
        valid = USBD_MSC_CfgAdd (msc_nbr,
                                dev_nbr,
                                cfg_fs,
                &err);

        if (valid != DEF_YES) {
        	UCOS_Printf("Error adding MSC instance. USBD_MSC_CfgAdd() returned error code %d\r\n", err);
            return (DEF_FAIL);
        }
    }
                                                                /* Add Logical Unit to MSC interface.                   */
    USBD_MSC_LunAdd((void *)"sdcard:0:",
                             msc_nbr,
                    (void *)"Micrium",
                    (void *)"SD Card",
                             0x00,
                             DEF_FALSE,
                            &err);
    if (err != USBD_ERR_NONE) {
    	UCOS_Printf("Error adding LU to MSC class. USBD_MSC_LunAdd() returned with error %d\r\n", err);
        return (DEF_FAIL);
    }

    return (DEF_OK);
}

