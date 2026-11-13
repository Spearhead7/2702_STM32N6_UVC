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
