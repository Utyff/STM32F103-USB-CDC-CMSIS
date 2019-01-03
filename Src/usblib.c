/* 
 * This file is part of the SaeWave RemoteSwitch (USB-CDC-CMSIS) 
 * distribution (https://github.com/saewave/STM32F103-USB-CDC-CMSIS).
 * Copyright (c) 2017 Samoilov Alexey.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "stm32l0xx.h"
#include "usblib.h"


#define __SECTION_PMA __attribute__((section(".PMA"))) /* USB PMA */
#define SIZE_OF_BTABLE 64

volatile USB_TypeDef_ *USB_ = (USB_TypeDef_ *)USB_BASE;
__SECTION_PMA volatile USBLIB_EPBuf EPBufTable[EPCOUNT];
#define USBEP ((volatile uint32_t *)USB_BASE)
USBLIB_SetupPacket   *SetupPacket;
volatile uint8_t      DeviceAddress = 0;
volatile USBLIB_WByte LineState;

uint8_t rxBuf0[64];
uint8_t rxBuf1[16];
uint8_t rxBuf2[64];

USBLIB_EPData EpData[EPCOUNT] = {
        {0, EP_CONTROL,   64, 64, 0, 0, (uint16_t *) rxBuf0, 0, 1},
        {1, EP_INTERRUPT, 16, 16, 0, 0, (uint16_t *) rxBuf1, 0, 1},
        {2, EP_BULK,      64, 64, 0, 0, 0,                   0, 1},  // IN  (Device -> Host)
        {3, EP_BULK,      64, 64, 0, 0, (uint16_t *) rxBuf2, 0, 1}}; // OUT (Host   -> Device)

void USBLIB_Init(void)
{
    NVIC_DisableIRQ(USB_IRQn);
    // disable D+ Pull-up
    USB->BCDR &= ~USB_BCDR_DPPU;
    RCC->APB1ENR |= RCC_APB1ENR_USBEN;

    USB->CNTR   = USB_CNTR_FRES; /* Force USB Reset */
    USB->BTABLE = 0;
    USB->DADDR  = 0;
    USB->ISTR   = 0;
    USB->CNTR   = USB_CNTR_RESETM;
    NVIC_EnableIRQ(USB_IRQn);
    // enable D+ Pull-up
    USB->BCDR |= USB_BCDR_DPPU;

    // ===== DMA for copy data to/from PMA
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;      // enable DMA clock
    DMA1_Channel4->CCR &= ~DMA_CCR_DIR;    // direction from peripheral to memory
    DMA1_Channel4->CCR |= DMA_CCR_MEM2MEM; // enable MEM2MEM mode
    DMA1_Channel4->CCR |= DMA_CCR_PINC;    // enable peripheral address increment
    DMA1_Channel4->CCR |= DMA_CCR_MINC;    // enable memory address increment
    DMA1_Channel4->CCR |= DMA_CCR_PSIZE_0; // peripheral wide - 16 бит
    DMA1_Channel4->CCR |= DMA_CCR_MSIZE_0; // memory wide - 16 бит
    DMA1_Channel4->CCR |= DMA_CCR_PL;      // priority - very high
    DMA1_Channel4->CCR &= ~DMA_CCR_CIRC;   // disable DMA circular mode
}

USBLIB_LineCoding lineCoding = {115200, 0, 0, 8};

