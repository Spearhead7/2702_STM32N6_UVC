# STM32N6 UVC — Claude Code 작업 안내

이 저장소는 STM32N6570-DK로 만드는 1CH DVR의 **1단계: USB UVC 카메라 + USB CDC(CLI)** 펌웨어다.
요구사항 전체는 `docs/prd.md`에 있다. 작업 전에 반드시 읽고, 결정이 바뀌면 PRD도 함께 고친다.

## 작업 방식
- 사용자와는 **한국어**로 대화한다. 사용자는 Linux를 모르므로 **Windows 기준**으로 안내한다.
- 개발 도구: STM32CubeIDE(+ STM32CubeMX), STM32CubeProgrammer, Tera Term, Windows 카메라 앱·VLC.
- 마일스톤(M1~M6) 단위로 진행하고, 각 마일스톤의 완료 기준(PRD 9장)을 사용자와 확인한 뒤 다음으로 넘어간다.
- 보드에 굽기, 실물 확인(영상이 나오는지, 장치 관리자 인식 등)은 사용자가 해야 하는 단계다. 무엇을 보고 알려 달라고 할지 구체적으로 요청한다.
- ST 패키지(STM32CubeN6, x-cube-n6-*)는 저장소에 복사하지 않는다. 필요한 소스만 가져오고, 출처와 버전을 커밋 메시지나 README에 적는다.
- 빌드 산출물(Debug/, Release/ 등)은 커밋하지 않는다(.gitignore 참고).

## 확정된 결정 (요약, 자세한 내용은 docs/prd.md)
- 영상: 1280×720 30fps. **1-A MJPEG로 먼저 영상 확인 → 1-B H.264로 전환·확정**. H.264가 태블릿에서 확인되면 MJPEG 디스크립터·코드·CLI 명령은 영구 삭제.
- 인코딩: N6 하드웨어 인코더 VENC. H.264는 I/P만(B-프레임 없음), CBR 4 Mbps, IDR 1초 주기, 스트림 시작 시 SPS/PPS + IDR 먼저.
- USB: USB OTG HS 하나에 CDC-ACM(IF0-1) + UVC(IF2-3) composite, IAD 사용. 예제 UVC 디스크립터의 VC/VS 인터페이스 번호 하드코딩(0/1)을 composite 번호로 고쳐야 한다.
- CDC: Tera Term용 대화형 CLI. 프롬프트 `n6uvc> `, 에코, `help`, 마지막 줄은 항상 `OK` 또는 `ERROR: <이유>`. 명령 목록은 PRD 7장.
- 지연: 전체 100 ms 이하, 장치 내부(캡처 시작 → USB 전송 완료) 50 ms 이하.
- 실행: 개발 중에는 개발 모드(디버거로 내부 SRAM에 올려 실행). 플래시 부팅(FSBL + 앱을 SRAM으로 복사)은 M6에서.
- 큰 버퍼(프레임, VENC 참조 프레임)는 외부 PSRAM. 내부 SRAM은 코드·스택·USB·라인 버퍼용.
- 호스트: 개발은 Windows PC, 각 단계 최종 확인은 Android 태블릿의 UVC 뷰어 앱.

## 2단계 이후를 위한 설계 제약 (지금부터 지킨다)
- 카메라·ISP·인코더를 UVC에 묶지 않는다. 인코더 출력은 프레임 큐로 내보내고 UVC는 소비자 중 하나(2단계에서 SD 녹화가 추가됨).
- 해상도·fps는 설정값으로 둔다(2단계 녹화는 1080p15).
- 녹화는 BBox 없는 원본. 표시 경로와 녹화 경로를 분리할 수 있게 둔다.
- CLI 명령 처리와 '설정 항목 테이블'을 분리한다(4단계 마우스 메뉴가 같은 테이블을 쓴다).
- DCMIPP의 나머지 파이프(AI용, 화면용)와 PSRAM 영역은 비워 둔다.

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

## 현재 상태
- PRD 확정, 펌웨어 코드는 아직 없음.
- **M1 진행 중**: 예제 준비·빌드·서명 완료, 사용자가 `docs/m1-guide.md`에 따라 보드에서 확인할 차례. 결과를 받으면 `docs/progress.md`와 PRD 10장(보드 리비전, 태블릿 앱)에 반영한다.
