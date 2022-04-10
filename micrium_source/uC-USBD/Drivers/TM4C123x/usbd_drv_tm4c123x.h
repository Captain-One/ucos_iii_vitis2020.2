/*
*********************************************************************************************************
*                                            uC/USB-Device
*                                    The Embedded USB Device Stack
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
*                                          USB DEVICE DRIVER
*
*                              Texas Instruments Tiva C Series USB-OTG
*
* Filename : usbd_drv_tm4c123x.h
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)  : (1) You can find specific information about this driver at:
*                https://doc.micrium.com/display/USBDDRV/TM4C123x
*
*            (2) With an appropriate BSP, this device driver will support the USB-OTG device module
*                on the entire Texas Instruments Tiva C Series.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) This USB device driver function header file is protected from multiple pre-processor
*               inclusion through use of the USB device driver module present pre-processor macro
*               definition.
*********************************************************************************************************
*/

#ifndef  USBD_DRV_TM4C123X_MODULE_PRESENT                       /* See Note #1.                                         */
#define  USBD_DRV_TM4C123X_MODULE_PRESENT

/*
*********************************************************************************************************
*                                          USB DEVICE DRIVER
*********************************************************************************************************
*/

extern  USBD_DRV_API  USBD_DrvAPI_TM4C123X;
extern  USBD_DRV_API  USBD_DrvAPI_MSP432E;


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif