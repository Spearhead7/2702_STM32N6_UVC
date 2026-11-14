# STM32N6 UVC — Claude Code 작업 안내

이 저장소는 STM32N6로 만드는 1CH AI DVR 펌웨어다. 지금은 STM32N6570-DK에서 **1단계: USB UVC 카메라 + USB CDC(CLI)** 를 마무리하는 중이다.
제품 방향(2026-11-14 변경)은 `docs/Product Conception.md`, 요구사항·로드맵은 `docs/prd.md`(1장 로드맵, 2~10장 1단계, 11장 2단계 이후)에 있다. 작업 전에 반드시 읽고, 결정이 바뀌면 PRD도 함께 고친다.

## 작업 방식
- 사용자와는 **한국어**로 대화한다. 사용자는 Linux를 모르므로 **Windows 기준**으로 안내한다.
- 개발 도구: STM32CubeIDE(+ STM32CubeMX), STM32CubeProgrammer, Tera Term, Windows 카메라 앱·VLC.
- 단계(PRD 1.3)와 마일스톤 단위로 진행하고, 각 완료 기준을 사용자와 확인한 뒤 다음으로 넘어간다.
- 보드에 굽기, 실물 확인(영상이 나오는지, 장치 관리자 인식 등)은 사용자가 해야 하는 단계다. 무엇을 보고 알려 달라고 할지 구체적으로 요청한다.
- ST 패키지(STM32CubeN6, x-cube-n6-*)는 저장소에 복사하지 않는다. 필요한 소스만 가져오고, 출처와 버전을 커밋 메시지나 README에 적는다.
- 빌드 산출물(Debug/, Release/ 등)은 커밋하지 않는다(.gitignore 참고).

## 확정된 결정 (요약, 자세한 내용은 docs/prd.md)
- 영상: 1280×720 30fps. **1-A MJPEG로 먼저 영상 확인 → 1-B H.264로 전환·확정**. H.264가 태블릿에서 확인되면 MJPEG 디스크립터·코드·CLI 명령은 영구 삭제. 삭제 전 MJPEG 동작 코드는 git 태그 `m3-mjpeg-ref`(커밋 b7444ea)로 보존되어 있다(참고용).
- 인코딩: N6 하드웨어 인코더 VENC. H.264는 I/P만(B-프레임 없음), CBR 4 Mbps, IDR 1초 주기, 스트림 시작 시 SPS/PPS + IDR 먼저.
- USB: USB OTG HS 하나에 CDC-ACM(IF0-1) + UVC(IF2-3) composite, IAD 사용. 예제 UVC 디스크립터의 VC/VS 인터페이스 번호 하드코딩(0/1)을 composite 번호로 고쳐야 한다.
- CDC: 1단계는 Tera Term용 대화형 CLI(프롬프트 `n6uvc> `, 마지막 줄 `OK`/`ERROR: <이유>`, PRD 7장). **2단계에서 CLI를 UART로 옮기고, 4단계부터 CDC는 UVC_Viewer 바이너리 프로토콜 전용**(저장소 밖 `2701_UVC_VIEWER/UVC_Viewer/docs/protocol.md`).
- 지연: 전체 100 ms 이하, 장치 내부(캡처 시작 → USB 전송 완료) 50 ms 이하.
- 실행: 개발 중에는 개발 모드(디버거로 내부 SRAM에 올려 실행). 플래시 부팅(FSBL + 앱을 SRAM으로 복사)은 M6에서.
- 큰 버퍼(프레임, VENC 참조 프레임)는 외부 PSRAM. 내부 SRAM은 코드·스택·USB·라인 버퍼용.
- 호스트: 개발은 Windows PC, 각 단계 최종 확인은 Android 태블릿의 UVC 뷰어 앱.

## 제품 방향 (2026-11-14, 자세한 내용은 docs/Product Conception.md)
- 단일 펌웨어: 녹화(SD, OSD 없음) + AI + 화면(OSD 포함)은 항상, 태블릿 앱이 영상을 열면 UVC를 더한다. UVC/LCD 제품으로 나누지 않는다.
- DK 보드: 입력 IMX335, 출력 DK LCD. 커스텀 보드(나중): 입력 TP2850(MIPI CSI-2), 출력 LTDC YUV422 → TP2912(AHD).
- 해상도는 입력을 따른다. 1080p 입력: AHD 1080p30, 녹화 LCD 모드 1080p15 / UVC 모드 720p30(녹화와 UVC는 같은 스트림).
- USB1: 태블릿이면 장치(UVC + CDC), 마우스면 호스트(HID, 설정 모드). 커스텀 보드는 DRP + Try.SRC + DR_Swap으로 태블릿 충전.
- 진행 순서(PRD 1.3): 1단계 M6 플래시 부팅 → UVC_Viewer H.264 표시 + 노트20 확인(별도 저장소) → 2 SD 녹화 + UART CLI → 3 화면·OSD·마우스 → 4 태블릿 프로토콜 → 5 AI(마지막) → 6 커스텀 보드.