const uint8_t USB_DEVICE_DESC[] =
    {
        (uint8_t)18,                        //    bLength
        (uint8_t)USB_DEVICE_DESC_TYPE,      //    bDescriptorType
        (uint8_t)0x00,                      //    bcdUSB
        (uint8_t)0x02,                      //    bcdUSB
        (uint8_t)USB_COMM,                  //    bDeviceClass
        (uint8_t)0,                         //    bDeviceSubClass
        (uint8_t)0,                         //    bDeviceProtocol
        (uint8_t)64,                        //    bMaxPacketSize0
        (uint8_t)LOBYTE(DEVICE_VENDOR_ID),  //    idVendor
        (uint8_t)HIBYTE(DEVICE_VENDOR_ID),  //    idVendor
        (uint8_t)LOBYTE(DEVICE_PRODUCT_ID), //    idProduct
        (uint8_t)HIBYTE(DEVICE_PRODUCT_ID), //    idProduct
        (uint8_t)0x00,                      //    bcdDevice
        (uint8_t)0x01,                      //    bcdDevice
        (uint8_t)1,                         //    iManufacturer
        (uint8_t)2,                         //    iProduct
        (uint8_t)3,                         //    iSerialNumbert
        (uint8_t)1                          //    bNumConfigurations
};
const uint8_t USBD_CDC_CFG_DESCRIPTOR[] =
    {
        /*Configuration Descriptor*/
        0x09, /* bLength: Configuration Descriptor size */
        0x02, /* bDescriptorType: Configuration */
        67,   /* wTotalLength:no of returned bytes */
        0x00,
        0x02, /* bNumInterfaces: 2 interface */
        0x01, /* bConfigurationValue: Configuration value */
        0x00, /* iConfiguration: Index of string descriptor describing the configuration */
        0x80, /* bmAttributes - Bus powered */
        0x32, /* MaxPower 100 mA */

        /*---------------------------------------------------------------------------*/

        /*Interface Descriptor */
        0x09, /* bLength: Interface Descriptor size */
        0x04, /* bDescriptorType: Interface */
        0x00, /* bInterfaceNumber: Number of Interface */
        0x00, /* bAlternateSetting: Alternate setting */
        0x01, /* bNumEndpoints: One endpoints used */
        0x02, /* bInterfaceClass: Communication Interface Class */
        0x02, /* bInterfaceSubClass: Abstract Control Model */
        0x01, /* bInterfaceProtocol: Common AT commands */
        0x00, /* iInterface: */

        /*Header Functional Descriptor*/
        0x05, /* bLength: Endpoint Descriptor size */
        0x24, /* bDescriptorType: CS_INTERFACE */
        0x00, /* bDescriptorSubtype: Header Func Desc */
        0x10, /* bcdCDC: spec release number */
        0x01,

        /*Call Management Functional Descriptor*/
        0x05, /* bFunctionLength */
        0x24, /* bDescriptorType: CS_INTERFACE */
        0x01, /* bDescriptorSubtype: Call Management Func Desc */
        0x00, /* bmCapabilities: D0+D1 */
        0x01, /* bDataInterface: 1 */

        /*ACM Functional Descriptor*/
        0x04, /* bFunctionLength */
        0x24, /* bDescriptorType: CS_INTERFACE */
        0x02, /* bDescriptorSubtype: Abstract Control Management desc */
        0x02, /* bmCapabilities */

        /*Union Functional Descriptor*/
        0x05, /* bFunctionLength */
        0x24, /* bDescriptorType: CS_INTERFACE */
        0x06, /* bDescriptorSubtype: Union func desc */
        0x00, /* bMasterInterface: Communication class interface */
        0x01, /* bSlaveInterface0: Data Class Interface */

        /*Endpoint 2 Descriptor*/
        0x07, /* bLength: Endpoint Descriptor size */
        0x05, /* bDescriptorType: Endpoint */
        0x81, /* bEndpointAddress IN1 */
        0x03, /* bmAttributes: Interrupt */
        0x08, /* wMaxPacketSize LO: */
        0x00, /* wMaxPacketSize HI: */
        0x10, /* bInterval: */
        /*---------------------------------------------------------------------------*/

        /*Data class interface descriptor*/
        0x09, /* bLength: Endpoint Descriptor size */
        0x04, /* bDescriptorType: */
        0x01, /* bInterfaceNumber: Number of Interface */
        0x00, /* bAlternateSetting: Alternate setting */
        0x02, /* bNumEndpoints: Two endpoints used */
        0x0A, /* bInterfaceClass: CDC */
        0x02, /* bInterfaceSubClass: */
        0x00, /* bInterfaceProtocol: */
        0x00, /* iInterface: */

        /*Endpoint IN2 Descriptor*/
        0x07, /* bLength: Endpoint Descriptor size */
        0x05, /* bDescriptorType: Endpoint */
        0x82, /* bEndpointAddress IN2 */
        0x02, /* bmAttributes: Bulk */
        64,   /* wMaxPacketSize: */
        0x00,
        0x00, /* bInterval: ignore for Bulk transfer */

        /*Endpoint OUT3 Descriptor*/
        0x07, /* bLength: Endpoint Descriptor size */
        0x05, /* bDescriptorType: Endpoint */
        0x03, /* bEndpointAddress */
        0x02, /* bmAttributes: Bulk */
        64,   /* wMaxPacketSize: */
        0,
        0x00 /* bInterval: ignore for Bulk transfer */
};

