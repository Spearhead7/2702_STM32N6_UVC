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

## M2 composite 인식(MJPEG 가짜 영상) — 완료

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

### 2026-11-13 1차 실행: HardFault (사용자 → Claude)

- 증상: 실행 직후 HardFault. `CFSR=0x00100000`(UsageFault STKOF, 스택 오버플로), 스레드 `CAD`(USBPD), PC = `HAL_RCCEx_PeriphCLKConfig` ← `PWR_TCPP0203_Configure_ADC`.
- 원인: Debug(-O0)에서 두 함수 스택만 928 B인데 CAD 스레드 스택이 1024 B(ST 예제값). M33/M55는 PSPLIM으로 넘침을 바로 잡아낸다.
- 조치: CAD·PE 스택 1024 → 2048, USBPD 풀 5000 → 8K, USBX 앱 스레드·CDC 에코 스레드 1024 → 2048.
- HardFault 핸들러가 원인 레지스터·PC/LR·스레드 이름을 USART1로 출력하도록 바꿨다(`Core/Src/stm32n6xx_it.c`). PC는 `arm-none-eabi-addr2line -f -e Debug/STM32N6_UVC.elf <PC>`로 찾는다.

### 2026-11-13 2차 실행: 사용자 확인 결과 (스택 수정 후)

| 항목 | 결과 |
|---|---|
| 디버그 로그 | 배너, `USB: start`, `CDC: activate` 출력 |
| 장치 관리자 | COM 포트(USB 직렬 장치)와 카메라 동시 인식, 노란 느낌표 없음 |
| CDC 에코 | 정상(친 글자가 한 번 돌아옴, Tera Term 로컬 에코 꺼짐 상태) |
| 영상 | Windows 카메라 앱에서 ST 로고 영상, 320×236 |
| 동시 동작 | 영상 스트리밍 중에도 에코 정상, 카메라 앱을 다시 열어도 영상 나옴 |

**M2 완료 기준 (PRD 9장)**

- [x] Windows에서 카메라와 COM 포트 동시 인식
- [x] CDC 에코 통과(영상 스트리밍 중에도)

참고: 처음 보고에서 "영상 중 에코 안 됨"은 Tera Term 화면에 글자가 두 번 찍혀야 에코라고 오해한 것이었다. 로컬 에코가 꺼져 있으면 화면의 글자 한 번이 보드가 돌려보낸 것이다(m2-guide.md 3장).

## M3 실제 카메라 MJPEG 720p30 — 보드 확인 대기

### 2026-11-13 (Claude)

**설계**

- 카메라·인코더는 USB와 무관하게 부팅 후 계속 돈다(`video` 스레드, 우선순위 15). 결과는 **프레임 큐**(`vframe_queue`)로만 나가고, UVC는 큐의 소비자 하나다. 2단계 SD 녹화는 같은 큐의 두 번째 소비자로 붙는다.
- 영상 경로: IMX335(2592×1944 RAW10, CSI-2 2 lane 1600 Mbps) → DCMIPP PIPE1(ISP 디모자이크·AE·AWB → **16:9 크롭 2592×1458** → 1280×720 축소 → YUV422 YUYV) → PSRAM 캡처 버퍼 3개 → VENC JPEG(4:2:0, qLevel 6) → 프레임 큐 4×512 KB(PSRAM) → UVC.
  - VENC_USB 예제는 4:3 전체를 16:9로 줄여 화면이 찌그러진다. 크롭을 먼저 한다.
  - 캡처 버퍼를 3개로 두고, 다음 주소를 고를 때 인코딩 중인 버퍼는 건너뛴다.
- 프레임 큐: 소비자는 '가장 최근 프레임'을 참조 카운트로 빌린다. 빈 슬롯이 없으면 아무도 빌리지 않은 가장 오래된 프레임을 덮어쓴다(인코더는 멈추지 않음).
- UVC: 페이로드가 하나 나갈 때마다(USBX 영상 스레드의 payload done) 하나를 채운다. 보낼 프레임이 없으면 헤더만 보낸다(VENC_USB는 바쁜 대기). 스트림 시작 때 최신 프레임부터 보낸다.
- 해상도·fps·JPEG 품질은 `Video/Inc/video_cfg.h`의 설정 구조체 하나에 모았다. 버퍼는 1080p 최대 크기로 잡았다.
- PSRAM은 MPU 비캐시(VENC_USB와 같음). CPU는 USB로 보낼 JPEG 조각만 읽는다.
- HSLV(VDDIO3 1.8V 고속 패드) OTP 퓨즈: ST 예제 FSBL은 이 퓨즈를 굽는다(되돌릴 수 없음). 이 펌웨어는 **읽기만** 하고, 꺼져 있으면 PSRAM을 200 → 100 MHz로 낮춘다.

**가져온 소스(STM32CubeN6 V1.4.1, `C:\Users\<사용자>\STM32Cube\Repository\STM32Cube_FW_N6_V1.4.1`)**

| 위치(fw/) | 출처 |
|---|---|
| `Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_{dcmipp,xspi,bsec}.c`, `stm32n6xx_ll_venc.c` | `Drivers/STM32N6xx_HAL_Driver/Src` |
| `Drivers/BSP/STM32N6570-DK/stm32n6570_discovery_xspi.c` | `Drivers/BSP/STM32N6570-DK` |
| `Drivers/BSP/STM32N6570-DK/stm32n6570_discovery_camera.c` | `Projects/STM32N6570-DK/Applications/VENC/VENC_USB/Appli/Core/Src/Patch` (ISP 통합 패치판) |
| `Drivers/BSP/Components/{imx335,aps256xx,mx66uw1g45g}` | `Drivers/BSP/Components` |
| `Middlewares/ST/STM32_ISP_Library` (isp: Inc, Src 3개 / evision: Inc, Lib `*_gcc.a`) | `Middlewares/ST/STM32_ISP_Library` |
| `Middlewares/ST/VideoEncoder_EWL` (ewl_impl.c/h) | `Middlewares/ST/VideoEncoder_EWL` |
| `Middlewares/Third_Party/VideoEncoder` (inc, source/common 4개, source/jpeg) | `Middlewares/Third_Party/VideoEncoder` (H.264 소스는 1-B에서 추가) |
| `Core/Inc/{ewl_conf,aps256xx_conf,mx66uw1g45g_conf}.h` | VENC_USB `Appli/Core/Inc` |

