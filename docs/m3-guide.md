# M3 실행 가이드 — 실제 카메라 MJPEG 720p30 (Windows)

M3 완료 기준(PRD 9장): Windows 카메라 앱과 태블릿 뷰어에서 **1280×720 30fps** 실시간 영상이 끊김 없이 나온다. 이번에는 Windows PC까지 확인하고, 태블릿은 M1처럼 나중에 확인한다.

펌웨어는 M2와 같은 `fw\` 프로젝트(`STM32N6_UVC`)다. Claude가 Debug/Release 모두 0 errors로 빌드까지 확인했다(2026-11-13, STM32CubeIDE 1.18.0). 경고 2개는 ST 인코더 라이브러리 소스 안의 것이라 그대로 둔다. 이 문서는 **보드에서 실행하고 확인하는 단계**만 다룬다.

## 이번 펌웨어가 하는 일

| 항목 | 내용 |
|---|---|
| 영상 경로 | IMX335 → DCMIPP(ISP 자동 노출·화이트밸런스, 16:9 크롭, 1280×720 축소) → VENC 하드웨어 JPEG → 프레임 큐 → UVC |
| UVC | MJPEG 1280×720 30fps 한 가지. isochronous 1024 B × 3/마이크로프레임(high-bandwidth), USB DMA |
| CDC | M2와 같은 에코(M5에서 CLI로 바뀜) |
| 카메라 동작 | USB 연결과 상관없이 부팅하면 바로 카메라·인코더가 돈다. 카메라 앱을 열면 그 순간의 최신 프레임부터 보낸다 |
| 외부 PSRAM | 32 MB 중 약 14 MB를 영상 버퍼로 쓴다. 부팅 때 간단한 읽기/쓰기 확인을 한다 |
| 디버그 로그 | ST-LINK 가상 COM(USART1, 115200)에 5초마다 영상 통계 한 줄 |

## 0. 준비

- 보드 연결과 부트 스위치는 M2와 같다(ST-LINK CN6 + USB1 CN18, 개발 모드 BOOT1 **2-3**).
- **카메라 모듈 MB1854B를 꽂는다.** M1의 카메라 예제 때와 같은 상태면 된다. 꽂거나 뺄 때는 전원을 끈다.
- 렌즈 캡이 있으면 연다. 밝은 곳을 향하게 둔다.

## 1. CubeIDE로 실행

1. CubeIDE에서 `STM32N6_UVC` 프로젝트를 오른쪽 클릭 → **Refresh**(F5). 이번에 폴더와 파일이 많이 늘었다.
2. **Project → Clean...** → `STM32N6_UVC` → Clean. 그다음 망치 아이콘(Build, Debug 구성). Console 끝에 `0 errors`가 나오면 된다(`2 warnings`는 정상).
3. **Tera Term ①**(ST-LINK COM, 115200)을 연다.
4. **Debug As → STM32 C/C++ Application**(또는 M2 때 만든 디버그 구성) → main에서 멈추면 **Resume(F8)**.
5. Tera Term ①에 아래처럼 나와야 한다(숫자는 달라도 된다).

   ```
   === STM32N6 UVC (M3: camera MJPEG 720p30 + CDC echo) ===
   PSRAM: VDDIO3 HSLV fuse set
   PSRAM: OK (XSPI1 200 MHz)
   VIDEO: 1280x720 30 fps, JPEG q6
   VIDEO: camera started
   USB: start
   CDC: activate
   VIDEO: cap 30.0 fps, enc 30.0 fps, jpeg last 120 KB max 135 KB, enc 9 ms (max 12), skip 0, noslot 0, recycle 140, err 0, pipeerr 0
   ```

   - `PSRAM: VDDIO3 HSLV fuse NOT set (using 100 MHz)`가 나와도 정상이다. 보드의 고속 IO 퓨즈가 꺼져 있어 PSRAM을 100 MHz로 쓴다는 뜻이다(퓨즈는 되돌릴 수 없어서 펌웨어가 굽지 않는다).
   - `VIDEO:` 통계 줄은 5초마다 나온다. `recycle`은 계속 늘어나는 게 정상이다(카메라 앱을 안 열면 인코딩한 프레임을 버리고 다시 쓴다).
6. USB1(CN18)을 PC에 연결한다(이미 꽂혀 있으면 뽑았다 다시 꽂는다).

## 2. 영상 확인

1. **Windows 카메라 앱**을 열고 이 장치(`STM32N6 UVC CDC` 또는 `USB 비디오 장치`)를 고른다.
2. 확인할 것
   - 실제 카메라 영상이 나오는지, 움직임이 부드러운지(손을 흔들어 본다)
   - 색이 자연스러운지(처음 몇 초는 밝기·색이 맞춰지면서 변하는 게 정상)
   - 화면이 위아래나 옆으로 **찌그러져 보이지 않는지**(동그란 물체를 비춰 본다)
   - 카메라 앱 설정(톱니바퀴) → 비디오 품질에 **1280×720 / 16:9** 같은 값이 보이면 그 값
3. **VLC로 fps 확인**: 카메라 앱을 닫고 VLC → 미디어 → 캡처 장치 열기 → 비디오 장치 이름에서 이 장치 → 재생. 재생 중에 **도구 → 코덱 정보**를 열어 **해상도**와 **프레임 속도**, 코덱(`MJPG`)을 본다.
4. 영상이 나오는 동안 Tera Term ①의 `VIDEO:` 줄을 하나 복사해 둔다.
5. M2처럼 영상 중에 **Tera Term ②(USB 직렬 장치)** 에코가 되는지, 카메라 앱을 닫았다 다시 열어도 영상이 나오는지 본다.

## 3. 알려 줄 것

아래를 채워서 알려 주면 된다.

```
1) Tera Term ① 부팅 로그 전체(배너부터 첫 VIDEO: 줄까지 복사)
2) 영상: 카메라 앱에서 실제 영상이 나오나? 부드러운가, 끊기나? 색은?
3) 찌그러짐: 동그란 물체가 동그랗게 보이나?
4) VLC 코덱 정보: 해상도 / 프레임 속도 / 코덱
5) 영상 재생 중 VIDEO: 줄 하나
6) 동시 동작: 영상 중 에코 되나? 카메라 앱 다시 열어도 되나?
```

## 문제가 생기면

| 증상 | 확인할 것 |
|---|---|
| `VIDEO: init failed at step -1 (camera init ...)` | 카메라 모듈 연결(전원 끄고 다시 꽂기). 로그 바로 위의 `BSP_CAMERA_Init ret` 값도 알려 준다 |
| `PSRAM: TEST FAILED` 또는 `PSRAM: init failed` | 그 줄과 앞뒤 로그를 그대로 보내 준다 |
| `VIDEO: no frame from camera for 500 ms` | 카메라 모듈 연결. 로그 전체를 보내 준다 |
| HardFault 덤프(`*** HardFault ***`로 시작하는 줄들) | 덤프 전체를 보내 준다. M2 때처럼 Claude가 PC로 위치를 찾는다 |
| 영상은 나오는데 뚝뚝 끊김 | `VIDEO:` 줄의 `enc ... fps`, `jpeg last ... KB` 값을 알려 준다. JPEG이 크면 USB 대역폭(약 270 KB/프레임)을 넘어 프레임이 줄어든다 |
| 영상이 녹색·분홍 등 이상한 색 | 화면 사진이나 캡처를 보내 준다(YUV 바이트 순서 문제일 수 있음) |

## 참고: 펌웨어 구성

- 카메라·DCMIPP·VENC 설정: CubeN6 V1.4.1 `Applications/VENC/VENC_USB`(H.264 예제)에서 가져오고, 인코더만 JPEG 모드로 바꿨다(사용법은 `Applications/VENC/VENC_JPEG_Encoding`).
- 새 코드는 `fw/Video/`: `video_app.c`(카메라·인코딩 스레드, DCMIPP 설정), `jpeg_enc.c`(VENC JPEG), `vframe_queue.c`(프레임 큐), `video_cfg.h`(해상도·fps·품질 설정값).
- 자세한 변경 내용은 `docs/progress.md`의 M3 항목.