/* USB String Descriptors */
_USB_LANG_ID_(LANG_US);
_USB_STRING_(wsVendor, u"SaeWave.com")
_USB_STRING_(wsProd, u"RemoteSwitch HUB")
_USB_STRING_(wsSN, u"0123-4567-89")
_USB_STRING_(wsCDC, u"CDC Device")
_USB_STRING_(wsCDCData, u"CDC Data")

void USBLIB_Reset(void)
{
    /* *********** WARNING ********** */
    /* We DO NOT CHANGE BTABLE!! So we assume that buffer table start from address 0!!! */

    uint16_t Addr = SIZE_OF_BTABLE;
    for (uint8_t i = 0; i < EPCOUNT; i++) {
        EPBufTable[i].TX_Address = Addr;
        EPBufTable[i].TX_Count   = 0;
        Addr += EpData[i].TX_Max;
        EPBufTable[i].RX_Address = Addr;
        if (EpData[i].RX_Max >= 64)
            EPBufTable[i].RX_Count = (uint16_t) (0x8000 | ((EpData[i].RX_Max / 64) << 10));
        else
            EPBufTable[i].RX_Count = (uint16_t) ((EpData[i].RX_Max / 2) << 10);

        Addr += EpData[i].RX_Max;

        *(uint16_t *)&(USB_->EPR[i]) = (uint16_t)(EpData[i].Number | EpData[i].Type | RX_VALID | TX_NAK);
    }

    for (uint8_t i = EPCOUNT; i < 8; i++) {
        *(uint16_t *)&(USB_->EPR[i]) = (uint16_t)(i | RX_NAK | TX_NAK);
    }
    USB->CNTR   = USB_CNTR_CTRM | USB_CNTR_RESETM | USB_CNTR_SUSPM;
    USB->ISTR   = 0x00;
    USB->BTABLE = 0x00;
    USB->DADDR  = USB_DADDR_EF;
}

void USBLIB_setStatTx(uint8_t EPn, uint16_t Stat)
{
    register uint16_t val = *(uint16_t *)&(USB_->EPR[EPn]);
    *(uint16_t *)&(USB_->EPR[EPn]) = (uint16_t) ((val ^ (Stat & EP_STAT_TX)) & (EP_MASK | EP_STAT_TX));
}

void USBLIB_setStatRx(uint8_t EPn, uint16_t Stat)
{
    register uint16_t val = *(uint16_t *)&(USB_->EPR[EPn]);
    *(uint16_t *)&(USB_->EPR[EPn]) = (uint16_t) ((val ^ (Stat & EP_STAT_RX)) & (EP_MASK | EP_STAT_RX));
}

void dmacpy(void *dst, const void *src, uint8_t count) {
    DMA1_Channel4->CPAR = (uint32_t)src;   // set peripheral address
    DMA1_Channel4->CMAR = (uint32_t)dst;   // set memory address
    DMA1_Channel4->CNDTR = count;          // circle count
    DMA1_Channel4->CCR |= DMA_CCR_EN;      // run DMA channel 4

    while (DMA1_Channel4->CNDTR != 0);     // wait transfer complete

    DMA1_Channel4->CCR &= ~DMA_CCR_EN;     // disable DMA channel 4
}

void USBLIB_Pma2EPBuf2(uint8_t EPn)
{
    register uint8_t Count = EpData[EPn].lRX = (uint8_t) (EPBufTable[EPn].RX_Count & 0x3FF);
    uint16_t *Address = (uint16_t *) (USB_PBUFFER + EPBufTable[EPn].RX_Address);
    uint16_t *Distination = EpData[EPn].pRX_BUFF;

    dmacpy(Distination, Address, (uint8_t) ((Count + 1) / 2));
}