**변경 파일**

| 파일 | 내용 |
|---|---|
| `Video/Inc,Src` (신규) | `video_app.c`(카메라·인코딩 스레드, `MX_DCMIPP_Init` 재정의, 프레임 콜백, 5초 통계), `jpeg_enc.c`(VENC JPEG, EWL 풀 256 KB), `vframe_queue.c`, `video_cfg.h` |
| `Core/Src/main.c` | 배너 M3, `PSRAM_Init()`(XSPI1 = IC3 PLL1/6 200 MHz 또는 /12 100 MHz, memory-mapped, 32곳 읽기/쓰기 확인), RIF에 DCMIPP·CSI·VENC 추가, MPU region1 PSRAM 비캐시 |
| `Core/Src/app_threadx.c` | `video_app_init()` 호출 |
| `Core/Src/stm32n6xx_it.c` | `CSI_IRQHandler`, `DCMIPP_IRQHandler` (VENC_IRQHandler는 ewl_impl.c) |
| `Core/Inc/stm32n6xx_hal_conf.h` | DCMIPP, XSPI, BSEC 모듈 켬 |
| `Core/Inc/stm32n6570_discovery_conf.h` | `USE_IMX335_SENSOR 1U` |
| `STM32N657X0HXQ_AXISRAM2_fsbl.ld` | PSRAM 32M 영역, `.psram_bss (NOLOAD)` |
| `USBX/App/ux_device_video.c` | 고정 영상 → 프레임 큐 스트리밍(헤더 EOH/FID/EOF) |
| `USBX/App/ux_device_descriptors.h` | 1280×720, HS 30fps, MPS 1024, 최대 프레임 512 KB |
| `USBX/App/ux_user.h` | `UX_SLAVE_REQUEST_DATA_MAX_LENGTH` 512 → 1024, `UX_THREAD_STACK_SIZE` 2K |
| `USBX/App/app_usbx.h`, `AZURE_RTOS/App/app_azure_rtos_config.h` | USBX 메모리 20K → 28K, 풀 28K → 36K |
| `Core/Src/usb_otg.c` | Tx3(영상) FIFO 0x100 → 0x200 word. 합계 0x370 |
| `USBX/App/stream1.h` | 삭제(M2 고정 영상) |
| `.cproject` | include 경로 11개, evision 라이브러리 2개(Debug/Release) |

**빌드**: Debug(text 315 KB) / Release(text 201 KB) 모두 0 errors. 경고 2개는 ST VideoEncoder 소스(`encasiccontroller_v2.c`, `encpreprocess.c`)의 `-Wint-in-bool-context`라 고치지 않는다. 내부 RAM 사용 약 74 KB / 96 KB(비캐시 32K 제외), PSRAM 14.1 MB / 32 MB.

**보드에서 확인할 위험**

- PSRAM 200 MHz 안정성(HSLV 퓨즈 상태에 따름).
- DCMIPP 다음 주소가 섀도 레지스터라 프레임 끝 인터럽트가 늦으면 한 프레임 늦게 반영될 수 있다(화면 찢어짐으로 보임).
- USB 대역폭: 1024 B × 8000/s ≈ 8 MB/s → 30fps면 프레임당 약 270 KB까지. JPEG이 크면 fps가 떨어진다. 그때는 qLevel을 낮춘다.
- PRD 5장은 MJPEG에 high-bandwidth(1024×3)를 적어 두었지만, USB DMA를 쓰지 않는 지금은 FIFO가 부족해 1024×1로 했다(PRD 고침).

확인 절차: `docs/m3-guide.md`.

### 2026-11-13 1차 실행: 사용자 확인 결과

| 항목 | 결과 |
|---|---|
| 부팅 로그 | PSRAM HSLV 퓨즈 켜짐, 200 MHz OK. 카메라 30.0 fps, 인코딩 30.0 fps, JPEG 66~67 KB, 인코딩 11~12 ms, skip·noslot·err·pipeerr 0 |
| 영상 | Windows 카메라 앱에서 실제 영상, 색 정상, 찌그러짐 없음(16:9 크롭 확인) |
| 문제 | **가끔 화면 맨 아래 한두 줄(MCU 행)이 검정·초록 띠로 깨짐**. 녹화 9초를 2 fps로 뽑은 18장 중 2장 |

**분석**: 장치 안쪽(캡처·인코딩·큐)은 오류 0. 아래쪽만 깨지는 것은 JPEG 데이터 뒷부분이 호스트에 제대로 안 간 모양이다. USBX STM32 DCD는 incomplete ISO IN이 나면 같은 페이로드를 재전송하는데, HAL은 EP만 끄고 Tx FIFO를 비우지 않는다. USB DMA를 쓰지 않는 slave 모드에서는 못 나간 패킷이 FIFO에 남은 채 다시 쌓여 이후 패킷이 밀리는 것으로 본다.

**조치(2차 펌웨어)**

- `Middlewares/ST/usbx/.../ux_dcd_stm32_callback.c` `HAL_PCD_ISOINIncompleteCallback`: 재전송 전에 해당 EP Tx FIFO flush, 발생 횟수 카운트(`N6UVC 수정` 주석). ST 미들웨어 직접 수정이므로 패키지 갱신 시 다시 넣어야 한다.
- USB OTG 인터럽트 우선순위 7 → 6(카메라·VENC 7보다 높게). ISP 통계 수집이 DCMIPP VSYNC 인터럽트 안에서 돈다.
- 영상 스레드 우선순위 15 → 22(UVC 전송 스레드 20보다 낮게).
- 5초마다 `UVC:` 통계 줄 추가(sent fps, empty, isoinc, buferr, getfail).

### 2026-11-13 2차 실행: 사용자 확인 결과

- `UVC:` 줄 확인(새 펌웨어). sent 30.0 fps, buferr 0, getfail 0. **isoinc가 초당 약 10~15회** 늘어난다. 아래쪽 깨짐은 여전함(카메라 앱에서 더 심함, VLC도 가끔).
- VLC 4:3 표시는 DirectShow 기본 화면비 때문(코덱 정보 MJPG 1280×720 30.000030 fps). 비디오 → 화면비 → 16:9.

