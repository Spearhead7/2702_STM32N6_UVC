# M4 계획: H.264 전환과 페이로드 방식 확정 (1-B)

작성 2026-11-13. 요구사항 원문은 `docs/prd.md`(F-03, F-04, F-09, 4장 표, 9장 M4, 10장 리스크).

## 1. 목표와 완료 기준

- 인코더를 VENC H.264로 바꿔 1280×720 30fps를 UVC로 보낸다.
- 페이로드 방식 3가지 중 **태블릿 뷰어(필수)와 Windows(가능하면)에서 재생되는 방식 하나를 확정**한다.
- 지연: 전체(glass-to-glass) 100 ms 이하, 장치 내부(`LAT total`) 50 ms 이하. M3 MJPEG는 평균 약 57 ms였고, 50 ms 확인은 여기로 넘겨 왔다.
- 위가 모두 확인되면 MJPEG 디스크립터·코드·CLI 명령을 삭제한다. 참고 코드는 태그 `m3-mjpeg-ref`로 보존되어 있다.

## 2. 출발점 (M3 끝)

| 항목 | 상태 |
|---|---|
| 영상 경로 | IMX335 → DCMIPP PIPE1(ISP, 16:9 크롭, 1280×720 YUYV) → VENC JPEG → 프레임 큐(4×512 KB) → UVC |
| USB | OTG HS DMA, isochronous 1024 B×3/마이크로프레임, CDC+UVC composite |
| 측정 | 5초마다 `VIDEO:` / `UVC:` / `LAT`(cap 29.1, enc 11.9, usb 16.3, total 57.4 ms) |
| 인코더 소스 | `Middlewares/Third_Party/VideoEncoder`에 common과 jpeg만 있다. **H.264 소스(source/h264)는 아직 없다** |
| 참고 예제 로컬 사본 | `ref/x-cube-n6-ai-h264-usb-uvc`(v2.2.1, M4-1에서 받음), CubeN6 V1.4.1 VENC_USB는 CubeMX 저장소(`%USERPROFILE%\STM32Cube\Repository\STM32Cube_FW_N6_V1.4.1`)에 있음 |

## 3. 정해야 할 것

| 항목 | 후보 | 정하는 시점 |
|---|---|---|
| 페이로드 방식 | ① UVC 1.1 frame-based H.264, ② UVC 1.5 H.264, ③ MPEG-2 TS | M4-5, M4-6 시험 결과로 |
| profile | Baseline 또는 Main (Level 3.1) | M4-2. Baseline부터 시작(호환성이 가장 넓음) |
| 비트레이트 | 기본 CBR 4 Mbps, 2~8 Mbps 범위에서 화질 보고 결정 | M4-6 |
| 태블릿 기종·뷰어 앱 | 예: USB Camera(ShenYao, H.264 UVC 지원 표기) | **사용자가 M4-6 전에 정해 둔다** |

시험 순서는 ① → ② → ③으로 한다. ①은 디스크립터만 바꾸면 되고, ③은 장치에서 TS로 포장하는 코드가 더 필요하다. 둘 중 하나가 태블릿과 Windows에서 되면 ③은 하지 않는다.

## 4. 단계

각 단계 끝에 사용자 확인이 있다. 굽기·화면 확인·로그 전달은 사용자가 한다.

### M4-1 참고 코드 확보와 분석 (Claude)

- x-cube-n6-ai-h264-usb-uvc(GitHub)와 STM32CubeN6 V1.4.1 VENC_USB를 `ref/`에 받아 둔다(저장소에는 넣지 않는다).
- 확인할 것: H.264 인코더 설정(CBR, GOP, 저지연 옵션, SPS/PPS 처리), 사용한 UVC 페이로드 방식과 디스크립터, VENC 메모리 요구량, DCMIPP 출력 형식(NV12 또는 YUYV).
- 결과를 `docs/progress.md`에 정리한다. 사용자 확인 없음.

