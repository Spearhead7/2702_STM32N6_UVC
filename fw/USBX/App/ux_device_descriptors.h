/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ux_device_descriptors.h
  * @author  MCD Application Team
  * @brief   USBX Device descriptor header file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __UX_DEVICE_DESCRIPTORS_H__
#define __UX_DEVICE_DESCRIPTORS_H__

#ifdef __cplusplus
extern "C" {
#endif
/* Includes ------------------------------------------------------------------*/
#include "ux_api.h"
#include "ux_stm32_config.h"
#include "ux_device_class_cdc_acm.h"
#include "ux_device_class_video.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private defines -----------------------------------------------------------*/
#define USBD_MAX_NUM_CONFIGURATION                     1U
#define USBD_MAX_SUPPORTED_CLASS                       3U
#define USBD_MAX_CLASS_ENDPOINTS                       9U
#define USBD_MAX_CLASS_INTERFACES                      12U

#define USBD_CDC_ACM_CLASS_ACTIVATED                   1U
#define USBD_VIDEO_CLASS_ACTIVATED                     1U

#define USBD_CONFIG_MAXPOWER                           0U
#define USBD_COMPOSITE_USE_IAD                         1U
#define USBD_DEVICE_FRAMEWORK_BUILDER_ENABLED          1U

/* 장치 + 한정자 + 구성(CDC 66 B + UVC 127 B) 디스크립터가 들어가야 한다. 예제의 200은 부족하다. */
#define USBD_FRAMEWORK_MAX_DESC_SZ                     512U
/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */
/* Enum Class Type */
typedef enum
{
  CLASS_TYPE_NONE     = 0,
  CLASS_TYPE_HID      = 1,
  CLASS_TYPE_CDC_ACM  = 2,
  CLASS_TYPE_MSC      = 3,
  CLASS_TYPE_CDC_ECM  = 4,
  CLASS_TYPE_DFU      = 5,
  CLASS_TYPE_PIMA_MTP = 6,
  CLASS_TYPE_RNDIS    = 7,
  CLASS_TYPE_VIDEO    = 8,
  CLASS_TYPE_CCID     = 9,
  CLASS_TYPE_PRINTER  = 10,
  CLASS_TYPE_AUDIO_10 = 11,
  CLASS_TYPE_AUDIO_20 = 12
} USBD_CompositeClassTypeDef;

/* Enum HID Interface Type */
typedef enum
{
  INTERFACE_HID_CUSTOM     = 0,
  INTERFACE_HID_KEYBOARD   = 1,
  INTERFACE_HID_MOUSE      = 2,
} USBD_HIDInterfaceTypeDef;

/* USB Endpoint handle structure */
typedef struct
{
  uint32_t status;
  uint32_t total_length;
  uint32_t rem_length;
  uint32_t maxpacket;
  uint16_t is_used;
  uint16_t bInterval;
} USBD_EndpointTypeDef;

/* USB endpoint handle structure */
typedef struct
{
  uint8_t add;
  uint8_t type;
  uint16_t size;
  uint8_t is_used;
} USBD_EPTypeDef;

/* USB Composite handle structure */
typedef struct
{
  USBD_CompositeClassTypeDef ClassType;
  uint32_t ClassId;
  uint8_t InterfaceType;
  uint32_t Active;
  uint32_t NumEps;
  uint32_t NumIf;
  USBD_EPTypeDef Eps[USBD_MAX_CLASS_ENDPOINTS];
  uint8_t Ifs[USBD_MAX_CLASS_INTERFACES];
} USBD_CompositeElementTypeDef;

/* USB Device handle structure */
typedef struct _USBD_DevClassHandleTypeDef
{
  uint8_t Speed;
  uint32_t classId;
  uint32_t NumClasses;
  USBD_CompositeElementTypeDef tclasslist[USBD_MAX_SUPPORTED_CLASS];
  uint32_t CurrDevDescSz;
  uint32_t CurrConfDescSz;
} USBD_DevClassHandleTypeDef;

/* USB Device endpoint direction */
typedef enum
{
  OUT   = 0x00,
  IN    = 0x80,
} USBD_EPDirectionTypeDef;

/* USB Device descriptors structure */
typedef struct
{
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint16_t bcdUSB;
  uint8_t bDeviceClass;
  uint8_t bDeviceSubClass;
  uint8_t bDeviceProtocol;
  uint8_t bMaxPacketSize;
  uint16_t idVendor;
  uint16_t idProduct;
  uint16_t bcdDevice;
  uint8_t iManufacturer;
  uint8_t iProduct;
  uint8_t iSerialNumber;
  uint8_t bNumConfigurations;
} __PACKED USBD_DeviceDescTypedef;

/* USB Iad descriptors structure */
typedef struct
{
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bFirstInterface;
  uint8_t bInterfaceCount;
  uint8_t bFunctionClass;
  uint8_t bFunctionSubClass;
  uint8_t bFunctionProtocol;
  uint8_t iFunction;
} __PACKED USBD_IadDescTypedef;

/* USB interface descriptors structure */
typedef struct
{
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bInterfaceNumber;
  uint8_t bAlternateSetting;
  uint8_t bNumEndpoints;
  uint8_t bInterfaceClass;
  uint8_t bInterfaceSubClass;
  uint8_t bInterfaceProtocol;
  uint8_t iInterface;
} __PACKED USBD_IfDescTypedef;

/* USB endpoint descriptors structure */
typedef struct
{
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bEndpointAddress;
  uint8_t bmAttributes;
  uint16_t wMaxPacketSize;
  uint8_t bInterval;
} __PACKED USBD_EpDescTypedef;

/* USB Config descriptors structure */
typedef struct
{
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint16_t wDescriptorLength;
  uint8_t bNumInterfaces;
  uint8_t bConfigurationValue;
  uint8_t iConfiguration;
  uint8_t bmAttributes;
  uint8_t bMaxPower;
} __PACKED USBD_ConfigDescTypedef;

/* USB Qualifier descriptors structure */
typedef struct
{
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint16_t bcdDevice;
  uint8_t Class;
  uint8_t SubClass;
  uint8_t Protocol;
  uint8_t bMaxPacketSize;
  uint8_t bNumConfigurations;
  uint8_t bReserved;
} __PACKED USBD_DevQualiDescTypedef;

#if (USBD_CDC_ACM_CLASS_ACTIVATED == 1) || (USBD_RNDIS_CLASS_ACTIVATED == 1) || (USBD_CDC_ECM_CLASS_ACTIVATED == 1)
typedef struct
{
  /* Header Functional Descriptor*/
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint16_t bcdCDC;
} __PACKED USBD_CDCHeaderFuncDescTypedef;

typedef struct
{
  /* Call Management Functional Descriptor*/
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint8_t bmCapabilities;
  uint8_t bDataInterface;
} __PACKED USBD_CDCCallMgmFuncDescTypedef;

typedef struct
{
  /* ACM Functional Descriptor*/
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint8_t bmCapabilities;
} __PACKED USBD_CDCACMFuncDescTypedef;

typedef struct
{
  /* Union Functional Descriptor*/
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint8_t bMasterInterface;
  uint8_t bSlaveInterface;
} __PACKED USBD_CDCUnionFuncDescTypedef;

#endif /* (USBD_CDC_ACM_CLASS_ACTIVATED == 1) || (USBD_RNDIS_CLASS_ACTIVATED == 1)  || (USBD_CDC_ECM_CLASS_ACTIVATED == 1)*/

#if USBD_VIDEO_CLASS_ACTIVATED == 1U
/* 아래 VIDEO 구조체는 STM32CubeN6 V1.4.1 Ux_Device_Video 예제에서 가져왔다. */

/* Video Control Interface Descriptor (Interface Header) */
typedef struct
{
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint16_t bcdUVC;
  uint16_t wTotalLength;
  uint32_t dwClockFrequency;
  uint8_t bInCollection;
  uint8_t aInterfaceNr;
} __PACKED USBD_VIDEOCSVCIfDescTypeDef;

/* Video Interface Descriptor (Input Terminal) */
typedef struct
{
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint8_t bTerminalID;
  uint16_t wTerminalType;
  uint8_t bAssocTerminal;
  uint8_t iTerminal;
} __PACKED USBD_VIDEOInputTerminalDescTypeDef;

/* Video Interface Descriptor (Output Terminal) */
typedef struct
{
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint8_t bTerminalID;
  uint16_t wTerminalType;
  uint8_t bAssocTerminal;
  uint8_t bSourceID;
  uint8_t iTerminal;
} __PACKED USBD_VIDEOOutputTerminalDescTypeDef;

/* Video Streaming Interface Input Header Descriptor */
typedef struct
{
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint8_t bNumFormats;
  uint16_t wTotalLength;
  uint8_t bEndpointAddress;
  uint8_t bmInfo;
  uint8_t bTerminalLink;
  uint8_t bStillCaptureMethod;
  uint8_t bTriggerSupport;
  uint8_t bTriggerUsage;
  uint8_t bControlSize;
  uint8_t bmaControls;
} __PACKED USBD_VIDEOVSHeaderDescTypeDef;

/* Video Format Descriptor*/
typedef struct
{
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubType;
  uint8_t bFormatIndex;
  uint8_t bNumFrameDescriptors;
  uint8_t bmFlags;
  uint8_t bDefaultFrameIndex;
  uint8_t bAspectRatioX;
  uint8_t bAspectRatioY;
  uint8_t bmInterlaceFlag;
  uint8_t bCopyProtect;
} __PACKED USBD_VIDEOPayloadFormatDescTypeDef;

/* Frame Descriptor */
typedef struct
{
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubType;
  uint8_t bFrameIndex;
  uint8_t bmCapabilities;
  uint16_t wWidth;
  uint16_t wHeight;
  uint32_t dwMinBitRate;
  uint32_t dwMaxBitRate;
  uint32_t dwMaxVideoFrameBufferSize;
  uint32_t dwDefaultFrameInterval;
  uint8_t bFrameIntervalType;
  uint32_t dwFrameInterval;
} __PACKED USBD_VIDEOFrameDescTypeDef;
#endif /* USBD_VIDEO_CLASS_ACTIVATED == 1U */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

uint8_t *USBD_Get_Device_Framework_Speed(uint8_t Speed, ULONG *Length);
uint8_t *USBD_Get_String_Framework(ULONG *Length);
uint8_t *USBD_Get_Language_Id_Framework(ULONG *Length);
uint16_t USBD_Get_Interface_Number(uint8_t class_type, uint8_t interface_type);
uint16_t USBD_Get_Configuration_Number(uint8_t class_type, uint8_t interface_type);

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private_defines */

/* USER CODE END Private_defines */

/* 개발용 VID/PID. ST VID에 두 예제(0x5750, 0x7753)와 다른 PID를 써서
   Windows가 예제 때 저장한 인터페이스 정보를 재사용하지 않게 한다. 제품화 때 자체 발급(PRD 10장). */
#define USBD_VID                                      0x0483
#define USBD_PID                                      0x5760
#define USBD_LANGID_STRING                            1033
#define USBD_MANUFACTURER_STRING                      "STMicroelectronics"
#define USBD_PRODUCT_STRING                           "STM32N6 UVC CDC"
#define USBD_SERIAL_NUMBER                            "N6UVC0001"

#define USB_DESC_TYPE_INTERFACE                       0x04U
#define USB_DESC_TYPE_ENDPOINT                        0x05U
#define USB_DESC_TYPE_CONFIGURATION                   0x02U
#define USB_DESC_TYPE_IAD                             0x0BU

#define USBD_EP_TYPE_CTRL                             0x00U
#define USBD_EP_TYPE_ISOC                             0x01U
#define USBD_EP_TYPE_BULK                             0x02U
#define USBD_EP_TYPE_INTR                             0x03U

#define USBD_EP_ATTR_ISOC_NOSYNC                      0x00U
#define USBD_EP_ATTR_ISOC_ASYNC                       0x04U
#define USBD_EP_ATTR_ISOC_ADAPT                       0x08U
#define USBD_EP_ATTR_ISOC_SYNC                        0x0CU

#define USBD_FULL_SPEED                               0x00U
#define USBD_HIGH_SPEED                               0x01U

#define USB_BCDUSB                                    0x0200U
#define LANGUAGE_ID_MAX_LENGTH                        2U

#define USBD_IDX_MFC_STR                              0x01U
#define USBD_IDX_PRODUCT_STR                          0x02U
#define USBD_IDX_SERIAL_STR                           0x03U

#define USBD_MAX_EP0_SIZE                             64U
#define USBD_DEVICE_QUALIFIER_DESC_SIZE               0x0AU

#define USBD_STRING_FRAMEWORK_MAX_LENGTH              256U

/* Device CDC-ACM Class (IF0-1) */
#define USBD_CDCACM_EPINCMD_ADDR                      0x82U
#define USBD_CDCACM_EPINCMD_FS_MPS                    8U
#define USBD_CDCACM_EPINCMD_HS_MPS                    8U
#define USBD_CDCACM_EPIN_ADDR                         0x81U
#define USBD_CDCACM_EPOUT_ADDR                        0x01U
#define USBD_CDCACM_EPIN_FS_MPS                       64U
#define USBD_CDCACM_EPIN_HS_MPS                       512U
#define USBD_CDCACM_EPOUT_FS_MPS                      64U
#define USBD_CDCACM_EPOUT_HS_MPS                      512U
#define USBD_CDCACM_EPINCMD_FS_BINTERVAL              5U
#define USBD_CDCACM_EPINCMD_HS_BINTERVAL              5U

/* Device VIDEO Class (IF2-3). M3: 1280x720 30fps MJPEG.
   HS high-bandwidth isochronous: 1024 B x 3 패킷/마이크로프레임(최대 약 24 MB/s).
   x-cube-n6-camera-capture(USBL_PACKET_PER_MICRO_FRAME=3)와 같다. 1 패킷이면 페이로드마다
   스레드를 거치는 비용 때문에 한 프레임(약 68 KB) 전송에 약 40 ms가 걸려 30 fps를 못 냈다.
   USBD_VIDEO_EPIN_HS_MPS: 디스크립터 wMaxPacketSize 값(bit 12:11 = 추가 패킷 수).
   USBD_VIDEO_HS_PAYLOAD_SIZE: 페이로드(한 마이크로프레임 전송) 최대 바이트. */
#define USBD_VIDEO_EPIN_ADDR                          0x83U
#define USBD_VIDEO_EPIN_FS_MPS                        300U
#define USBD_VIDEO_HS_PKT_PER_UFRAME                  3U
#define USBD_VIDEO_EPIN_HS_MPS                        (1024U | ((USBD_VIDEO_HS_PKT_PER_UFRAME - 1U) << 11))
#define USBD_VIDEO_HS_PAYLOAD_SIZE                    (1024U * USBD_VIDEO_HS_PKT_PER_UFRAME)
#define USBD_VIDEO_EPIN_FS_BINTERVAL                  1U
#define USBD_VIDEO_EPIN_HS_BINTERVAL                  1U

#define UVC_FRAME_WIDTH                               1280U
#define UVC_FRAME_HEIGHT                              720U
#define UVC_CAM_FPS_FS                                10U
#define UVC_CAM_FPS_HS                                30U

#define UVC_MIN_BIT_RATE(n)                           (UVC_FRAME_WIDTH * UVC_FRAME_HEIGHT * 16U * (n))
#define UVC_MAX_BIT_RATE(n)                           (UVC_FRAME_WIDTH * UVC_FRAME_HEIGHT * 16U * (n))
#define UVC_INTERVAL(n)                               (10000000U/(n))
#define UVC_MAX_FRAME_SIZE                            (512U * 1024U)  /* JPEG 한 장 최대 = VFQ_SLOT_SIZE */

#define VC_HEADER_SIZE                                0x37U

#ifndef USBD_CONFIG_STR_DESC_IDX
#define USBD_CONFIG_STR_DESC_IDX                      0U
#endif /* USBD_CONFIG_STR_DESC_IDX */

#ifndef USBD_CONFIG_BMATTRIBUTES
#define USBD_CONFIG_BMATTRIBUTES                      0xC0U
#endif /* USBD_CONFIG_BMATTRIBUTES */

/* Private macro -----------------------------------------------------------*/
/* USER CODE BEGIN Private_macro */

/* USER CODE END Private_macro */
#define __USBD_FRAMEWORK_SET_EP(epadd, eptype, epsize, HSinterval, FSinterval) do { \
                                /* Append Endpoint descriptor to Configuration descriptor */ \
                                pEpDesc = ((USBD_EpDescTypedef*)((uint32_t)pConf + *Sze)); \
                                pEpDesc->bLength            = (uint8_t)sizeof(USBD_EpDescTypedef); \
                                pEpDesc->bDescriptorType    = USB_DESC_TYPE_ENDPOINT; \
                                pEpDesc->bEndpointAddress   = (epadd); \
                                pEpDesc->bmAttributes       = (eptype); \
                                pEpDesc->wMaxPacketSize     = (epsize); \
                                if(pdev->Speed == USBD_HIGH_SPEED) \
                                { \
                                  pEpDesc->bInterval        = (HSinterval); \
                                } \
                                else \
                                { \
                                  pEpDesc->bInterval        = (FSinterval); \
                                } \
                                *Sze += (uint32_t)sizeof(USBD_EpDescTypedef); \
                              } while(0)

#define __USBD_FRAMEWORK_SET_IF(ifnum, alt, eps, class, subclass, protocol, istring) do {\
                                /* Interface Descriptor */ \
                                pIfDesc = ((USBD_IfDescTypedef*)((uint32_t)pConf + *Sze)); \
                                pIfDesc->bLength = (uint8_t)sizeof(USBD_IfDescTypedef); \
                                pIfDesc->bDescriptorType = USB_DESC_TYPE_INTERFACE; \
                                pIfDesc->bInterfaceNumber = (ifnum); \
                                pIfDesc->bAlternateSetting = (alt); \
                                pIfDesc->bNumEndpoints = (eps); \
                                pIfDesc->bInterfaceClass = (class); \
                                pIfDesc->bInterfaceSubClass = (subclass); \
                                pIfDesc->bInterfaceProtocol = (protocol); \
                                pIfDesc->iInterface = (istring); \
                                *Sze += (uint32_t)sizeof(USBD_IfDescTypedef); \
                              } while(0)
#ifdef __cplusplus
}
#endif
#endif  /* __UX_DEVICE_DESCRIPTORS_H__ */