**분석**: HAL의 EPDISD 처리를 다시 보니 Tx FIFO flush는 HAL이 이미 한다(`stm32n6xx_hal_pcd.c` EPDISD 분기). 1차 추정(FIFO 잔류)은 틀렸다. 대신 같은 인터럽트에서 XFRC(전송 완료)를 먼저 처리하고 EPDISD에서 incomplete 콜백을 부르므로, 패킷이 실제로 나갔는데 incomplete로도 잡히면 DCD가 **이미 완료된 페이로드를 한 번 더 보낸다**. 호스트는 같은 1 KB를 두 번 받아 그 뒤 JPEG 데이터가 밀리고, 다음 페이로드 arm과도 겹친다 → 그 지점부터 아래쪽이 깨진다.

**조치(3차 펌웨어)**

- `ux_dcd_stm32_callback.c` `HAL_PCD_ISOINIncompleteCallback`: 요청이 아직 `UX_TRANSFER_STATUS_PENDING`일 때만 재전송. 2차에서 넣은 FIFO flush는 중복이라 뺐다.
- `UVC:` 줄에 `dupskip`(완료된 요청이라 재전송을 건너뛴 횟수) 추가. dupskip이 늘면서 깨짐이 사라지면 이 원인이 맞다.
- 그래도 깨지면 다음 후보: ST x-cube-n6-camera-capture처럼 USB DMA 사용(`UVC_LIB_USE_DMA`), 캡처 버퍼 tearing.

### 2026-11-13 3차 실행: 사용자 확인 결과 → 4차(USB DMA)

- 3차(완료된 요청은 재전송 안 함): 카메라 앱에서 **아래쪽 깨짐이 더 심해짐**. 그 재전송은 실제로 필요했다 → 3차 추정 틀림, 되돌렸다(ST 원본 동작 + isoinc 카운트만 남김).
- 결론: 재전송 방식이 아니라 DMA 없는 slave 모드(CPU가 FIFO를 채움)가 isochronous 실시간을 못 따라가는 쪽으로 본다. ST x-cube-n6-camera-capture는 같은 보드에서 `UVC_LIB_USE_DMA`로 USB DMA를 쓴다.
- 사용자와 상의: H.264로 전송량을 줄이는 것보다 USB 전송부터 고치기로 함(H.264는 패킷 손실이 다음 IDR까지 번짐).

**조치(4차 펌웨어, USB DMA)**

- `usb_otg.c`: `dma_enable = ENABLE`.
- MPU region 0 비캐시를 0x341F8000(32K) → **0x341F0000(64K)**로 넓힘(`main.c`).
- 링커: USBX 풀(`.UsbxPoolSection`, 36K, 엔드포인트 전송 버퍼가 모두 여기서 나옴)을 0x341F0000에 고정. hpcd 0x341F9000, `.UsbxAppSection` 0x341FA000, MSP 스택은 맨 끝. 캐시 RAM은 0x341E0000~0x341EFFFF 64K. 겹침은 링커 ASSERT로 막는다.
- RIMC OTG1(CID1, Sec/Priv)은 원래 설정돼 있음.
- 빌드 확인은 사용자가 먼저 한다.

### 2026-11-13 4차 실행: 사용자 확인 결과

- 빌드 성공(사용자 확인). **영상 아래쪽 깨짐 없어짐** → 원인은 slave 모드 USB 전송, USB DMA로 해결.
- 남은 확인: 영상 중 CDC 에코, 카메라 앱 재시작, `UVC:` 줄 isoinc 변화. 태블릿 확인은 나중에.

### 2026-11-13 장치 내부 지연 측정 추가 (Claude)

- `Video/Inc/vtime.h`: CPU 사이클 카운터(DWT->CYCCNT) 시각. `vfq_frame_t`에 `t_cap_start/t_cap_end/t_enc_end` 추가.
- `video_app.c`: `BSP_CAMERA_VsyncEventCallback`에서 프레임 시작 시각, 프레임 이벤트에서 캡처 완료 시각, 인코딩 직후 인코딩 완료 시각 기록.
- `ux_device_video.c`: 프레임을 가져온 시각과 **EOF 페이로드의 전송 완료 콜백** 시각(미리 채운 페이로드 수만큼 뒤)을 잰다. 5초마다 `UVC:` 줄 아래에 출력:
  `LAT ms (avg/max, n N): cap a/m enc a/m wait a/m usb a/m total a/m`
  - cap = VSYNC → 캡처 완료, enc = 캡처 완료 → 인코딩 완료, wait = 인코딩 완료 → UVC가 가져감, usb = 가져감 → EOF 전송 완료, total = VSYNC → EOF 전송 완료(PRD 장치 내부 지연, 목표 50 ms 이하).
- 주의: BSP(VENC_USB 판)는 VSYNC 콜백 전에 ISP 통계 수집을 먼저 해서 시작 시각이 약간 늦게 찍힐 수 있다. cap이 33 ms 근처면 VSYNC = 프레임 시작이 맞고, 0에 가까우면 VSYNC가 프레임 끝이라는 뜻이다.

### 2026-11-13 지연 측정 첫 결과 → high-bandwidth isochronous (Claude)

- 첫 `LAT`(카메라 앱 스트리밍): `cap 29.1 enc 14.4 wait 14.9 usb 40.0 total 98.5`(평균 ms), UVC sent 25 fps, isoinc 초당 약 500. VSYNC = 프레임 시작 확인(cap ≈ 29 ms).
- 사용자: VLC는 끊기며 나오고 카메라 앱은 자연스러움(카메라 앱이 프레임 간격을 부드럽게 맞춤). 근본은 장치가 25 fps만 보내는 것.
- 원인: 페이로드 1 KB마다 USBX 영상 스레드 전환 + PSRAM 복사 + arm. 초당 약 1700 페이로드가 한계라 68 KB 프레임에 40 ms.
- 조치: ST x-cube-n6-camera-capture처럼 **1024 B × 3/마이크로프레임**(wMaxPacketSize 0x1400), 페이로드 최대 3072 B. HAL은 ISO IN에서 MULCNT = 패킷 수로 설정한다(`USB_EPStartXfer`).
  - `UX_SLAVE_REQUEST_DATA_MAX_LENGTH` 3072, USBX 메모리 28K → 44K, UX 풀 36K → 52K(비캐시 0x341F0000~), hpcd 0x341FD000.
  - FIFO: Rx 0xC0, EP0 0x10, CDC 0x40/0x10, UVC 0x280(합 0x3A0 word).
  - PRD 5장 USB 표, m3-guide 갱신.

