#include <DHT.h>  // DHT 센서 라이브러리 포함
#if defined(ARDUINO_PORTENTA_C33)
#include <WiFiC3.h>  // Portenta 보드용 WiFi 라이브러리 포함
#elif defined(ARDUINO_UNOWIFIR4)
#include <WiFiS3.h>  // Uno WiFi Rev2 보드용 WiFi 라이브러리 포함
#endif
#include <PubSubClient.h>  // MQTT 클라이언트 라이브러리 포함
#include <ArduinoJson.h>  // ArduinoJson 라이브러리 포함
#ifdef __AVR__
#include <avr/power.h>  // AVR 아키텍처용 전원 관리 라이브러리 포함
#endif

#define A_DHTPin 2  // A 구역 DHT 센서 핀 번호
#define A_FlamePin 3  // A 구역 적외선 감지 핀 번호
#define B_DHTPin 4  // B 구역 DHT 센서 핀 번호
#define B_FlamePin 5  // B 구역 적외선 감지 핀 번호
#define C_DHTPin 10  // C 구역 DHT 센서 핀 번호
#define C_FlamePin 11  // C 구역 적외선 감지 핀 번호
#define D_DHTPin 8  // D 구역 DHT 센서 핀 번호
#define D_FlamePin 9  // D 구역 적외선 감지 핀 번호

DHT A_dht(A_DHTPin, DHT22);  // A 구역 DHT 센서 객체 생성
DHT B_dht(B_DHTPin, DHT22);  // B 구역 DHT 센서 객체 생성
DHT C_dht(C_DHTPin, DHT22);  // C 구역 DHT 센서 객체 생성
DHT D_dht(D_DHTPin, DHT22);  // D 구역 DHT 센서 객체 생성

const char* ssid = "JSW iPhone14 Pro";  // WiFi 네트워크 SSID 설정
const char* password = "0000001151";  // WiFi 네트워크 암호 설정

// MQTT 브로커 설정
const char* mqttServer = "172.20.10.14";  // MQTT 브로커 주소 설정
const int mqttPort = 1883;  // MQTT 브로커 포트 설정
WiFiClient espClient;  // WiFi 클라이언트 객체 생성
PubSubClient client(espClient);  // MQTT 클라이언트 객체 생성

unsigned long lastReconnectAttempt = 0;  // 마지막 재연결 시도 시간 기록
unsigned long lastSensorReadTime = 0;  // 마지막 센서 읽기 시간 기록
const long sensorReadInterval = 1000;  // 센서 읽기 간격 설정 (1초)

void setup_wifi() {
  Serial.begin(9600);  // 시리얼 통신 시작
  Serial.println();  // 개행
  Serial.print("Connecting to ");  // 연결 중 메시지 출력
  Serial.println(ssid);  // SSID 출력

  WiFi.begin(ssid, password);  // WiFi 연결 시작

  while (WiFi.status() != WL_CONNECTED) {  // WiFi 연결 상태 확인
    delay(500);  // 0.5초 대기
    Serial.print(".");  // 연결 중 메시지 출력
  }

  Serial.println("WiFi connected");  // WiFi 연결 완료 메시지 출력
  Serial.println("IP address: ");  // IP 주소 출력
  Serial.println(WiFi.localIP());  // 로컬 IP 주소 출력
}

boolean reconnect() {
  if (client.connect("ArduinoClient")) {  // MQTT 클라이언트 연결 시도
    Serial.println("connected");  // 연결 성공 메시지 출력
    client.subscribe("fireAlarm/status");  // 토픽 구독
    return true;  // 연결 성공을 나타내는 값 반환
  }
  return false;  // 연결 실패를 나타내는 값 반환
}

void setup() {
  pinMode(A_FlamePin, INPUT);  // A 구역 적외선 감지 핀을 입력으로 설정
  pinMode(B_FlamePin, INPUT);  // B 구역 적외선 감지 핀을 입력으로 설정
  pinMode(C_FlamePin, INPUT);  // C 구역 적외선 감지 핀을 입력으로 설정
  pinMode(D_FlamePin, INPUT);  // D 구역 적외선 감지 핀을 입력으로 설정

  A_dht.begin();  // A 구역 DHT 센서 초기화
  B_dht.begin();  // B 구역 DHT 센서 초기화
  C_dht.begin();  // C 구역 DHT 센서 초기화
  D_dht.begin();  // D 구역 DHT 센서 초기화

  setup_wifi();  // WiFi 연결 설정
  client.setServer(mqttServer, mqttPort);  // MQTT 브로커 설정
  if(client.connect("sensor")) {  // MQTT 클라이언트 연결 시도
    client.subscribe("fireAlarm/status");  // 토픽 구독
  }
  lastReconnectAttempt = 0;  // 마지막 재연결 시도 시간 초기화
}

