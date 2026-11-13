# 진행 기록

## M1 환경 구축과 예제 확인 — 태블릿 확인만 남음(사용자가 나중에 확인)

### 2026-11-13 (Claude)

**도구 확인**

| 도구 | 버전 | 위치 |
|---|---|---|
| STM32CubeIDE | 1.18.0 | `C:\ST\STM32CubeIDE_1.18.0` |
| STM32CubeProgrammer (+ SigningTool) | 2.23.0 | `C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer` |
| STM32CubeN6 | V1.4.1 (CubeMX 저장소) | `%USERPROFILE%\STM32Cube\Repository\STM32Cube_FW_N6_V1.4.1` |
| Git | — | `C:\Program Files\Git` |

**예제 준비** — 저장소에는 넣지 않고 `D:\JOB2027\2702_STM32N6_UVC\ref\`에 둔다.

- `ref\STM32Cube_FW_N6_V1.4.1\`: CubeN6 V1.4.1에서 Drivers, Middlewares(threadx, usbx, STM32_USBPD_Library, STM32_ExtMem_Manager), 아래 두 예제만 폴더 구조를 유지해 복사했다.
- `ref\x-cube-n6-camera-capture\`: GitHub 태그 v2.0.1을 clone했다. Windows 경로 길이 제한 때문에 이 clone에만 `core.longpaths=true`를 설정했다.

**빌드 결과** (CubeIDE headless 빌드, Debug/Release)

| 예제 | 결과 | 비고 |
|---|---|---|
| Ux_Device_Video (FSBL + 앱) | 0 errors, 0 warnings | 앱과 FSBL 모두 서명해 `*-trusted.bin` 생성(`-align` 포함) |
| USBPD_SNK_Ux_Device_HID_CDC_ACM | 0 errors, 0 warnings | 단일 프로젝트, 개발 모드에서 SRAM 실행 |
| x-cube-n6-camera-capture | 빌드 안 함 | Claude의 외부 코드 빌드가 권한 정책으로 막힘. 사용자가 CubeIDE로 빌드하거나 미리 빌드된 hex 사용 |

**알게 된 점**

- CubeN6 V1.4.1의 DK용 USBX 예제에는 `Ux_Device_CDC_ACM`이 없다(NUCLEO용만 있음). DK에서는 `USBPD_SNK_Ux_Device_HID_CDC_ACM`으로 CDC를 확인한다. 이 예제는 HID+CDC IAD composite라서 M2 디스크립터 작업에도 참고할 수 있다.
- Ux_Device_Video는 개발 모드에서도 앱을 서명해 외부 플래시 0x7010'0000에 구워야 한다. FSBL이 이를 SRAM으로 복사해 실행한다(LRUN 구조). 기본 스트림은 320×236 MJPEG다.
- x-cube-n6-camera-capture는 FreeRTOS + ST `uvcl` 라이브러리(내부적으로 USBX)를 쓴다. ThreadX를 쓰는 CubeN6 예제와 RTOS가 다르다. 스트림은 최대 800×480 YUV422(무압축)이다.
- Ux_Device_Video README의 개발 모드 스위치 표기("BOOT1 1-3")는 다른 예제("BOOT1 2-3")와 다르다. 2-3을 기준으로 안내했다.

### 2026-11-13 (사용자 보드 확인 결과)

**하드웨어**

| 항목 | 값 |
|---|---|
| 보드 | STM32N6570-DK, MB1939-N6570-C02 (보고값 "N6750"은 오타로 보고 N6570으로 기록. ST 예제 테스트 리비전은 C01) |
| 카메라 모듈 | MB1854B (IMX335) |
| USB1 커넥터 | CN18 (camera-capture README가 맞고, CDC 예제 README의 "CN8"은 해당 없음) |

**예제 결과 (Windows PC)**

| 예제 | 결과 |
|---|---|
| ① Ux_Device_Video | 동작. ST 로고가 나오는 짧은 MJPEG 영상. 해상도는 "320×326"으로 보고됨(예제 기준값은 320×236, 확인 필요) |
| ② USBPD_SNK_Ux_Device_HID_CDC_ACM | 동작. COM 포트 2개 인식, 통신 OK |
| ③ x-cube-n6-camera-capture | 3-B(소스 빌드, 개발 모드)로 동작. 640×480 YUV422 영상 확인 |

**남은 것 (M1 완료 기준)**

- [ ] 태블릿 UVC 뷰어에서 ① MJPEG 영상 확인. 태블릿 기종과 앱 이름 기록
  - 외부 플래시에는 ①(FSBL + 앱)이 아직 남아 있을 것이다(③은 개발 모드로 실행했으므로). 부트 스위치를 플래시 부팅으로 두고 USB1(CN18)을 OTG로 태블릿에 연결하면 바로 확인할 수 있다.

## M2 composite 인식(MJPEG 가짜 영상) — 보드 확인 대기

### 2026-11-13 (Claude)

**펌웨어 프로젝트 `fw/` 생성** (CubeIDE 프로젝트 이름 `STM32N6_UVC`, 개발 모드 전용 FSBL형 단일 프로젝트, AXISRAM2에서 실행)

- 출발점: CubeN6 V1.4.1 `USBPD_SNK_Ux_Device_HID_CDC_ACM`(STM32CubeIDE/FSBL). ThreadX + USBX + USBPD(UCPD가 USB 연결을 감지해 장치 시작) 구조를 그대로 쓴다.
- 함께 가져온 것: Drivers(HAL 일부, CMSIS, BSP DK, tcpp0203), Middlewares(threadx, usbx: 장치 스택 + CDC-ACM·Video 클래스, USBPD 라이브러리), `Ux_Device_Video`의 UVC 디스크립터·스트림 코드와 고정 영상 `stream1.h`.
- `.ioc`는 넣지 않았다. CubeMX로 코드를 다시 만들면 composite 수정이 덮어써지므로 **코드를 직접 관리**한다(USER CODE 블록 밖도 수정함).

**바꾼 것**

| 파일 | 내용 |
|---|---|
| `USBX/App/ux_device_descriptors.[ch]` | HID 제거, `UserClassInstance = {CDC_ACM, VIDEO}`. UVC 디스크립터(IAD, VC, IT, OT, VS, MJPEG 320×236, ISOC EP 0x83 MPS 512) 추가. VC 헤더의 `baInterfaceNr`를 예제의 하드코딩 1에서 `tclasslist[].Ifs[1]`로 고쳐 IF 3을 가리키게 함. 디스크립터 버퍼 200 → 512. PID 0x5760(개발용 임시), 제품 이름 "STM32N6 UVC CDC" |
| `USBX/App/app_usbx_device.c` | CDC + Video 클래스 등록, HID 제거, 에코 스레드 하나 |
| `USBX/App/ux_device_cdc_acm.c` | UART 브리지 → 에코. line coding은 저장만 |
| `USBX/App/ux_device_video.c` | 예제 버그 수정: COMMIT의 `dwMaxPayloadTransferSize`에 bInterval(1)이 들어가 있던 것을 HS MPS(512)로 |
| `USBX/Target/ux_stm32_config.h` | `USBD_HAL_ISOINCOMPLETE_CALLBACK` 추가(Video 예제와 같음) |
| `Core/Src/usb_otg.c` | FIFO(word): Rx 0x100, Tx0 0x10, Tx1 CDC IN 0x40, Tx2 CDC CMD 0x10, Tx3 영상 0x100. 합계 0x270 |
| `USBX/App/app_usbx.h`, `AZURE_RTOS/App/app_azure_rtos_config.h` | USBX 메모리 12K → 20K, 풀 18K → 28K |
| `Core/Src/main.c` | printf → USART1(ST-LINK VCP) 연결, 부팅 배너 |
| `STM32N657X0HXQ_AXISRAM2_fsbl.ld` | ROM 255K/RAM 256K → **ROM 383K/RAM 128K**(Debug 빌드 코드가 고정 영상 때문에 넘침). USBX 풀을 비캐시 고정 주소(0x341FA000)에서 일반 RAM으로 옮김(28K로 키우니 SRAM 끝을 넘음, USB DMA를 쓰지 않으므로 캐시 영향 없음) |

**빌드**: Debug(text 296 KB) / Release(text 214 KB) 모두 0 errors, 0 warnings.

**주의(다음 단계)**

- 플래시 부팅(M6)에서는 FSBL 크기 제한과 LRUN 구조를 다시 정해야 한다. 지금 링커 배치는 개발 모드 전용이다.
- 고정 영상 `stream1.h`(약 95 KB)는 M3에서 실제 카메라로 바꾸면 삭제한다.
- 확인 절차: `docs/m2-guide.md`.

**남은 것 (M2 완료 기준)**

- [ ] Windows에서 카메라와 COM 포트 동시 인식
- [ ] CDC 에코 통과(영상 스트리밍 중에도)