void USBLIB_EPBuf2Pma(uint8_t EPn)
{
    uint16_t *Distination;
    uint16_t *TX_Buff;
    register uint8_t   Count;

    Count = (uint8_t) (EpData[EPn].lTX <= EpData[EPn].TX_Max ? EpData[EPn].lTX : EpData[EPn].TX_Max);
    EPBufTable[EPn].TX_Count = Count;

    TX_Buff = EpData[EPn].pTX_BUFF;
    Distination = (uint16_t *)(USB_PBUFFER + EPBufTable[EPn].TX_Address);

    dmacpy(Distination, TX_Buff, (uint8_t) ((Count + 1) / 2));

    EpData[EPn].lTX -= Count;
    EpData[EPn].pTX_BUFF = TX_Buff + ((Count + 1) / 2);
    EpData[EPn].TX_PMA_FREE = 0;
}

void USBLIB_SendData(uint8_t EPn, uint16_t *Data, uint16_t Length)
{
    // wait till TX buffer busy. ~3 ms
    uint16_t timeout = 3000;
    while (--timeout>0 && EpData[EPn].TX_PMA_FREE == 0);
    if( EpData[EPn].TX_PMA_FREE == 0 ) {
        return;
    }

    EpData[EPn].lTX      = Length;
    EpData[EPn].pTX_BUFF = Data;
    if (Length > 0) {
        USBLIB_EPBuf2Pma(EPn);
    } else {
        EPBufTable[EPn].TX_Count = 0;
    }
    USBLIB_setStatTx(EPn, TX_VALID);
}

void USBLIB_GetDescriptor(USBLIB_SetupPacket *SPacket)
{
    uint8_t             c;
    uint16_t             sz;
    USB_STR_DESCRIPTOR *pSTR;
    switch (SPacket->wValue.H) {
    case USB_DEVICE_DESC_TYPE:
        USBLIB_SendData(0, (uint16_t *)&USB_DEVICE_DESC, sizeof(USB_DEVICE_DESC));
        break;

    case USB_CFG_DESC_TYPE:
        sz = sizeof(USBD_CDC_CFG_DESCRIPTOR);
        if( sz > SPacket->wLength) {
            sz = SPacket->wLength;
        }
        USBLIB_SendData(0, (uint16_t *)&USBD_CDC_CFG_DESCRIPTOR, sz);
        break;

    case USB_STR_DESC_TYPE:
        pSTR = (USB_STR_DESCRIPTOR *)&wLANGID;

        for (c = 0; c < SetupPacket->wValue.L; c++) {
            pSTR = (USB_STR_DESCRIPTOR *)((uint8_t *)pSTR + pSTR->bLength);
        }
        USBLIB_SendData(0, (uint16_t *)pSTR, pSTR->bLength);
        break;
    default:
        USBLIB_SendData(0, 0, 0);
        break;
    }
}

