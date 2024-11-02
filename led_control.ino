// 부저와 LED 출력을 위한 필요한 라이브러리들을 포함시킴
#include <DHT>  // DHT 센서 관련 라이브러리
#include <Adafruit_NeoPixel.h>  // NeoPixel LED 스트립 관련 라이브러리
// 아두이노 보드에 따라 WiFi 라이브러리가 다르게 포함됨
#if defined(ARDUINO_PORTENTA_C33)
#include <WiFiC3.h>  // Portenta C33용 WiFi 라이브러리
#elif defined(ARDUINO_UNOWIFIR4)
#include <WiFiS3.h>  // Uno WiFi Rev2용 WiFi 라이브러리
#endif
#include <PubSubClient.h>  // MQTT 클라이언트를 위한 라이브러리
#include <ArduinoJson.h>  // JSON 데이터 처리를 위한 라이브러리
#ifdef __AVR__
#include <avr/power.h>  // AVR 아키텍처의 전력 관리 라이브러리
#endif

// LED 핀과 부저 핀의 핀 번호 정의
#define LedPin_1 3
#define LedPin_2 6
#define LedPin_3 5
#define LedPin_4 1
#define NUMPIXELS 10  // LED 스트립의 LED 개수
#define SoundPin_A 2  // 부저 핀 번호
#define SoundPin_B 4
#define SoundPin_C 7
#define SoundPin_D 8
int DELAYVAL = 200;  // LED 턴라이트의 딜레이 값
int num = 0;  // LED 턴라이트의 순환 카운터 변수
int firecontrol = 0;  // 제어 버튼의 상태를 나타내는 변수 (0: 자동, 1: 켜짐, 2: 꺼짐)
bool A_fire = false;  // A 감지기에서 화재 감지 여부를 나타내는 변수
bool B_fire = false;  // B 감지기에서 화재 감지 여부를 나타내는 변수
bool C_fire = false;  // C 감지기에서 화재 감지 여부를 나타내는 변수
bool D_fire = false;  // D 감지기에서 화재 감지 여부를 나타내는 변수