### 2026-11-13 high-bandwidth 결과 → 복사 속도 개선 (Claude)

- 사용자: 빌드 성공, 영상 잘 나옴. VLC 지연 500 ms 이상은 VLC 캐싱 때문(캡처 장치 열기 → 추가 옵션 보기 → 캐싱 0 ms로 줄어듦). 카메라 앱은 지연 거의 없음. **영상 중 CDC 에코 OK, 카메라 앱 재시작 OK.**
- `LAT`(첫 5초): `cap 29.1 enc 24.5 wait 0.4 usb 29.4 total 83.5`, sent 27.7 fps, isoinc 178. usb가 40 → 29 ms로 줄었지만 페이로드 수는 여전히 초당 약 1700 수준이고, 스트리밍 중 인코딩이 12 → 24 ms로 늘어남.
- 원인: 페이로드마다 USBX 영상 스레드에서 복사가 두 번 일어난다(우리 `video_write_payload`: PSRAM → 페이로드 버퍼, USBX 쓰기 스레드: 페이로드 버퍼 → 엔드포인트 버퍼). 둘 다 1바이트 루프였다(`ux_utility_memory_copy`, 그리고 링크된 newlib-nano `memcpy`도 1바이트 루프). 비캐시 PSRAM 1바이트 읽기 3000번이 전송 사이 간격을 늘리고 VENC와 PSRAM 버스를 다툰다.
- 조치:
  - `Video/Src/fast_copy.c` `fast_memcpy`: 원본 4바이트 정렬 후 워드 8개씩 읽기, 대상 비정렬이면 비정렬 쓰기.
  - `ux_utility_memory_copy.c`(USBX 코어, `N6UVC 수정` 주석): `fast_memcpy` 호출로 교체.
  - `ux_device_video.c`: PSRAM 복사를 `fast_memcpy`로, 조각 크기를 4의 배수(3068 B)로 해서 원본 정렬 유지.

### 2026-11-13 복사 개선 결과 (사용자 → Claude)

- 영상 정상. `UVC: sent 30.0 fps`, buferr·getfail 0. isoinc 초당 약 400(재전송으로 처리, 영상 깨짐 없음).
- `LAT` 평균/최대(ms): **cap 29.1 / enc 12.1~12.2(최대 36) / wait 0.0 / usb 15.4~16.8 / total 56.8~58.2(최대 81)**.
- 인코딩은 스트리밍 중에도 12 ms로 돌아옴(1바이트 복사가 PSRAM 버스를 막던 것 해소). USB 40 → 16 ms.
- PRD 장치 내부 목표 50 ms에는 약 7 ms 모자람. 남은 큰 몫은 cap(센서 한 프레임 받기, 29 ms, 구조상 고정)과 usb(16 ms).
- 더 줄이는 방법(미적용): USBX zero-copy(`UX_DEVICE_ENDPOINT_BUFFER_OWNER=1`) + 페이로드 채우기를 별도 스레드로 + 프레임 슬롯을 캐시 영역으로(읽기 버스트). 1-B H.264는 프레임이 평균 약 17 KB라 usb 구간이 저절로 약 4 ms로 줄어든다.

### 2026-11-13 M3 완료 (사용자 결정)

- 최종 확인: 30.0 fps 안정, buferr·getfail 0, LAT total 평균 57.5~57.8 ms(최대 약 81 ms). 영상 중 CDC 에코 OK, 카메라 앱 재시작 OK. VLC는 캐싱 0 ms로 지연 줄어듦.
- **결정**: M3 완료. 장치 내부 지연 50 ms는 1-B H.264(M4)에서 확인한다(프레임이 작아 USB 구간이 줄어듦). 모자라면 USBX zero-copy·채우기 스레드·캐시 슬롯을 적용.
- **MJPEG 백업**: 커밋 b7444ea에 태그 `m3-mjpeg-ref`(원격에도 푸시). H.264 확정 후 MJPEG 코드를 지울 때 참고 코드로 쓴다.
- 마무리 수정(보드 확인 전, 태그 이후):
  - 5초 통계 출력(printf, USART1 1글자씩 블로킹 약 30 ms)을 영상 스레드에서 가장 낮은 우선순위 스레드 `vstats`(30)로 옮김. LAT enc 최대 36 ms / total 최대 81 ms의 원인으로 본다.
  - ISP AE/AWB(`BSP_CAMERA_BackgroundProcess`)를 프레임 인코딩 뒤로 옮김.
- 보드 확인(사용자): LAT enc 최대 36 → **12.0 ms**, total 최대 81 → **58~60 ms**(평균 57.3~57.8 ms 그대로). 통계는 5초마다 정상 출력. 튐의 원인은 영상 스레드 안의 printf였다.

## M4 H.264 전환 (1-B)

### 2026-11-13 M4-1 참고 코드 확보·분석 (Claude)