void USBLIB_EPHandler(uint16_t Status)
{
    uint16_t DeviceConfigured = 0, DeviceStatus = 0;
    uint8_t  EPn = (uint8_t) (Status & USB_ISTR_EP_ID);
    uint32_t EP  = USBEP[EPn];
    if (EP & EP_CTR_RX) { //something received
        USBLIB_Pma2EPBuf2(EPn);
        if (EPn == 0) { //If control endpoint
            if (EP & USB_EP_SETUP) {
                SetupPacket = (USBLIB_SetupPacket *)EpData[EPn].pRX_BUFF;
                switch (SetupPacket->bRequest) {
                case USB_REQUEST_SET_ADDRESS:
                    USBLIB_SendData(0, 0, 0);
                    DeviceAddress = SetupPacket->wValue.L;
                    break;

                case USB_REQUEST_GET_DESCRIPTOR:
                    USBLIB_GetDescriptor(SetupPacket);
                    break;

                case USB_REQUEST_GET_STATUS:
                    USBLIB_SendData(0, &DeviceStatus, 2);
                    break;

                case USB_REQUEST_GET_CONFIGURATION:
                    USBLIB_SendData(0, &DeviceConfigured, 1);
                    break;

                case USB_REQUEST_SET_CONFIGURATION:
                    DeviceConfigured = 1;
                    USBLIB_SendData(0, 0, 0);
                    break;

                case USB_DEVICE_CDC_REQUEST_SET_COMM_FEATURE:
                    //TODO
                    break;

                case USB_DEVICE_CDC_REQUEST_SET_LINE_CODING:        //0x20
                    USBLIB_SendData(0, 0, 0);
                    break;

                case USB_DEVICE_CDC_REQUEST_GET_LINE_CODING:        //0x21
                    USBLIB_SendData(EPn, (uint16_t *)&lineCoding, sizeof(lineCoding));
                    break;

                case USB_DEVICE_CDC_REQUEST_SET_CONTROL_LINE_STATE:         //0x22
                    LineState = SetupPacket->wValue;
                    USBLIB_SendData(0, 0, 0);
                    uUSBLIB_LineStateHandler(SetupPacket->wValue);
                    break;
                }
            }
        } else { // Got data from another EP
            // Call user function
            uUSBLIB_DataReceivedHandler(EpData[EPn].pRX_BUFF, (uint16_t) EpData[EPn].lRX);
        }
        USBEP[EPn] &= 0x78f;
        USBLIB_setStatRx(EPn, RX_VALID);
    }
    if (EP & EP_CTR_TX) { //something transmitted
        if (DeviceAddress) {
            USB->DADDR    = (uint16_t) (DeviceAddress | 0x80);
            DeviceAddress = 0;
        }

        EpData[EPn].TX_PMA_FREE = 1;

        if (EpData[EPn].lTX) { //Have to transmit something?
            USBLIB_EPBuf2Pma(EPn);
            USBLIB_setStatTx(EPn, TX_VALID);
        } else {
            uUSBLIB_DataTransmitedHandler(EPn, EpData[EPn]);
        }

        USBEP[EPn] &= 0x870f;
    }
}

void USB_IRQHandler()
{
    if (USB->ISTR & USB_ISTR_RESET) { // Reset
        USB->ISTR &= ~USB_ISTR_RESET;
        USBLIB_Reset();
        return;
    }
    if (USB->ISTR & USB_ISTR_CTR) { //Handle data on EP
        USBLIB_EPHandler((uint16_t)USB->ISTR);
        USB->ISTR &= ~USB_ISTR_CTR;
        return;
    }
    if (USB->ISTR & USB_ISTR_PMAOVR) {
        USB->ISTR &= ~USB_ISTR_PMAOVR;
        // Handle PMAOVR status
        return;
    }
    if (USB->ISTR & USB_ISTR_SUSP) {
        USB->ISTR &= ~USB_ISTR_SUSP;
        if (USB->DADDR & 0x7f) {
            USB->DADDR = 0;
            USB->CNTR &= ~ 0x800;
        }
        return;
    }
    if (USB->ISTR & USB_ISTR_ERR) {
        USB->ISTR &= ~USB_ISTR_ERR;
        // Handle Error
        return;
    }
    if (USB->ISTR & USB_ISTR_WKUP) {
        USB->ISTR &= ~USB_ISTR_WKUP;
        // Handle Wakeup
        return;
    }
    if (USB->ISTR & USB_ISTR_SOF) {
        USB->ISTR &= ~USB_ISTR_SOF;
        // Handle SOF
        return;
    }
    if (USB->ISTR & USB_ISTR_ESOF) {
        USB->ISTR &= ~USB_ISTR_ESOF;
        // Handle ESOF
        return;
    }
    USB->ISTR = 0;
}

void USBLIB_Transmit(void *Data, uint16_t Length)
{
    if (LineState.L & 0x01) {
        USBLIB_SendData(2, Data, Length);
    }
}

__weak void uUSBLIB_DataReceivedHandler(uint16_t *Data, uint16_t Length)
{
    /* NOTE: This function Should not be modified, when the callback is needed,
       the uUSBLIB_DataReceivedHandler could be implemented in the user file
    */
}

__weak void uUSBLIB_DataTransmitedHandler(uint8_t EPn, USBLIB_EPData EpData)
{
    /* NOTE: This function Should not be modified, when the callback is needed,
       the uUSBLIB_EPStateHandler could be implemented in the user file
    */
}

__weak void uUSBLIB_LineStateHandler(USBLIB_WByte LineState)
{
    /* NOTE: This function Should not be modified, when the callback is needed,
       the uUSBLIB_LineStateHandler could be implemented in the user file
    */
}
