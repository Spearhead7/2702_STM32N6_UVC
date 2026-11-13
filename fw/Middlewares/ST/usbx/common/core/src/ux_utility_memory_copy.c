/**************************************************************************/
/*                                                                        */
/*       Copyright (c) Microsoft Corporation. All rights reserved.        */
/*                                                                        */
/*       This software is licensed under the Microsoft Software License   */
/*       Terms for Microsoft Azure RTOS. Full text of the license can be  */
/*       found in the LICENSE file at https://aka.ms/AzureRTOS_EULA       */
/*       and in the root directory of this software.                      */
/*                                                                        */
/**************************************************************************/


/**************************************************************************/
/**************************************************************************/
/**                                                                       */ 
/** USBX Component                                                        */ 
/**                                                                       */
/**   Utility                                                             */
/**                                                                       */
/**************************************************************************/
/**************************************************************************/


/* Include necessary system files.  */

#define UX_SOURCE_CODE

#include "ux_api.h"
#include "fast_copy.h"   /* N6UVC 수정(M3): Video/Inc */


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _ux_utility_memory_copy                             PORTABLE C      */ 
/*                                                           6.1          */
/*  AUTHOR                                                                */
/*                                                                        */
/*    Chaoqiong Xiao, Microsoft Corporation                               */
/*                                                                        */
/*  DESCRIPTION                                                           */
/*                                                                        */ 
/*    This function copies a block of memory from a source to a           */ 
/*    destination.                                                        */ 
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    memory_destination                    Pointer to destination        */ 
/*    memory_source                         Pointer to source             */ 
/*    length                                Number of bytes to copy       */ 
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */ 
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    USBX Components                                                     */ 
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  05-19-2020     Chaoqiong Xiao           Initial Version 6.0           */
/*  09-30-2020     Chaoqiong Xiao           Modified comment(s),          */
/*                                            resulting in version 6.1    */
/*                                                                        */
/**************************************************************************/
VOID  _ux_utility_memory_copy(VOID *memory_destination, VOID *memory_source, ULONG length)
{


    /* N6UVC 수정(M3): 원본은 1바이트씩 복사하는 루프다. 영상 스레드가 페이로드(3 KB)마다
       이 함수로 엔드포인트 버퍼에 복사하므로 전송 사이 간격이 길어져 USB 처리량이 떨어졌다.
       USBX 안의 호출은 모두 겹치지 않는 영역 복사(원본 주석 "Use case of memcpy is verified")라
       워드 단위 fast_memcpy로 바꾼다(newlib-nano memcpy도 1바이트 루프다). */
    if (length != 0U)
    {
        fast_memcpy(memory_destination, memory_source, length);
    }

    /* Return to caller.  */
    return; 
}

