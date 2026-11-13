# M1 실행 가이드 — 예제 3개 확인 (Windows)

M1 완료 기준(PRD 9장): 세 예제가 Windows PC에서 동작하고, 태블릿 뷰어에서도 MJPEG 예제 영상이 나온다.

빌드와 서명은 Claude가 미리 해 두었다(2026-11-13, STM32CubeIDE 1.18.0, STM32CubeProgrammer 2.23.0). 이 문서는 **보드에 굽고 확인하는 단계**만 다룬다.

## 0. 준비

### 예제 위치 (저장소 밖)

| 이름 | 위치 | 상태 |
|---|---|---|
| ① Ux_Device_Video (MJPEG UVC) | `D:\JOB2027\2702_STM32N6_UVC\ref\STM32Cube_FW_N6_V1.4.1\Projects\STM32N6570-DK\Applications\USBX\Ux_Device_Video\STM32CubeIDE` | 빌드·서명 완료 |
| ② USBPD_SNK_Ux_Device_HID_CDC_ACM (CDC) | `D:\JOB2027\2702_STM32N6_UVC\ref\STM32Cube_FW_N6_V1.4.1\Projects\STM32N6570-DK\Applications\USBPD\USBPD_SNK_Ux_Device_HID_CDC_ACM\STM32CubeIDE\FSBL` | 빌드 완료 |
| ③ x-cube-n6-camera-capture v2.0.1 | `D:\JOB2027\2702_STM32N6_UVC\ref\x-cube-n6-camera-capture` | 소스만 받음(빌드는 직접) |

- ②는 PRD에 적힌 `Ux_Device_CDC_ACM` 대신 쓴다. CubeN6 V1.4.1에서 `Ux_Device_CDC_ACM`은 NUCLEO 보드용만 있고, DK용으로는 이 예제가 CDC-ACM을 담고 있다. HID+CDC를 IAD로 묶은 composite라서 M2의 참고 자료로도 쓸 수 있다.
- ③을 Claude가 빌드하는 것은 권한 정책(외부에서 받은 코드 실행)으로 막혀서 직접 빌드하거나 미리 빌드된 hex를 쓴다.

### 보드 연결과 부트 스위치

- **전원·디버그**: ST-LINK 포트(CN6) ↔ PC, **USB-C to USB-C 케이블**(전력이 부족하지 않도록).
- **USB 데이터**: USB1 포트 ↔ PC. camera-capture README는 DK의 USB1을 **CN18**로, CDC 예제 README는 **CN8**로 적고 있다. 보드 실크 인쇄를 보고 실제로 어느 커넥터를 썼는지 알려 주세요.
- **카메라 모듈**: 리본 케이블은 반드시 **전원을 끈 상태에서** 연결한다.

| 모드 | BOOT0 | BOOT1 | 용도 |
|---|---|---|---|
| 개발 모드 | 상관없음 | **2-3** | 디버거로 SRAM에 올려 실행, 외부 플래시에 굽기 |
| 플래시 부팅 | **1-2** | **1-2** | 외부 플래시에서 부팅 |

스위치를 바꾼 뒤에는 **전원을 껐다 켠다**(USB 케이블을 뽑았다 꽂기).

### PowerShell에서 쓸 변수

아래 명령은 PowerShell 창에 그대로 붙여 넣으면 된다. 창을 새로 열 때마다 이 두 줄을 먼저 실행한다.

```powershell
$CLI = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
$EL  = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\ExternalLoader\MX66UW1G45G_STM32N6570-DK.stldr"
```

---

## 1. ① Ux_Device_Video — MJPEG UVC (카메라 없이 고정 영상)

이 예제는 FSBL(1단계 부트로더)이 외부 플래시의 앱을 SRAM으로 복사해 실행하는 구조다. 디버거 없이 확인할 수 있도록 **FSBL과 앱을 둘 다 플래시에 굽고 플래시 부팅**하는 방법으로 진행한다.

1. 부트 스위치를 **개발 모드**로 두고 전원을 넣는다.
2. 앱과 FSBL을 굽는다.

   ```powershell
   $V = "D:\JOB2027\2702_STM32N6_UVC\ref\STM32Cube_FW_N6_V1.4.1\Projects\STM32N6570-DK\Applications\USBX\Ux_Device_Video\STM32CubeIDE"
   & $CLI -c port=SWD mode=HOTPLUG -el $EL -hardRst -w "$V\Ux_Device_Video\Debug\Ux_Device_Video-trusted.bin" 0x70100000
   & $CLI -c port=SWD mode=HOTPLUG -el $EL -hardRst -w "$V\FSBL\Debug\FSBL-trusted.bin" 0x70000000
   ```

   각 명령의 끝에 `File download complete`가 나오면 성공이다.
3. 부트 스위치를 **플래시 부팅**으로 바꾸고 전원을 껐다 켠다.
4. USB1 포트를 PC에 연결한다.
5. **장치 관리자**의 "카메라" 또는 "이미징 장치"에 카메라가 나타나는지 본다.
6. **Windows 카메라 앱**이나 **VLC**(미디어 → 캡처 장치 열기)로 영상이 나오는지 본다. 320×236 MJPEG 고정 영상이다.
7. **태블릿 확인**: USB1 포트를 OTG 케이블로 태블릿에 연결하고 UVC 뷰어 앱에서 영상이 나오는지 본다. 사용한 앱 이름을 기록한다.