**받은 것**(저장소 밖, 참고용)
- `ref\x-cube-n6-ai-h264-usb-uvc\` v2.2.1(GitHub 얕은 클론). FreeRTOS + ST `uvcl`(USBX), 안에 CubeN6 사본(HAL V1.3.0 등)이 들어 있다.
- CubeN6 V1.4.1 `VENC_USB`는 CubeMX 저장소 `%USERPROFILE%\STM32Cube\Repository\STM32Cube_FW_N6_V1.4.1\Projects\STM32N6570-DK\Applications\VENC\VENC_USB`에 이미 있다. **ThreadX + USBX + ux_device_descriptors 구조가 우리 fw와 같다** → M4의 주 참고.
- VideoEncoder: 두 예제 모두 우리 fw의 사본과 inc·common 내용이 같다(줄끝만 다름). 우리에게 없는 것은 `source/h264/`(32개 파일)와 `source/common/encInputLineBuffer.c`(DCMIPP–VENC 하드웨어 핸드셰이크용)뿐이다. CubeN6 V1.4.1 저장소에서 가져온다.

**인코더 설정 비교**

| 항목 | VENC_USB (CubeN6) | ai-h264-usb-uvc | M4 계획 |
|---|---|---|---|
| 입력 | YUYV(DCMIPP YUV422_1), 슬라이스 모드는 NV12(YUV420_2) | ARGB8888(RGB888 32bpp) | **YUYV 그대로**(지금 DCMIPP 출력) |
| level / 스트림 | 4.1, BYTE_STREAM(Annex-B) | 5.1, BYTE_STREAM | 3.1, BYTE_STREAM |
| 율 제어 | pictureRc·mbRc, HRD 켬, qpHdr 26, qp 0~51 | VBR(HRD 끔), qpHdr 25, qp 10~51, 720p 약 11 Mbps | CBR 4 Mbps(HRD로 순간 비트 제한) |
| GOP | 30 | fps−1(=29, `pic_cnt % (gop+1)`) | 30 |
| SPS/PPS | `idrHeader = 1`(IDR마다) | `idrHeader = 1` + 첫 프레임 `H264EncStrmStart` | 같음. 스트림 시작 시 IDR 강제 |
| 출력 정렬 | — | StrmStart 뒤 출력 주소 8바이트 정렬용 필러 NAL(0x2c) 삽입 | 같은 처리 필요 |
| VENC 메모리 | 풀 8 MB(1080p 기준) | 선형 할당 4 MB(PSRAM) | 4 MB부터(지금 EWL 풀 256 KB) |
| 하드웨어 핸드셰이크 | 있음(`VENC_HW_MODE_ENABLE`, 720p_Slice: NV12 입력 버퍼를 내부 RAM에 16줄×2) | 없음 | M4-6 지연 대책 후보 |

**UVC 페이로드 방식**

| 예제 | 방식 | bcdUVC | 비고 |
|---|---|---|---|
| VENC_USB | frame-based H.264(기본, `USBD_UVC_USE_FRAME_BASE_H264`) 또는 UVC 1.5 H.264(`USBD_UVC_USE_H264`) 스위치 | 0x0110 | 두 방식 코드가 모두 있다 → ①·② 시험에 그대로 참고 |
| ai-h264-usb-uvc | frame-based H.264(GUID 'H264'), bVariableSize 1 | 0x0150 | README의 Windows 재생 방법은 **ffplay만** 안내(카메라 앱 언급 없음) |

- 두 예제 모두 기본은 frame-based → 계획의 ① 우선 순서가 맞다. Windows 카메라 앱 재생은 보장되지 않으므로 M4-3에서 ffplay(우리에겐 ShareX ffmpeg에 ffplay 없음 → ffmpeg로 캡처 저장 또는 VLC)와 카메라 앱을 함께 본다.
- MJPEG도 ai 예제는 frame-based(GUID 'MJPG')로 낼 수 있다. 우리는 지금처럼 MJPEG 전용 포맷(VS_FORMAT_MJPEG)을 유지한다.

**지연 관점**
- 지금 장치 내부 57 ms 중 cap 29 ms는 프레임 모드에서는 줄일 수 없다. VENC_USB의 하드웨어 핸드셰이크(슬라이스) 모드는 DCMIPP가 16줄을 쓸 때마다 VENC가 바로 인코딩하므로 cap과 enc가 겹친다 → 50 ms 목표의 가장 확실한 수단. 다만 입력이 NV12여야 하고(DCMIPP 출력 변경), MJPEG(YUYV)와 동시 유지가 어렵다 → M4-6에서 프레임 모드 결과를 보고 결정.

### 2026-11-13 M4-2 H.264 인코더 추가 (Claude)

**가져온 소스**: CubeN6 V1.4.1 저장소의 VideoEncoder 9.22.3.7 → `fw/Middlewares/Third_Party/VideoEncoder/source/h264/`(32개), `source/common/encInputLineBuffer.c`. `.cproject` 두 구성에 `source/h264` include 경로 추가.

**코드**
- `fw/Video/Src/h264_enc.c/.h`(새로 만듦): Baseline(CAVLC), Level 3.1, YUYV 입력, 참조 1장, I/P만. CBR(pictureRc·mbRc·HRD, cpb = 1초분), qpHdr 26, qp 10~51, GOP 30. `H264EncStrmStart`의 SPS/PPS를 보관해 IDR마다 앞에 붙이고, 뒤에 8바이트 정렬용 필러 NAL(type 12)을 넣는다. `h264_enc_request_idr()`로 즉시 IDR. 인코딩 실패 뒤에는 IDR부터 다시.
- `fw/Video/Src/venc_ewl.c`(새로 만듦): EWL 작업 메모리 6 MB(PSRAM). JPEG·H.264가 번갈아 쓴다. `jpeg_enc.c`의 256 KB 풀은 지웠다.
- `video_cfg.h`: `VIDEO_DEFAULT_FORMAT`(지금 시험용 1 = H.264), `VIDEO_DEFAULT_BITRATE` 4 Mbps, `VIDEO_DEFAULT_GOP` 30.
- `video_app.c`: `video_set_format()` / `video_request_keyframe()` 추가. 영상 스레드가 프레임마다 요청 형식을 보고 인코더를 다시 만든다(VENC 인스턴스는 하나). 프레임 큐에 형식·키프레임 여부를 넣는다. 통계 줄에 형식, 평균·IDR 크기 추가.
- `ux_device_video.c`: 호스트가 고른 형식(아직 MJPEG 고정)과 다른 프레임, 키프레임 대기 중의 P 프레임은 보내지 않고 `skipfrm`으로 센다. 스트림 시작 시 키프레임 대기.

**M4-2 보드 확인에서 볼 것**: 부팅 로그 `VIDEO: encoder H264`, 5초 통계 `VIDEO: H264 … enc 30 fps, avg ≈17 KB, idr N KB, enc N ms, err 0`. UVC는 아직 MJPEG만 알려 주므로 카메라 앱에는 영상이 나오지 않는 것이 정상(`skipfrm` 증가).
- Debug 링크 오류(ROM 383K를 800 B 초과): H.264 라이브러리가 -O0에서 커서. `.cproject` Debug에 `Middlewares/Third_Party/VideoEncoder` 폴더만 -Os로 빌드하도록 설정(Release는 원래 전체 -Os).

**M4-2 보드 확인 결과(2026-11-13, 사용자)**: 통과. `VIDEO: encoder H264`, cap/enc 30.0 fps, avg 16 KB(≈3.9 Mbps, 목표 4 Mbps CBR와 일치), IDR 약 117~119 KB(30프레임마다), enc 16~19 ms, err 0, pipeerr 0, noslot 0. UVC는 MJPEG만 알려 주므로 `skipfrm`이 30/s로 늘고 PC·갤럭시 노트20 모두 까만 화면(예상대로). IDR이 평균의 약 7배라 M4-6에서 IDR 크기·지연을 함께 본다.

### 2026-11-13 M4-3 UVC H.264 frame-based 추가 (Claude)

- `ux_device_descriptors.h/.c`: VS 인터페이스에 형식 2개. 형식 1 = MJPEG(기존), 형식 2 = H.264 frame-based(UVC 1.1, VS_FORMAT_FRAME_BASED 0x10, GUID `H264`, bVariableSize 1, 프레임 1280×720 30fps, 비트레이트 1~16 Mbps 표기). 형식마다 Color Matching 디스크립터 추가. VS 헤더 bmaControls를 형식 수만큼, wTotalLength 126.
- `ux_device_video.c`: Probe/Commit SET_CUR에서 호스트가 보낸 bFormatIndex를 받는다(예제는 무시했음). Commit 형식으로 `uvc_format`을 정하고, 스트림 시작(대체 설정 1) 때 `video_set_format()` + `video_request_keyframe()`. UVC 통계에 형식 표시(`UVC: streaming H264 ...`).
- 인코더 기본 형식은 H.264(스트림을 열면 호스트가 고른 형식으로 바뀜). 부팅 배너를 M4로.
- 빌드 확인은 사용자가 한다.
- 보드 시험 1차: PC에서 'USB 장치 인식 실패'. 원인: 구성 디스크립터가 273 B로 늘었는데 USBX 제어 전송 버퍼 기본값(UX_SLAVE_REQUEST_CONTROL_MAX_LENGTH 256)을 넘어 GET_DESCRIPTOR가 STALL. `ux_user.h`에서 512로 늘림.
- 보드 시험 2차(사용자, Windows): 장치 인식 OK. ffmpeg dshow 목록에 `mjpeg`, `h264` 1280x720 30fps 둘 다 보임. Windows 카메라 앱은 MJPEG를 고름(정상 표시). ffmpeg `-vcodec h264`로 열면 `VIDEO: encoder H264` → `UVC: streaming H264, sent 29.9 fps`, 닫고 MJPEG로 열면 다시 `encoder MJPEG` → 30 fps. 형식 전환 OK, err 0.
- 지연(LAT avg/max): MJPEG total 57.9/58.7 ms(cap 29.1, enc 11.3, usb 17.4). H.264 total 51.5/75.4 ms(cap 29.1, enc 18.6, usb 3.6/29.5). H.264는 USB 구간이 크게 줄었지만 목표 50 ms를 약간 넘는다. 최대값은 IDR(약 122 KB) 프레임의 USB 전송.
  - 실제 USB 전송률이 약 4.3 MB/s(75 KB에 17 ms, 122 KB에 29 ms)로 HS 3×1024 B 이론값(약 24 MB/s)보다 훨씬 낮다(isoinc 계속 증가). M4-6에서 USB 최적화(zero-copy, 페이로드 채우기)와 DCMIPP–VENC 핸드셰이크(cap 29 ms 겹치기)를 검토.
- H.264 녹화 확인(Claude가 이 PC에서 ffmpeg로 직접): 처음 0바이트였던 이유는 dshow가 패킷에 키프레임 표시를 하지 않아 ffmpeg `-c copy`가 첫 키프레임을 기다리며 모두 버렸기 때문(장치 문제 아님, 152 패킷 2.58 MB는 받았음). `-copyinkf`를 붙이면 5초 150프레임, 4.0 Mbps, Constrained Baseline 1280x720 30fps로 저장되고 디코딩 오류 0, 화면 정상. **M4-3 Windows 확인 통과.**
  - 스트림을 보니 IDR 앞에 SPS/PPS가 두 번 들어간다(h264_enc.c가 붙이는 것 + 인코더 `idrHeader=1`). 동작에는 문제없음. 정리 대상(M4-6/M4-7).
  - 녹화 명령: `ffmpeg -f dshow -vcodec h264 -video_size 1280x720 -framerate 30 -i video="STM32N6 UVC CDC" -c copy -copyinkf -t 10 test.h264`

### 2026-11-13 M4-5 갤럭시 노트20 시험 (사용자)

- 연결하자마자 권한 창이나 반응이 없어서 처음에는 인식 실패로 보였다. 실제로는 카메라 앱을 열 때 장치를 연다(USB Device Info 등으로 확인). OTG 젠더와 C to C 모두 같음.
- 앱이 MJPEG를 골라 `UVC: streaming MJPEG, sent 30.0 fps`, LAT total 57.4 ms. 화면 정상, 화질 좋고 깨짐 없음. **MJPEG 노트20 통과.** isoinc가 PC보다 많지만(약 380/s) 화면 영향은 보이지 않음.
- 남은 것: 앱에서 H.264를 고를 수 있는지, 앱 이름·버전과 Android 버전 기록(PRD 10장).
- 앱: ShenYao China "USB Camera"(Play 스토어). 버전·무료/Pro 여부, Android 버전은 확인 중.
- 앱 해상도 목록에 `MJPEG 1280x720`, `H.264 1280x720` 둘 다 보이고 선택 가능. **H.264를 고르면 영상이 잘 나옴(사용자 확인). M4-5 노트20 H.264 통과** → frame-based(①)로 충분, UVC 1.5(M4-4)·TS(M4-5b)는 필요 없음.
  - 노트20 H.264 로그: sent 29.9~30.1 fps, err 0, LAT total 평균 50.7~51.2 ms / 최대 64~68 ms(cap 29.1, enc 18, usb 3.6, IDR일 때 usb 최대 약 19~22 ms). isoinc 약 50/s로 MJPEG(약 380/s)보다 적음.
- 기록: 갤럭시 노트20, One UI 5.1(Android 13), ShenYao "USB Camera" 12.3.2. PRD 10장 미결 사항에 반영.

### 2026-11-13 M4-6 지연 개선 1차 (Claude)

분석(H.264 LAT: cap 29.1 + enc 18 + usb 3.6 = 51 ms, IDR일 때 usb 최대 약 20~29 ms):
- **enc 18 ms**: HCLK가 100 MHz(SYSCLK 400 / 4)였다. VENC는 HCLK로 돈다. ST 예제들은 HCLK 200 MHz.
- **usb**: 실제 전송률 약 4.3 MB/s(이론 약 24 MB/s). USBX 영상 스레드는 페이로드 하나를 보내고 완료를 기다린 뒤 다음 것을 보내는데, 그 사이에 (1) 완료 콜백 안의 PSRAM→페이로드 복사 3 KB, (2) USBX가 페이로드를 엔드포인트 버퍼로 다시 복사하는 3 KB가 끼어 마이크로프레임을 자주 놓쳤다(isoinc 증가).
- **cap 29 ms**: 센서가 한 프레임을 내보내는 시간. 줄이려면 DCMIPP–VENC 핸드셰이크(줄 단위 인코딩)가 필요하다. 이번에는 손대지 않음.

수정:
- `main.c`: HCLK 200 MHz(`RCC_HCLK_DIV2`). I2C·UART·타이머는 HAL이 클럭을 읽어 다시 계산하므로 영향 없음(예상).
- `ux_user.h`: `UX_DEVICE_ENDPOINT_BUFFER_OWNER 1` → UVC 페이로드 버퍼를 그대로 DMA로 보낸다(복사 1번 제거). 엔드포인트마다 잡던 3 KB 버퍼도 없어짐.
- `ux_device_video.c`: 페이로드 채우기 스레드(우선순위 21, USBX 영상 스레드 20 바로 아래). 완료 콜백은 신호만 준다. 프레임 데이터가 있으면 페이로드 버퍼(4 → 8개)를 모두 앞서 채우고, 없으면 빈 페이로드는 2개까지만. 스트림 시작/정지 처리도 이 스레드로 옮김(USB 인터럽트 문맥에서 복사·스레드 동작을 하지 않게).
- 새 통계 `UVCPAY: data N/s, gap>200us M, maxgap X us`: 프레임 데이터 페이로드끼리 전송 간격. 이상적이면 125 us.
- `h264_enc.c`: IDR 앞의 중복 SPS/PPS 제거(인코더가 `idrHeader=1`로 이미 넣는다). 필러 NAL도 필요 없어짐.

**M4-6 1차 보드 결과(2026-11-13, 사용자, PC H.264)**:
- LAT total **평균 48.9~49.3 ms / 최대 51.0~51.8 ms**(이전 51.5 / 75.4). cap 29.1, enc 18.1~18.4, usb 평균 1.6 / 최대 5.8 ms(이전 IDR일 때 20~29 ms).
- USB: UVCPAY gap>200us 0, maxgap 약 130 us(이상값 125 us). IDR 116 KB도 약 6 ms에 보냄 → USB 구간은 해결. isoinc 2(이전에는 계속 증가).
- **enc는 HCLK 200 MHz로 바꿔도 18 ms 그대로** → VENC 시간은 HCLK가 아니라 PSRAM 대역폭(YUYV 입력 1.8 MB + 참조 읽기 + 복원 쓰기 ≈ 프레임당 5 MB)에 묶인 것으로 보인다.
- 남은 50 ms 초과분은 최대값 약 2 ms. cap+enc(47 ms)가 직렬이라 더 줄이려면 DCMIPP–VENC 핸드셰이크(줄 단위, cap과 enc를 겹침)가 필요.
- 부팅 로그에 `USBPD: hard reset tx`가 반복(PC A to C 케이블, PD 상대 없음). USB 시작은 한 번뿐이라 영향 없음.

### 2026-11-13 M4-6 2차: DCMIPP–VENC 하드웨어 핸드셰이크 + M4-7 MJPEG 삭제 (Claude, 사용자 지시)

사용자 결정: "DCMIPP–VENC 핸드셰이크하고 MJPEG는 삭제". 참고: CubeN6 V1.4.1 `VENC_USB`(CubeMX 저장소)의 `venc_h264_config_720p_Slice.h`, `dcmipp_app.c`(`dcmipp_enable_HW_Handshake`), `venc_app.c`(`IsVideoOverflow`, `encoder_reset`).

핸드셰이크(fw/Video):
- DCMIPP PIPE1 출력 YUYV → **NV12**(`YUV420_2`, pitch = 폭). 프레임 버퍼 3장(PSRAM 12 MB) 대신 **64줄 줄 버퍼**(Y 64줄 + UV 32줄, 최대 폭 1920 기준 180 KB)를 내부 SRAM에 둔다. 링커에 `LINEBUF`(AXISRAM2 아래쪽 0x34100000, 512K, 개발 모드에서 비어 있음)와 `.linebuf_bss` 추가, 매크로 `IN_LINEBUF`.
- DCMIPP: line wrapping 64줄, LINEMULT 32줄(= VENC 하드웨어 신호). 라인 이벤트 인터럽트는 '늦음' 판단용으로만 센다.
- VENC: `H264ENC_YUV420_SEMIPLANAR`, `viewMode = SINGLE_BUFFER`, `inputLineBufEn/LoopBackEn/HwModeEn = 1`, depth 2 MB 줄.
- 흐름: 프레임 끝 이벤트 → 영상 스레드가 바로 `h264_enc_frame()`(다음 프레임을 인코딩, 그 프레임 캡처가 끝날 때까지 VENC 인터럽트를 기다리며 잠듦). 걸기 전에 다음 프레임 라인 이벤트가 이미 왔으면 건너뜀(`late`). `H264ENC_FUSE_ERROR`면 DCMIPP 일시정지 → VENC 리셋 → 인코더 재생성(`desync`).
- ISP AE/AWB(`BSP_CAMERA_BackgroundProcess`)는 새 `isp` 스레드(우선순위 23, 스택 8K)로 옮김. 영상 스레드(22, 스택 6K)가 인코딩을 기다리는 동안 돈다.
- 통계 줄 변경: `VIDEO: H264 ... call X ms (max), late N, desync N, ...`. `call`은 인코딩 호출 시간(이제 캡처 시간 포함, 약 30 ms가 정상). LAT의 `enc`(캡처 완료 → 인코딩 완료)가 0~2 ms로 줄어야 한다.

MJPEG 삭제(M4-7, PRD F-02/F-08):
- `jpeg_enc.c/.h` 삭제, UVC 디스크립터에서 MJPEG 형식·프레임·색 디스크립터 삭제 → H.264 frame-based가 형식 1(구성 디스크립터 273 → 226 B). Commit 형식 전환, `video_set_format()`, `VFQ_FMT_MJPEG`, `jpeg_q` 설정 삭제. VideoEncoder 라이브러리의 `source/jpeg`는 그대로(링커가 안 쓰는 코드 제거).
- 참고 코드는 태그 `m3-mjpeg-ref`.

확인: 바뀐 C 파일 7개를 arm-none-eabi-gcc `-fsyntax-only -Wall`로 검사해 오류·경고 없음. 빌드·링크는 사용자가 한다.

**M4-6 2차 보드 결과(2026-11-14, 사용자 로그)**: 핸드셰이크 정상 동작.
- VIDEO: cap/enc 30.0 fps, late 0, desync 0, err 0, pipeerr 0, call 30~31 ms(캡처 시간 포함, 정상). IDR 72~93 KB, 평균 16 KB. max 197 KB는 시작 직후 첫 IDR(율 제어 초기값).
- LAT(정상 구간): **cap 25.9 / enc 0.0 / wait 0.0 / usb 1.6(최대 4.8) / total 평균 27.6 ms, 최대 31.0 ms** → 장치 내부 50 ms 목표 달성(1차 49.3/51.8에서 약 22 ms 줄어듦). 첫 통계 구간의 wait 최대 26.8·total 최대 57.4는 스트림 시작 직후 한 번.
- cap이 29.1 → 25.9 ms로 준 것은 DCMIPP 출력량 감소(NV12, PSRAM 쓰기 없음) 때문으로 보인다.
- UVC: 29.9~30.0 fps, isoinc 0, UVCPAY maxgap 약 129 us.
- 남은 확인: 화면(깨짐·색), 노트20 H.264, CDC 에코, 스톱워치로 전체 지연(100 ms 이하).

### 2026-11-14 노트20(C to C) 연결 문제 수정 (Claude, 사용자 보고)

증상(사용자): 노트20에 C to C로 연결하면 ① 보드 실행 후 꽂으면 인식 못 함(CC 연결/해제가 약 0.67초 주기로 반복), ② 꽂은 채 실행하면 인식은 되지만 영상 없음(`BSP_CAMERA_Init ret -7`).

원인과 수정(진단 출력으로 확인):
- ① **PD 스택(PE)이 연결 약 185 ms 뒤 Error Recovery를 요청** → CAD가 Rd를 떼고 detach → 다시 attach, 무한 반복. TCPP0203 FLGn(과전류 등)은 원인이 아니었다(연결 때 VBUS_OK만). 보드는 ST-LINK 전원이라 PD 계약이 필요 없으므로 `USBPD_DPM_EnterErrorRecovery()`에서 요청을 세기만 하고 따르지 않는다(로그 `USBPD: PE error recovery request n (ignored)`). PE가 왜 요청하는지는 미상(라이브러리 내부).
- CAD detach 때 USB 장치를 끄던 것(USBX 스택 해제·재초기화)을 없앴다. 처음 attach 때 한 번 켜고 끄지 않는다. 실제로 뽑았다 꽂으면 버스 리셋으로 다시 열거된다.
- **VBUS 측정값 1.83배 오류**: N6 ADC 기준전압은 1.8 V인데 `VDD_VALUE`가 3300(BSP USBPD VBUS 환산에만 쓰임) → 5.3 V가 9.7 V로 읽혔다. `stm32n6xx_hal_conf.h`에서 1800으로. (이것만으로는 ①이 해결되지 않았다.)
- ② 꽂은 채 부팅하면 PD 하드 리셋과 겹쳐 IMX335 I2C 확인이 실패(-7). `BSP_CAMERA_Init` 실패 시 DCMIPP 해제 후 300 ms 쉬고 최대 3번 재시도(매번 카메라 전원 재투입). 두 번째 시도에서 성공함.

보드 결과(노트20 + ShenYao USB Camera): ① 실행 후 연결, ② 연결한 채 실행, ③ 영상 중 뽑았다 다시 꽂기 모두 영상 정상. USB HS, 30 fps, 장치 내부 지연 평균 27.6 / 최대 30.9 ms.

남은 것: 꽂은 채 부팅하면 PD 하드 리셋(3회)과 Error Recovery 요청 1회가 로그에 나오지만 영상에는 영향 없음. 다른 기기에서 문제가 생기면 PD 메시지를 쓰지 않는 `USBPDCORE_NOPD_CM55` 라이브러리로 바꾸는 것을 검토(로컬 CubeN6 V1.4.1에 있음). 진단 출력(VBUS, CC/FLG, UVC 요청 카운터, PD 이벤트 번호)은 정리하고 attach/detach 한 줄씩만 남겼다.

### 2026-11-14 전체 지연 스톱워치 측정(노트20, 사용자) — 후속은 나중에

- 스톱워치 폰 화면과 노트20(ShenYao USB Camera) 화면을 한 사진에: 130 / 260 / 130 ms(평균 약 170 ms) → 목표 100 ms 초과.
- 장치 내부 27.6 ms + 센서 노출(최대 약 33 ms) + 화면 표시(약 17~33 ms)를 빼면 약 80~200 ms가 폰 앱 디코딩 버퍼로 추정. 인코더는 SPS VUI에 bitstream_restriction(재정렬 없음)을 이미 넣는다.
- 사용자 결정: 지연 후속은 나중에. 할 일: PC에서 `ffplay -f dshow -vcodec h264 -fflags nobuffer -flags low_delay -framedrop`로 같은 측정을 해 보드/앱 구간 분리, 다른 UVC 뷰어 앱 비교.
- 화질 메모: 화면이 실제보다 약간 보랏빛(AWB 확인 필요).

### 2026-11-14 PC에서 H.264 UVC + CDC 동시 확인 (사용자)

- Windows PC에서 `ffplay -f dshow -vcodec h264 -fflags nobuffer -flags low_delay -framedrop -i video="STM32N6 UVC CDC"`(ffmpeg gyan.dev essentials)로 H.264 영상 재생, 동시에 Tera Term(CN18 USB CDC 포트) 에코 정상.
- 처음에 ffplay가 `vq=0KB`로 멈춘 것은 다른 프로그램이 카메라를 잡고 있었기 때문(모두 닫고 다시 실행하니 정상).
- H.264만 남은 뒤 Windows 기본 카메라 앱은 형식을 못 고르므로 PC 확인은 ffplay(또는 OBS '비디오 캡처 장치' 형식 H264)로 한다.