## 설계 제약 (지금부터 지킨다, PRD 1.4)
- 카메라·인코더를 UVC에 묶지 않는다. 인코더 출력은 프레임 큐, SD 녹화와 UVC는 소비자(느린 SD가 UVC를 막지 않게).
- 해상도·fps는 설정값이고 실행 중에 바뀐다(입력 해상도, UVC/LCD 모드).
- 녹화는 OSD 없는 원본. OSD는 LTDC 레이어 2에만, 인코더 입력에는 절대 그리지 않는다.
- DCMIPP 파이프 역할: 인코딩 Pipe1(줄 버퍼 핸드셰이크), AI Pipe2, 화면은 커스텀 보드 Pipe0 / DK Pipe2.
- 설정 항목 테이블을 CLI·프로토콜·GUI가 함께 쓴다. 명령 처리와 테이블을 분리한다.
- 보드마다 다른 부분(영상 입력, 화면, AI 입력, USB-C 전원 역할)은 한 계층 뒤에 둔다.

## 참고 예제
- 로컬 사본은 저장소 밖 `D:\JOB2027\2702_STM32N6_UVC\ref\`에 있다(`STM32Cube_FW_N6_V1.4.1\` 일부, `x-cube-n6-camera-capture\` v2.0.1). 내용은 `docs/progress.md` M1 참고.
- STM32CubeN6: `Projects/STM32N6570-DK/Applications/USBX/Ux_Device_Video`(MJPEG UVC), `USBPD/USBPD_SNK_Ux_Device_HID_CDC_ACM`(DK용 CDC, HID+CDC IAD composite. `Ux_Device_CDC_ACM`은 NUCLEO용만 있음), `VENC/VENC_USB`
- https://github.com/STMicroelectronics/x-cube-n6-camera-capture (IMX335 → ISP → UVC)
- https://github.com/STMicroelectronics/x-cube-n6-ai-h264-usb-uvc (VENC H.264 → UVC, AI 부분 제외)

## 빌드 (Claude가 직접 확인할 때)
CubeIDE headless 빌드. 워크스페이스는 임시 폴더를 쓰고, `-import`에는 `.cproject`가 있는 하위 프로젝트 폴더를 준다.

```powershell
& "C:\ST\STM32CubeIDE_1.18.0\STM32CubeIDE\stm32cubeidec.exe" --launcher.suppressErrors -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -data <임시 워크스페이스> -import <프로젝트 폴더> -build all
```

서명: `STM32_SigningTool_CLI.exe -bin <app>.bin -nk -of 0x80000000 -t fsbl -o <app>-trusted.bin -hv 2.3 -align` (CubeProgrammer 2.21 이상은 `-align` 필요). 굽기는 사용자가 한다.

플래시 부팅(M6): 앱 하나를 FSBL로 서명해 외부 NOR 0x70000000에 굽는다(부트 ROM이 AXISRAM2로 복사, 한도 511 KB). `tools/flash.ps1`(기본 Debug, `-Config Release`, `-NoFlash`)이 서명 + 굽기를 한다. 굽기 전 BOOT1 1-3(개발 모드, 스위치 핀 순서는 2-1-3), 굽고 나서 BOOT0·BOOT1 1-2로 바꾸고 전원을 다시 넣는다.

## 현재 상태
- 보드 MB1939-N6570-C02, 카메라 MB1854B, USB1 = CN18.
- M1: 예제 3개 모두 Windows PC에서 동작 확인. 태블릿 UVC 뷰어 확인과 기종·앱 이름 기록(PRD 10장)은 사용자가 나중에 한다.
- **M2 완료**(2026-11-13): `fw/`(CubeIDE 프로젝트 `STM32N6_UVC`)의 CDC 에코 + UVC 고정 MJPEG composite가 Windows에서 동시 동작 확인.
- **M3 완료**(2026-11-13): IMX335 → DCMIPP/ISP → VENC JPEG → 프레임 큐 → UVC 1280×720 30fps가 Windows 카메라 앱·VLC에서 동작(USB DMA, HS 1024 B×3). 영상 중 CDC 에코·앱 재시작 OK. 장치 내부 지연 평균 약 57 ms(목표 50 ms)는 1-B H.264(M4)에서 다시 확인하기로 함. 태블릿 확인은 나중에. 참고 코드 태그 `m3-mjpeg-ref`. 5초마다 USART1에 `VIDEO:`/`UVC:`/`LAT` 통계.
- **M4 계획**(2026-11-13): `docs/m4-plan.md`(M4-1 참고 코드 확보 → M4-2 H.264 인코더 → M4-3 frame-based UVC → M4-4 UVC 1.5 → M4-5 태블릿 → M4-6 지연·화질 → M4-7 확정·MJPEG 삭제).
- **M4 진행**(2026-11-13): M4-2 H.264 인코더(VideoEncoder h264 소스, Debug에서도 이 폴더만 -Os), M4-3 UVC에 MJPEG(형식 1) + H.264 frame-based(형식 2), Commit 형식으로 인코더 전환, M4-5 갤럭시 노트20(Android 13) + ShenYao USB Camera 12.3.2에서 MJPEG·H.264 모두 재생 → M4-4(UVC 1.5)는 생략. H.264 장치 내부 지연 평균 약 51 ms / 최대 약 68 ms(IDR). 다음은 M4-6 지연·화질. Windows 카메라 앱은 MJPEG를 고르므로 H.264 확인은 ffmpeg `-vcodec h264 ... -c copy -copyinkf`로 녹화.
- **M4-6/M4-7**(2026-11-13): 1차(HCLK 200 MHz, UVC zero-copy·채우기 스레드)로 USB 구간 해결, 장치 내부 평균 49 ms / 최대 52 ms. 2차로 DCMIPP–VENC 하드웨어 핸드셰이크(NV12 64줄 줄 버퍼, AXISRAM2 아래쪽 `.linebuf_bss`)와 MJPEG 영구 삭제(UVC 형식은 H.264 frame-based 하나). 보드에서 장치 내부 평균 27.6 ms / 최대 31 ms 확인.
- **노트20 연결 수정**(2026-11-14): PD 스택의 Error Recovery 요청은 무시(`usbpd_dpm_user.c`, 보드는 PD 계약 불필요), CC detach로 USB를 끄지 않음, `VDD_VALUE` 1800(ADC 기준 1.8 V, VBUS 측정용), 카메라 초기화 최대 3회 재시도. 실행 후 연결·연결한 채 실행·다시 꽂기 모두 영상 확인. 자세한 내용은 `docs/progress.md`.
- **M4 정리**(2026-11-14): PC ffplay H.264 + CDC 에코 동시 확인. 화질 튜닝(보랏빛, AWB)과 전체 지연(노트20 약 170 ms, 폰 앱 버퍼 추정) 후속은 나중에. PC 확인은 `ffplay -fflags nobuffer -flags low_delay -framedrop -f dshow -vcodec h264 -video_size 1280x720 -framerate 30 -rtbufsize 64M -i video="STM32N6 UVC CDC"`.
- **M5 완료**(2026-11-14): CDC CLI(PRD 7장 명령 전부)를 ffplay 영상 재생 중 보드에서 확인. CDC data IN Tx FIFO가 256 B라 CLI 출력은 256 B 이하로 나눠 보낸다(`ux_device_cdc_acm.c`). 다음은 M6(플래시 부팅, 재연결 100회, 24시간).
- HardFault 시 원인 레지스터·PC·스레드 이름이 USART1로 출력된다. PC는 `arm-none-eabi-addr2line -f -e fw/Debug/STM32N6_UVC.elf <PC>`로 찾는다. Debug(-O0)는 스택을 많이 쓰니 스레드 스택은 넉넉히 잡는다.

## 펌웨어 구조 (fw/)
- CubeN6 V1.4.1 `USBPD_SNK_Ux_Device_HID_CDC_ACM`에서 출발(ThreadX + USBX + USBPD). UVC는 `Ux_Device_Video`에서 가져옴.
- `.ioc` 없음. CubeMX로 다시 생성하지 말고 코드를 직접 고친다.
- 개발 모드 전용 링커 배치(AXISRAM2, ROM 383K / RAM 128K, 끝 64K는 MPU 비캐시: USBX 풀(USB DMA 버퍼)·hpcd·MSP 스택). 플래시 부팅 구조는 M6에서.
- 디버그 로그: printf → USART1(ST-LINK 가상 COM, 115200).
- 영상: `fw/Video/`(video_app: 카메라·인코딩·ISP 스레드와 `MX_DCMIPP_Init` 재정의, h264_enc, vframe_queue, video_cfg). 큰 버퍼는 `IN_PSRAM`(링커 `.psram_bss`, 0x90000000, MPU 비캐시). PSRAM은 `main.c`의 `PSRAM_Init()`이 ThreadX 시작 전에 켠다.
- CLI(M5): `fw/App/`(settings: 설정 항목 테이블, cli: 줄 편집·명령, app_log, app_version). CDC 스레드는 `ux_device_cdc_acm.c`의 `usbx_cdc_acm_cli_thread_entry`. 설정 항목은 `settings.c` 테이블에 한 줄 추가하면 명령·help가 생긴다. 비트레이트·GOP 변경은 영상 스레드가 인코더를 다시 만들어 반영(VENC는 HRD 켠 채 실행 중 율 제어 변경 불가).
- HSLV OTP 퓨즈는 읽기만 한다(ST 예제 FSBL은 굽지만 되돌릴 수 없으므로 굽지 않는다).
