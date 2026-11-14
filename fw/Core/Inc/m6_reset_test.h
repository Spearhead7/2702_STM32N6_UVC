/* M6 진단: 리셋 뒤 부트 ROM이 다시 부팅하는지 앱 초기화 단계별로 본다.
   0 = 사용 안 함(정상 펌웨어). 1~4 = 그 단계 직후 약 2초 뒤 IWDG로 스스로 리셋을 반복한다.
   1: main 진입 직후, 2: SystemClock_Config 뒤, 3: 주변장치 초기화(GPIO~USBPD) 뒤, 4: PSRAM 초기화 뒤.
   5: PSRAM IO 전압 범위(VDDIO2 1.8 V)만, 6: PSRAM 칩 설정 뒤(memory-mapped 전),
   7: PSRAM 초기화 뒤 칩 Global Reset 후, 8: 7 + VDDIO2 범위 3.3 V로 되돌린 후.
   시험 이미지는 이 값을 바꿔 빌드한 뒤 tools/flash.ps1 -Image로 굽는다. */
#ifndef M6_RESET_TEST
#define M6_RESET_TEST 0
#endif
