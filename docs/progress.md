# 진행 기록

## M1 환경 구축과 예제 확인 — 진행 중

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

**다음 할 일 (사용자)**

- `docs/m1-guide.md`에 따라 세 예제를 보드에서 실행하고, 4장의 확인 항목을 알려 준다.