### M4-2 H.264 인코더 모듈 (Claude 코드 → 사용자 로그 확인)

M4-1 결과: 주 참고는 CubeN6 VENC_USB(ThreadX + USBX, YUYV 입력, 우리 구조와 같음). 비교표는 `docs/progress.md` M4-1.

- CubeN6 V1.4.1에서 VideoEncoder H.264 소스만 가져온다(출처·버전은 커밋 메시지에 남긴다).
- `Video/Src/h264_enc.c`를 `jpeg_enc.c`와 같은 모양(init / frame / deinit)으로 만든다.
  - Baseline, Level 3.1, I/P만, CBR 4 Mbps, IDR 30프레임 주기, lookahead 없음, 작은 비트레이트 버퍼.
  - 출력은 Annex-B(00 00 00 01 시작 코드). IDR 프레임 앞에 SPS/PPS를 붙인다.
  - `h264_enc_force_idr()`: 다음 프레임을 IDR로(스트림 시작, 나중의 `enc idr` 명령).
- 입력은 먼저 지금의 YUYV를 그대로 쓴다(VENC가 YUV422 인터리브 입력 지원). DCMIPP를 NV12로 바꾸는 것은 메모리나 성능이 문제될 때만 한다.
- VENC 작업 메모리(EWL 풀)를 256 KB에서 H.264 참조 프레임을 담을 크기(수 MB, PSRAM)로 키운다.
- 영상 형식은 설정값(`video_cfg`)으로 둔다. MJPEG와 H.264를 빌드를 다시 하지 않고 바꿀 수 있게 하고, MJPEG는 확정 전까지 남겨 둔다.
- **사용자 확인**: UVC는 아직 MJPEG인 상태에서 H.264 인코딩만 돌린다. `VIDEO:` 줄에 H.264 fps, 평균·IDR 프레임 크기, 인코딩 시간이 나오는지, 오류가 0인지 본다.

### M4-3 UVC H.264 스트리밍 — ① frame-based (Claude 코드 → 사용자 확인)

- VS 인터페이스에 **MJPEG(포맷 1)과 H.264 frame-based(포맷 2)를 함께** 넣는다. 호스트가 고른 포맷(Commit의 bFormatIndex)에 따라 인코더를 바꾼다. 비교하기 쉽고, H.264가 안 되면 MJPEG로 바로 돌아갈 수 있다.
- 스트림을 열면 IDR 강제 → 첫 프레임은 SPS/PPS+IDR. UVC 쪽은 키프레임이 올 때까지 기다렸다가 보낸다.
- 프레임 큐의 `key` 플래그와 형식 필드를 쓴다. 큐 슬롯 512 KB는 IDR(4 Mbps에서 대략 수십~100 KB대)에 충분하다.
- dwMaxVideoFrameBufferSize는 IDR 최대 크기 기준으로 잡는다.
- **사용자 확인(Windows)**:
  1. 장치 관리자에 카메라가 그대로 잡히는지
  2. 아래 명령으로 포맷 목록에 h264가 보이는지(ffmpeg는 ShareX에 들어 있는 것을 쓴다)
     `"C:\Program Files\ShareX\ffmpeg.exe" -f dshow -list_options true -i video="<카메라 이름>"`
  3. Windows 카메라 앱, VLC(캡처 장치 → 고급 옵션에서 H.264 선택, 캐싱 0 ms)에서 영상이 나오는지
  4. `UVC:`, `LAT` 줄

### M4-4 ② UVC 1.5 H.264 (①이 Windows에서 안 될 때)

- bcdUVC 0x0150, UVC 1.5 H.264 포맷·프레임 디스크립터로 바꾼다. ST 커뮤니티 답변은 Windows 호환을 위해 UVC 1.5 또는 MPEG-2 TS를 권했다.
- 사용자 확인은 M4-3과 같다.

### M4-5 태블릿 시험 (사용자)

