# M2 실행 가이드 — CDC + UVC composite (Windows)

M2 완료 기준(PRD 9장): Windows에서 카메라와 COM 포트가 **동시에** 동작하고 CDC 에코 테스트가 통과한다.

펌웨어는 저장소의 `fw\` 폴더(CubeIDE 프로젝트 이름 `STM32N6_UVC`)다. Claude가 Debug/Release 모두 0 errors, 0 warnings로 빌드까지 확인했다(2026-11-13, STM32CubeIDE 1.18.0). 이 문서는 **보드에서 실행하고 확인하는 단계**만 다룬다.

## 이번 펌웨어가 하는 일

| 항목 | 내용 |
|---|---|
| USB 장치 | USB1(CN18), High-Speed, VID 0x0483 / **PID 0x5760**(개발용 임시값), 제품 이름 "STM32N6 UVC CDC" |
| 인터페이스 0-1 | CDC-ACM: 받은 글자를 그대로 돌려보냄(에코). M5에서 CLI로 바뀜 |
| 인터페이스 2-3 | UVC: 카메라 없이 고정 MJPEG 영상(ST 로고, 320×236, 5 fps). M3에서 실제 카메라로 바뀜 |
| 디버그 로그 | ST-LINK 가상 COM(USART1, 115200 8N1)에 부팅 메시지와 USB 연결 상태 출력 |

PID를 ST 예제와 다르게 둔 이유: Windows는 VID/PID별로 드라이버 정보를 기억해 둔다. M1의 예제(HID+CDC, Video)와 같은 PID를 쓰면 인터페이스 구성이 달라도 예전 정보를 써서 인식이 꼬일 수 있다.

## 0. 준비

- 보드 연결은 M1과 같다.
  - ST-LINK 포트(CN6) ↔ PC: 전원·디버그·디버그 로그
  - USB1(CN18) ↔ PC: 이번에 확인할 USB 장치
- 부트 스위치: **개발 모드**(BOOT1 **2-3**). 스위치를 바꿨으면 전원을 껐다 켠다.
- 카메라 모듈은 이번 단계에서 쓰지 않는다(꽂혀 있어도 상관없음).

## 1. CubeIDE로 실행

1. STM32CubeIDE에서 **File → Import → General → Existing Projects into Workspace**.
   - Root directory: `D:\JOB2027\2702_STM32N6_UVC\STM32N6_UVC\fw`
   - 프로젝트 `STM32N6_UVC`가 체크되어 있는지 확인.
   - **Copy projects into workspace는 체크하지 않는다.**
   - M1의 CDC 예제 프로젝트가 워크스페이스에 남아 있어도 된다. 헷갈리면 닫아 둔다(오른쪽 클릭 → Close Project).
2. `STM32N6_UVC`를 선택하고 망치 아이콘(Build)으로 빌드한다(Debug 구성). Console 끝에 `0 errors`가 나오면 된다.
3. **Tera Term ①**을 열어 `STMicroelectronics STLink Virtual COM Port (COMx)`에 연결한다(115200, 8N1). 디버그 로그용이다.
4. 프로젝트를 오른쪽 클릭 → **Debug As → STM32 C/C++ Application** → 기본값으로 OK. 디버거가 main에서 멈추면 **Resume(F8)**.
5. Tera Term ①에 아래 줄이 나와야 한다.

   ```
   === STM32N6 UVC (M2: CDC echo + UVC fixed MJPEG) ===
   ```

6. USB1(CN18) 케이블을 PC에 연결한다(이미 꽂혀 있었다면 뽑았다가 다시 꽂는다). Tera Term ①에 `USB: start`가 나오고, 잠시 뒤 `CDC: activate`가 나와야 한다.

## 2. 장치 관리자 확인

**장치 관리자**(시작 버튼 오른쪽 클릭 → 장치 관리자)에서 아래 두 가지가 **동시에** 보여야 한다.

| 위치 | 이름(예) |
|---|---|
| 포트(COM & LPT) | `USB 직렬 장치 (COMy)` — 이번 펌웨어의 CDC |
| 카메라 (또는 이미징 장치) | `STM32N6 UVC CDC` 또는 `USB 비디오 장치` |

포트 항목에는 ST-LINK의 COMx도 함께 보인다(전부 COM 포트 2개).

노란 느낌표가 있으면 그 장치를 더블클릭 → **자세히** 탭 → 속성 **하드웨어 ID**에서 첫 줄을 확인해 알려 준다. 정상이라면 아래처럼 나온다.
- CDC: `USB\VID_0483&PID_5760&MI_00`
- 카메라: `USB\VID_0483&PID_5760&MI_02`

## 3. CDC 에코 테스트

1. **Tera Term ②**를 새로 열어 `USB 직렬 장치 (COMy)`에 연결한다. 속도 값은 상관없다(USB라서 실제 속도와 무관).
2. 영문 글자와 숫자를 쳐 본다. 친 글자가 **한 번씩** 화면에 나와야 한다(보드가 돌려보낸 것).
   - 두 번씩 나오면 Tera Term의 로컬 에코가 켜진 것이다. **설정 → 터미널 → 로컬 에코** 체크를 끈다.
   - 아무것도 안 나오면 Tera Term ①의 로그를 확인한다.
3. 긴 문장을 붙여 넣어도(오른쪽 클릭 붙여 넣기) 그대로 돌아오는지 본다. 한글은 이번 단계에서 확인하지 않는다.

## 4. 카메라 영상 + 동시 동작

1. **Windows 카메라 앱**을 연다. 카메라가 여러 개면 오른쪽 위의 카메라 전환 버튼으로 이 장치를 고른다.
   - 카메라 앱에서 안 나오면 **VLC**: 미디어 → 캡처 장치 열기 → 비디오 장치 이름에서 `STM32N6 UVC CDC`(또는 USB 비디오 장치) 선택 → 재생.
2. M1 ① 예제와 같은 **ST 로고 영상**이 나와야 한다(320×236, 초당 5장이라 조금 뚝뚝 끊기는 것이 정상).
3. 영상이 나오는 상태에서 Tera Term ②에 글자를 쳐서 **에코가 계속 되는지** 본다. 이것이 M2의 핵심 확인이다.
4. 카메라 앱을 닫았다 다시 열어도 영상이 다시 나오는지 본다.

## 5. 알려 줄 것

아래를 그대로 채워서 알려 주면 된다.

```
1) Tera Term ① 로그: 배너 / USB: start / CDC: activate 가 나왔나 (그대로 복사해 붙이면 가장 좋음)
2) 장치 관리자: COM 포트(USB 직렬 장치) 보임?  카메라 보임?  노란 느낌표?
3) 에코: 친 글자가 한 번씩 돌아오나?
4) 영상: 카메라 앱(또는 VLC)에서 ST 로고 영상이 나오나? 해상도 표시가 보이면 그 값도
5) 동시 동작: 영상이 나오는 동안 에코도 되나?  카메라 앱을 다시 열어도 되나?
```

문제가 있으면 위 항목에 더해 장치 관리자 화면 캡처와 Tera Term ① 로그 전체를 보내 준다.

## 참고: 펌웨어 구성

- 출발점: CubeN6 V1.4.1 `USBPD_SNK_Ux_Device_HID_CDC_ACM`(HID 제거, UART 브리지를 에코로 교체).
- UVC 부분: CubeN6 V1.4.1 `Ux_Device_Video`의 디스크립터·스트림 코드와 고정 영상(`stream1.h`).
- 자세한 변경 내용은 `docs/progress.md`의 M2 항목.