void loop() {
  if (!client.connected()) {  // MQTT 클라이언트가 연결되어 있지 않으면
    unsigned long now = millis();  // 현재 시간 기록
    if (now - lastReconnectAttempt > 5000) {  // 마지막 재연결 시도 이후 5초가 지나면
      lastReconnectAttempt = now;  // 마지막 재연결 시도 시간 기록
      if (reconnect()) {  // MQTT 재연결 시도
        lastReconnectAttempt = 0;  // 재연결 시도 시간 초기화
      }
    }
  } else {  // MQTT 클라이언트가 연결되어 있으면
    client.loop();  // MQTT 클라이언트 루프 수행
  }

  unsigned long now = millis();  // 현재 시간 기록
  if (now - lastSensorReadTime > sensorReadInterval) {  // 센서 읽기 간격마다
    lastSensorReadTime = now;  // 마지막 센서 읽기 시간 기록
    readAndPublishSensorData();  // 센서 데이터 읽고 MQTT로 게시
  }
}

void readAndPublishSensorData() {
  bool A_fire = false;  // A 구역 화재 감지 여부 초기화
  bool B_fire = false;  // B 구역 화재 감지 여부 초기화
  bool C_fire = false;  // C 구역 화재 감지 여부 초기화
  bool D_fire = false;  // D 구역 화재 감지 여부 초기화

  float A_temperature = A_dht.readTemperature();  // A 구역 온도 읽기
  float B_temperature = B_dht.readTemperature();  // B 구역 온도 읽기
  float C_temperature = C_dht.readTemperature();  // C 구역 온도 읽기
  float D_temperature = D_dht.readTemperature();  // D 구역 온도 읽기
  float A_Humidity = A_dht.readHumidity();  // A 구역 습도 읽기
  float B_Humidity = B_dht.readHumidity();  // B 구역 습도 읽기
  float C_Humidity = C_dht.readHumidity();  // C 구역 습도 읽기
  float D_Humidity = D_dht.readHumidity();  // D 구역 습도 읽기

  if (isnan(A_temperature) || isnan(A_Humidity)) {  // A 구역 센서 데이터가 유효하지 않으면
    Serial.println("Failed to read from A DHT sensor!");  // 실패 메시지 출력
  } else {  // A 구역 센서 데이터가 유효하면
    A_fire = checkFlame(A_FlamePin);  // A 구역 화재 감지 여부 확인
  }

  if (isnan(B_temperature) || isnan(B_Humidity)) {  // B 구역 센서 데이터가 유효하지 않으면
    Serial.println("Failed to read from B DHT sensor!");  // 실패 메시지 출력
  } else {  // B 구역 센서 데이터가 유효하면
    B_fire = checkFlame(B_FlamePin);  // B 구역 화재 감지 여부 확인
  }

  if (isnan(C_temperature) || isnan(C_Humidity)) {  // C 구역 센서 데이터가 유효하지 않으면
    Serial.println("Failed to read from C DHT sensor!");  // 실패 메시지 출력
  } else {  // C 구역 센서 데이터가 유효하면
    C_fire = checkFlame(C_FlamePin);  // C 구역 화재 감지 여부 확인
  }

  if (isnan(D_temperature) || isnan(D_Humidity)) {  // D 구역 센서 데이터가 유효하지 않으면
    Serial.println("Failed to read from D DHT sensor!");  // 실패 메시지 출력
  } else {  // D 구역 센서 데이터가 유효하면
    D_fire = checkFlame(D_FlamePin);  // D 구역 화재 감지 여부 확인
  }

  DynamicJsonDocument jsonDocument(200);  // JSON 문서 생성

  // JSON 문서에 데이터 추가
  jsonDocument["A_fireDetected"] = A_fire;
  jsonDocument["B_fireDetected"] = B_fire;
  jsonDocument["C_fireDetected"] = C_fire;
  jsonDocument["D_fireDetected"] = D_fire;
  jsonDocument["A_Tem"] = round(A_temperature * 10.0) / 10.0;
  jsonDocument["B_Tem"] = round(B_temperature * 10.0) / 10.0;
  jsonDocument["C_Tem"] = round(C_temperature * 10.0) / 10.0;
  jsonDocument["D_Tem"] = round(D_temperature * 10.0) / 10.0;
  jsonDocument["A_Hum"] = round(A_Humidity * 10.0) / 10.0;
  jsonDocument["B_Hum"] = round(B_Humidity * 10.0) / 10.0;
  jsonDocument["C_Hum"] = round(C_Humidity * 10.0) / 10.0;
  jsonDocument["D_Hum"] = round(D_Humidity * 10.0) / 10.0;

  char buffer[256];  // JSON 문자열을 저장할 버퍼

  // JSON 문서를 문자열로 직렬화
  serializeJson(jsonDocument, buffer);

  // MQTT 브로커에 JSON 문자열 게시
  client.publish("fireAlarm/status", buffer);
}

bool checkFlame(int FlamePin) {
  int flamesensorValue = digitalRead(FlamePin);  // 적외선 감지 센서에서 값을 읽음
  return flamesensorValue == LOW;  // 적외선을 감지했으면 true 반환, 그렇지 않으면 false 반환
}