// NeoPixel 객체 생성
Adafruit_NeoPixel pixels(NUMPIXELS, LedPin_1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixels2(6, LedPin_2, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixels3(NUMPIXELS, LedPin_3, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixels4(9, LedPin_4, NEO_GRB + NEO_KHZ800);

// WiFi 설정
const char* ssid = "JSW iPhone14 Pro";  // WiFi SSID
const char* password = "0000001151";   // WiFi 비밀번호

// MQTT 설정
const char* mqttServer = "172.20.10.14";  // MQTT 서버 주소
const int mqttPort = 1883;  // MQTT 포트 번호
WiFiClient espClient;  // ESP8266 클라이언트 객체 생성
PubSubClient client(espClient);  // MQTT 클라이언트 객체 생성

unsigned long lastReconnectAttempt = 0;  // MQTT 재연결 시도 시간 기록
unsigned long previousMillis = 0;  // 이전 루프 실행 시간 기록

void setup_wifi() {
  Serial.begin(9600);  // 시리얼 통신 시작
  Serial.println();  // 빈 줄 출력
  Serial.print("Connecting to ");  // 연결 중인 WiFi 네트워크 출력
  Serial.println(ssid);  // WiFi 네트워크 SSID 출력

  WiFi.begin(ssid, password);  // WiFi에 연결 시도

  while (WiFi.status() != WL_CONNECTED) {  // WiFi 연결이 완료될 때까지 대기
    delay(500);  // 0.5초 대기
    Serial.print(".");  // 연결 시도 표시
  }

  Serial.println("WiFi connected");  // WiFi 연결 완료 메시지 출력
  Serial.println("IP address: ");  // IP 주소 출력
  Serial.println(WiFi.localIP());  // 로컬 IP 주소 출력
}

void reconnect() {//재연결
  while (!client.connected()) {  // 클라이언트가 연결될 때까지 반복
    Serial.print("Attempting MQTT connection...");  // MQTT 연결 시도 메시지 출력
    if (client.connect("ArduinoClient")) {  // MQTT 서버에 연결
      Serial.println("connected");  // 연결 성공 메시지 출력
      client.subscribe("fireAlarm/control");  // "fireAlarm/control" 주제로 구독
    } else {
      Serial.print("failed, rc=");  // 연결 실패 메시지 출력
      Serial.print(client.state());  // 연결 상태 출력
      Serial.println(" try again in 5 seconds");  // 5초 후 다시 시도 메시지 출력
    }
  }
}

void setup() {
  pinMode(SoundPin_A, OUTPUT);  // 부저 핀을 출력으로 설정
  pinMode(SoundPin_B, OUTPUT);  
  pinMode(SoundPin_C, OUTPUT);
  pinMode(SoundPin_D, OUTPUT);
  pixels.begin();  // NeoPixel 초기화
  pixels2.begin();
  pixels3.begin();
  pixels4.begin();
  setup_wifi();  // WiFi 설정 초기화
  client.setServer(mqttServer, mqttPort);  // MQTT 클라이언트에 서버 설정
  client.setCallback(callback);  // MQTT 콜백 함수 설정
  if(client.connect("led")){  // MQTT 서버에 연결
    client.subscribe("fireAlarm/control");  // "fireAlarm/control" 주제로 구독
  }
  lastReconnectAttempt = 0;  // 마지막 재연결 시도 시간 초기화
}

void loop() {
  unsigned long currentMillis = millis();  // 현재 시간 가져오기

  if (!client.connected()) {//서버 연결 끊길시에 재연결
    reconnect();  // MQTT 서버와 재연결 시도
  }
  else{
      client.loop();  // MQTT 클라이언트 루프 실행
  }

  if (currentMillis - previousMillis >= DELAYVAL) {//딜레이만큼 대기
    previousMillis = currentMillis;  // 이전 시간 업데이트

    if (firecontrol) {//제어버튼 on or off시
      FireControl();  // 화재 제어 함수 호출
    } else {
      leeLED();  // LED 제어 함수 호출
    }
  }
}

void FireControl() {
  if (firecontrol == 1) {//제어버튼 on일때 모든 LED on 부저 on
    leeon(pixels);  // NeoPixel 객체에 LED 켜기 명령 전달
    leeon(pixels2);
    leeon(pixels3);
    leeon(pixels4);
    num++;  // num 변수 증가

    if (num % 5 == 0 || num % 5 == 1 || num % 5 == 2) {//부저가 계속 울리지않게 
      analogWrite(SoundPin_A, 3);  // 부저 출력 설정
      analogWrite(SoundPin_B, 3);
      analogWrite(SoundPin_C, 3);
      analogWrite(SoundPin_D, 3);
    } else {
      analogWrite(SoundPin_A, 0);  // 부저 출력 해제
      analogWrite(SoundPin_B, 0);
      analogWrite(SoundPin_C, 0);
      analogWrite(SoundPin_D, 0);
    }
  } else if (firecontrol == 2) {//제어버튼 off일때 LED off 부저 off
    analogWrite(SoundPin_A, 0);  // 부저 출력 해제
    analogWrite(SoundPin_B, 0);
    analogWrite(SoundPin_C, 0);
    analogWrite(SoundPin_D, 0);
    leeclear(pixels);  // NeoPixel 객체에 LED 끄기 명령 전달
    leeclear(pixels2);
    leeclear(pixels3);
    leeclear(pixels4);
  }
}

void leeLED() {//어디서 화재가 감지됐는지 확인후 LED on 부저 on
  if (A_fire || B_fire || C_fire || D_fire) {  // 화재가 감지된 경우
    leeledon(A_fire, B_fire, C_fire, D_fire);  // LED 방향 및 패턴 결정하는 함수 호출
    if (num % 5 == 0 || num % 5 == 1 || num % 5 == 2) {  // 부저 울리는 주기 설정
      if (A_fire) analogWrite(SoundPin_A, 3);  // 부저 출력 설정
      if (B_fire) analogWrite(SoundPin_B, 3);
      if (C_fire) analogWrite(SoundPin_C, 3);
      if (D_fire) analogWrite(SoundPin_D, 3);
    } else {
      analogWrite(SoundPin_A, 0);  // 부저 출력 해제
      analogWrite(SoundPin_B, 0);
      analogWrite(SoundPin_C, 0);
      analogWrite(SoundPin_D, 0);
    }
    num++;  // num 변수 증가
  } else {  // 화재가 감지되지 않은 경우
    client.publish("fireAlarm", "No fire detected.");  // MQTT로 메시지 전송
    analogWrite(SoundPin_A, 0);  // 부저 출력 해제
    analogWrite(SoundPin_B, 0);
    analogWrite(SoundPin_C, 0);
    analogWrite(SoundPin_D, 0);
    leeclear(pixels);  // NeoPixel 객체에 LED 끄기 명령 전달
    leeclear(pixels2);
    leeclear(pixels3);
    leeclear(pixels4);
  }
}

// 화재 감지 시 각 복도에 있는 LED가 어느 방향으로 순차적으로 점등해야하는지 결정을 해주는 함수
void leeledon(bool a, bool b, bool c, bool d) {
  if (c && b) {  // C와 B에서 화재가 감지된 경우
    leeon(pixels);  // 첫 번째 LED 스트립 켜기
    leeon(pixels4);  // 네 번째 LED 스트립 켜기
    leersv(pixels2);  // 두 번째 LED 스트립을 역방향으로 켜기
    leeclear(pixels3);  // 세 번째 LED 스트립 끄기
  } else if (c && d) {  // C와 D에서 화재가 감지된 경우
    leeon(pixels);  // 첫 번째 LED 스트립 켜기
    leersv(pixels3);  // 세 번째 LED 스트립 역방향으로 켜기
    leeon(pixels4);  // 네 번째 LED 스트립 켜기
  } else if (b && a) {  // B와 A에서 화재가 감지된 경우
    leersv(pixels);  // 첫 번째 LED 스트립을 역방향으로 켜기
    leeon(pixels3);  // 세 번째 LED 스트립 켜기
    leeon(pixels2);  // 두 번째 LED 스트립 켜기
  } else if (b && d) {  // B와 D에서 화재가 감지된 경우
    leeon(pixels);  // 첫 번째 LED 스트립 켜기
    leeon(pixels2);  // 두 번째 LED 스트립 켜기
    leeon(pixels3);  // 세 번째 LED 스트립 켜기
    leeon(pixels4);  // 네 번째 LED 스트립 켜기
  } else if (a && d) {  // A와 D에서 화재가 감지된 경우
    leersv(pixels4);  // 네 번째 LED 스트립을 역방향으로 켜기
    leeon(pixels2);  // 두 번째 LED 스트립 켜기
    leeon(pixels3);  // 세 번째 LED 스트립 켜기
  } else if (c) {  // C에서만 화재가 감지된 경우
    leeon(pixels);  // 첫 번째 LED 스트립 켜기
    leeon(pixels4);  // 네 번째 LED 스트립 켜기
    leersv(pixels2);  // 두 번째 LED 스트립을 역방향으로 켜기
    leersv(pixels3);  // 세 번째 LED 스트립을 역방향으로 켜기
  } else if (b || d) {  // B 또는 D에서 화재가 감지된 경우
    leeon(pixels);  // 첫 번째 LED 스트립 켜기
    leeon(pixels2);  // 두 번째 LED 스트립 켜기
    leeon(pixels3);  // 세 번째 LED 스트립 켜기
    leeon(pixels4);  // 네 번째 LED 스트립 켜기
  } else if (a) {  // A에서만 화재가 감지된 경우
    leeon(pixels2);  // 두 번째 LED 스트립 켜기
    leeon(pixels3);  // 세 번째 LED 스트립 켜기
    leersv(pixels);  // 첫 번째 LED 스트립을 역방향으로 켜기
    leersv(pixels4);  // 네 번째 LED 스트립을 역방향으로 켜기
  }
}

// LED 스트립의 LED를 정방향으로 켜는 함수
void leeon(Adafruit_NeoPixel& a) {  // LED 스트립의 LED를 켜는 함수
  a.setPixelColor(num % 10, 0, 150, 0);  // 현재 LED 스트립에서 순서에 맞게 LED를 초록색으로 켜기
  a.setPixelColor((num - 3) % 10, 0, 0, 0);  // 이전에 켰던 LED를 끄기
  a.show();  // 설정한 LED 색상을 실제로 표시
}

void leersv(Adafruit_NeoPixel& a) {  // LED 스트립의 LED를 반대 방향으로 켜는 함수
  int numrsv = 99999 - num;  // LED 번호를 반대로 매핑하기 위해 현재 번호를 반전시킴
  a.setPixelColor(numrsv % 10, 0, 150, 0);  // 현재 LED 스트립에서 순서에 맞게 반대 방향으로 LED를 초록색으로 켜기
  a.setPixelColor((numrsv - 7) % 10, 0, 0, 0);  // 이전에 켰던 LED를 끄기
  a.show();  // 설정한 LED 색상을 실제로 표시
}

void leemid(Adafruit_NeoPixel& a) {  // LED 스트립의 LED를 중앙에서부터 퍼지게 켜는 함수
  int numrsv = 99999 - num;  // LED 번호를 반대로 매핑하기 위해 현재 번호를 반전시킴
  a.setPixelColor((num % 5) + 5, 0, 150, 0);  // 현재 LED 스트립에서 중앙에서부터 LED를 초록색으로 켜기
  a.setPixelColor((num - 1) % 5 + 5, 0, 0, 0);  // 이전에 켰던 LED를 끄기
  a.setPixelColor(numrsv % 5, 0, 150, 0);  // 현재 LED 스트립에서 반대 방향에서 중앙으로 LED를 초록색으로 켜기
  a.setPixelColor((numrsv - 9) % 5, 0, 0, 0);  // 이전에 켰던 LED를 끄기
  a.show();  // 설정한 LED 색상을 실제로 표시
}

void leeclear(Adafruit_NeoPixel& a) {  // LED 스트립의 LED를 모두 끄는 함수
  a.clear();  // LED 스트립의 LED를 모두 끔
  a.show();  // 설정한 LED 색상을 실제로 표시
}

void callback(char* topic, byte* payload, unsigned int length) {  // MQTT 메시지를 수신하는 콜백 함수
  payload[length] = '\0';  // 페이로드 끝에 널 문자 추가하여 문자열로 변환
  String json = String((char*)payload);  // 페이로드를 문자열로 변환

  DynamicJsonDocument jsonDocument(200);  // JSON 문서 생성
  DeserializationError error = deserializeJson(jsonDocument, json);  // JSON 파싱

  if (!error) {  // 에러가 없을 경우
    A_fire = jsonDocument["A_fireDetected"];  // A 구역에서 화재 감지 여부 업데이트
    B_fire = jsonDocument["B_fireDetected"];  // B 구역에서 화재 감지 여부 업데이트
    C_fire = jsonDocument["C_fireDetected"];  // C 구역에서 화재 감지 여부 업데이트
    D_fire = jsonDocument["D_fireDetected"];  // D 구역에서 화재 감지 여부 업데이트
    firecontrol = jsonDocument["firecontrol"];  // 제어버튼 상태 업데이트
    Serial.print("A_fire: ");  // 시리얼 모니터에 A 구역 화재 감지 여부 출력
    Serial.println(A_fire);
    Serial.print("B_fire: ");  // 시리얼 모니터에 B 구역 화재 감지 여부 출력
    Serial.println(B_fire);
    Serial.print("C_fire: ");  // 시리얼 모니터에 C 구역 화재 감지 여부 출력
    Serial.println(C_fire);
    Serial.print("D_fire: ");  // 시리얼 모니터에 D 구역 화재 감지 여부 출력
    Serial.println(D_fire);
    Serial.print("firecontrol: ");  // 시리얼 모니터에 제어버튼 상태 출력
    Serial.println(firecontrol);
  } else {  // 에러가 있는 경우
    Serial.print("JSON deserialization failed: ");  // 시리얼 모니터에 JSON 파싱 실패 메시지 출력
    Serial.println(error.c_str());
  }
}