- 정해 둔 태블릿 + UVC 뷰어 앱으로 MJPEG와 H.264를 각각 연다. 기종과 앱 이름을 PRD 10장에 적는다.
- ①, ②가 모두 안 되면 ③ MPEG-2 TS를 만든다(M4-5b, Claude 코드). 1~2일 더 걸린다.

### M4-6 지연·화질 측정 (사용자 측정, Claude 정리)

- 장치 내부: `LAT total` 50 ms 이하인지 본다. H.264는 프레임이 평균 약 17 KB라 usb 구간이 MJPEG 16 ms보다 크게 줄 것으로 본다.
- 전체: 스톱워치(ms 표시)를 카메라에 비추고, 그 스톱워치와 화면(Windows 카메라 앱, 태블릿 앱)을 함께 사진으로 찍어 차이를 본다. 5회 평균.
- 화질: 4 Mbps에서 움직임이 있을 때 깨짐이 없는지 본다. 필요하면 비트레이트를 조정한다.
- 50 ms를 넘으면 USB 최적화를 한다(M3에서 미룬 것): USBX zero-copy(`UX_DEVICE_ENDPOINT_BUFFER_OWNER=1`), 페이로드 채우기 스레드 분리, 프레임 슬롯 캐시 영역화. 필요하면 DCMIPP–VENC 하드웨어 핸드셰이크(프레임이 다 들어오기 전에 인코딩 시작)도 검토한다.

### M4-7 확정과 정리 (사용자 결정 → Claude)

- 사용자와 페이로드 방식·profile·비트레이트를 확정하고 PRD(F-03, 10장 미결 사항)를 갱신한다.
- MJPEG 디스크립터·jpeg_enc·`enc jpegq` 관련 코드를 삭제한다. 참고 코드는 태그 `m3-mjpeg-ref`.
- 메모리 재배치: 캡처 버퍼·VENC 버퍼를 H.264 기준으로 다시 잡는다(2단계 1080p15 녹화 여유를 남긴다).
- 커밋·푸시는 사용자 요청 시.

## 5. 2단계 대비 (지금부터 지킬 것)

- 인코더 출력은 계속 프레임 큐로만 내보낸다. UVC는 소비자 중 하나다(2단계 SD 녹화가 같은 큐를 쓴다).
- 해상도·fps·비트레이트·GOP는 설정 테이블 값으로 둔다(M5 CLI와 4단계 메뉴가 같은 테이블을 쓴다).
- IDR 강제는 소비자가 요청할 수 있는 API로 둔다(UVC 스트림 시작, 녹화 파일 분할).

## 6. 리스크

| 리스크 | 대응 |
|---|---|
| Windows 카메라 앱·VLC가 H.264 UVC를 재생하지 못함 | ① → ② → ③ 순서로 시험. MJPEG를 같은 장치에 남겨 두어 확인 도구는 유지 |
| 태블릿 앱이 H.264 UVC를 재생하지 못함 | 다른 앱 시험, ③ TS 시험. 그래도 안 되면 사용자와 MJPEG 유지 여부를 다시 결정 |
| H.264 VENC 메모리가 PSRAM 배치와 충돌 | 캡처 버퍼(지금 3×4 MB)를 720p 크기로 줄여 공간 확보 |
| IDR 프레임이 커서 그 프레임만 USB 지연이 커짐 | CBR 버퍼와 IDR 크기 상한 조정, LAT 최대값으로 확인 |
| 스트림 중간 접속 시 디코더가 첫 IDR까지 검은 화면 | 스트림 시작 시 IDR 강제(F-04) |

## 7. 사용자가 미리 준비할 것

- 태블릿 기종과 UVC 뷰어 앱(H.264 UVC 지원 표기가 있는 앱)을 정해 설치해 둔다. M4-5에서 쓴다.
- 스톱워치 앱(ms 표시)이 있는 휴대폰. M4-6에서 쓴다.