## 2. ② USBPD_SNK_Ux_Device_HID_CDC_ACM — CDC 가상 COM 포트

이 예제는 FSBL 하나로 된 프로젝트라서 개발 모드에서 CubeIDE로 바로 실행한다.

1. 부트 스위치를 **개발 모드**로 두고 전원을 껐다 켠다.
2. STM32CubeIDE를 열고 **File → Import → General → Existing Projects into Workspace**를 고른다.
   - Root directory: 위 표의 ② 위치(`...\STM32CubeIDE\FSBL`)
   - **Copy projects into workspace는 체크하지 않는다.**
3. 프로젝트를 선택하고 망치 아이콘(Build)으로 빌드한다(Debug 구성).
4. 프로젝트를 오른쪽 클릭 → **Debug As → STM32 C/C++ Application** → 기본값으로 OK. 디버거가 main에서 멈추면 **Resume(F8)**.
5. USB1 포트를 PC에 연결한다. 장치 관리자의 "포트(COM & LPT)"에 COM 포트가 **두 개** 보여야 한다.
   - `STMicroelectronics STLink Virtual COM Port (COMx)` — 보드의 UART1
   - `USB 직렬 장치 (COMy)` — 이번에 확인할 USB CDC
   - "휴먼 인터페이스 장치"에 HID 마우스도 하나 생긴다.
6. **Tera Term을 두 개** 열어 각각 COMx, COMy에 연결한다(115200, 8N1).
7. COMy(USB CDC) 창에 글자를 치면 COMx 창에 나오고, 반대도 되는지 본다.
8. (선택) 보드의 USER 버튼을 누르면 PC 마우스 커서가 조금씩 움직인다.

## 3. ③ x-cube-n6-camera-capture — 실제 카메라(IMX335 → ISP → UVC)

이 예제가 지원하는 해상도는 최대 800×480 YUV422(무압축)다. 720p는 M3에서 다룬다.

### 3-A. 미리 빌드된 hex로 빠르게 확인 (권장, 먼저)

1. 전원을 끈 상태에서 카메라 모듈을 연결한다.
2. 부트 스위치를 **개발 모드**로 두고 전원을 넣는다.
3. 굽는다.

   ```powershell
   & $CLI -c port=SWD mode=HOTPLUG -el $EL -hardRst -w "D:\JOB2027\2702_STM32N6_UVC\ref\x-cube-n6-camera-capture\Binary\STM32N6570-DK\x-cube-n6-camera-capture-rear-dk.hex"
   ```

   (`rear`는 일반 카메라 방향, `front`는 셀카 방향이다.)
4. 부트 스위치를 **플래시 부팅**으로 바꾸고 전원을 껐다 켠다.
5. USB1 포트를 PC에 연결하고 카메라 앱이나 VLC에서 실제 영상이 나오는지 본다. 앱에서 해상도를 바꿔 보고, 바꾼 뒤 오류가 나면 앱을 다시 켠다(README에 알려진 문제로 나와 있음).
6. (선택) Tera Term으로 STLink COM 포트(115200 8N1)를 열면 예제 로그를 볼 수 있다.

### 3-B. 소스로 빌드해서 실행

1. 부트 스위치를 **개발 모드**로 두고 전원을 껐다 켠다.
2. CubeIDE에서 **File → Import → Existing Projects into Workspace**, Root directory를 `D:\JOB2027\2702_STM32N6_UVC\ref\x-cube-n6-camera-capture\STM32CubeIDE\STM32N6570-DK`로 지정한다(복사는 체크하지 않음).
3. 빌드하고 **Debug As → STM32 C/C++ Application** → Resume(F8).
4. 3-A의 5번과 같이 확인한다.

README가 권장하는 CubeIDE 버전은 1.17.0이고 설치된 버전은 1.18.0이다. 빌드 에러가 나면 Console 창 내용을 통째로 붙여 넣어 주세요.

---

## 4. 확인 후 알려 줄 것

아래 내용을 채워서 알려 주시면 `docs/progress.md`와 PRD에 반영하고 M2로 넘어갑니다.

```text
[보드]
- 보드 뒷면 라벨의 리비전 (예: MB1939-N6570-C01):
- 카메라 모듈 라벨 (예: MB1854B):
- USB1로 쓴 커넥터 실크 이름 (CN18 / CN8 / 기타):

[① Ux_Device_Video]
- 장치 관리자 인식: 예 / 아니오
- Windows 카메라 앱 / VLC 영상: 나옴 / 안 나옴
- 태블릿 기종, UVC 뷰어 앱 이름:
- 태블릿 영상: 나옴 / 안 나옴

[② CDC]
- COM 포트 두 개 인식: 예 / 아니오
- 양방향 글자 전달: 됨 / 안 됨

[③ camera-capture]
- 사용한 방법: 3-A hex / 3-B 소스 빌드
- 영상: 나옴 / 안 나옴, 확인한 해상도:
- (가능하면) 태블릿에서 영상: 나옴 / 안 나옴

[문제]
- 오류 메시지, 화면 사진, Tera Term 로그 등
```
