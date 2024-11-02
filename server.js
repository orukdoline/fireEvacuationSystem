const mqtt = require('mqtt');  // MQTT 모듈 불러옴
const WebSocket = require('ws');  // WebSocket 모듈 불러옴
const express = require('express');  // Express 모듈 불러옴
const mariadb = require('mariadb');  // MariaDB 모듈 불러옴

const app = express();  // Express 애플리케이션 생성함
const port = 3000;  // 서버 포트 지정함

// 화재가 감지되거나 감지되지 않는 순간에 화재 정보를 데이터베이스에 저장하기 위해서 전역변수를 선언함
let firecheck_A = false;  // 센서 A 화재 감지 상태 저장함
let firecheck_B = false;  // 센서 B 화재 감지 상태 저장함
let firecheck_C = false;  // 센서 C 화재 감지 상태 저장함
let firecheck_D = false;  // 센서 D 화재 감지 상태 저장함

const pool = mariadb.createPool({
  host: "127.0.0.1",  // MariaDB 서버 호스트 주소(로컬)
  user: "root",  // MariaDB 서버 사용자 이름(관리자)
  password: "1234",  // MariaDB 서버 사용자 비밀번호
  database: "fire_db"  // 사용할 데이터베이스 이름
});

// MQTT 브로커에 연결함(라즈베리파이가 현재 연결되어 있는 공유기 IP주소)
const mqttClient = mqtt.connect('mqtt://172.0.0.0:1883');

// 웹소켓 서버 생성하고 포트를 8080으로 설정함
const wss = new WebSocket.Server({ port: 8080 });

let firec = 0;  // LED 제어 상태를 저장하는 변수

wss.on('connection', (ws) => {
  console.log('WebSocket client connected');  // 웹소켓 클라이언트가 연결되면 콘솔에 해당 문구 출력

  ws.on('message', (message) => {
    console.log('Received from client:', message);  // 클라이언트로부터 메시지를 수신하면 해당 문구 출력
  });

  ws.on('close', () => {
    console.log('WebSocket client disconnected');  // 웹소켓 클라이언트가 연결 해제되면 해당 문구 출력
  });
});

mqttClient.on('connect', () => {
  console.log('Connected to MQTT broker');  // MQTT 브로커에 연결되면 해당 문구 출력
  mqttClient.subscribe('fireAlarm/status', (err) => { // fireAlarm/staus 토픽으로 구독 시도
    if (!err) {
      console.log('Subscribed to fireAlarm topic');  // fireAlarm/status 토픽으로 구독을 성공하면 해당 문구 출력
    } else {
      console.error('Subscription error:', err);  // fireAlarm/status 토픽으로 구독을 실패하면 해당 문구 출력
    }
  });
});

mqttClient.on('message', async(topic, message) => {
  if (topic === 'fireAlarm/status') {
    try {
      const jsonData = JSON.parse(message.toString());  // 수신한 메시지를 JSON 형식으로 파싱함

      // 각 변수에 JSON 데이터 저장함
      const A_fireDetected = jsonData.A_fireDetected;  // 센서 A 화재 감지 상태
      const B_fireDetected = jsonData.B_fireDetected;  // 센서 B 화재 감지 상태
      const C_fireDetected = jsonData.C_fireDetected;  // 센서 C 화재 감지 상태
      const D_fireDetected = jsonData.D_fireDetected;  // 센서 D 화재 감지 상태
      const A_Tem = jsonData.A_Tem;  // 센서 A 온도
      const B_Tem = jsonData.B_Tem;  // 센서 B 온도
      const C_Tem = jsonData.C_Tem;  // 센서 C 온도
      const D_Tem = jsonData.D_Tem;  // 센서 D 온도
      const A_Hum = jsonData.A_Hum;  // 센서 A 습도
      const B_Hum = jsonData.B_Hum;  // 센서 B 습도
      const C_Hum = jsonData.C_Hum;  // 센서 C 습도
      const D_Hum = jsonData.D_Hum;  // 센서 D 습도

      console.log('Received fireAlarm data:', jsonData);  // 수신한 데이터 로그로 출력함

      // 웹소켓을 통해 HTML에 데이터 전송함
      const sensorDataHTML = {
        A: { Tem: A_Tem, Hum: A_Hum, FIRE: A_fireDetected },
        B: { Tem: B_Tem, Hum: B_Hum, FIRE: B_fireDetected },
        C: { Tem: C_Tem, Hum: C_Hum, FIRE: C_fireDetected },
        D: { Tem: D_Tem, Hum: D_Hum, FIRE: D_fireDetected }
      };

      // 모든 웹소켓 클라이언트에 JSON 데이터 전송함
      wss.clients.forEach((client) => {
        if (client.readyState === WebSocket.OPEN) {
          client.send(JSON.stringify(jsonData));  // 원본 JSON 데이터 전송함
        }
      });

      // 웹소켓을 통해 HTML에 데이터 전송함
      wss.clients.forEach((client) => {
        if (client.readyState === WebSocket.OPEN) {
          client.send(JSON.stringify(sensorDataHTML));  // 가공된 HTML 데이터 전송함
        }
      });

      // 각 센서 화재 감지 상태가 변경되었는지 확인하고 데이터베이스에 저장함
      if (firecheck_A != A_fireDetected) {
        await insertFireData(A_Tem, A_Hum, 1, A_fireDetected);  // 센서 A 데이터 삽입함
        firecheck_A = A_fireDetected;  // 상태 업데이트
      }
      if (firecheck_B != B_fireDetected) {
        await insertFireData(B_Tem, B_Hum, 2, B_fireDetected);  // 센서 B 데이터 삽입함
        firecheck_B = B_fireDetected;  // 상태 업데이트
      }
      if (firecheck_C != C_fireDetected) {
        await insertFireData(C_Tem, C_Hum, 3, C_fireDetected);  // 센서 C 데이터 삽입함
        firecheck_C = C_fireDetected;  // 상태 업데이트
      }
      if (firecheck_D != D_fireDetected) {
        await insertFireData(D_Tem, D_Hum, 4, D_fireDetected);  // 센서 D 데이터 삽입함
        firecheck_D = D_fireDetected;  // 상태 업데이트
      }

      // fireDetectedData를 fireAlarm/control 토픽으로 발행함
      const fireDetectedData = {
        A_fireDetected: A_fireDetected,
        B_fireDetected: B_fireDetected,
        C_fireDetected: C_fireDetected,
        D_fireDetected: D_fireDetected,
        firecontrol: firec
      };

      mqttClient.publish('fireAlarm/control', JSON.stringify(fireDetectedData), (err) => {
        if (err) {
          console.error('Publish error:', err);  // 발행 실패 로그 출력함
        } else {
          console.log('Successfully published fireDetected data:', fireDetectedData);  // 발행 성공 로그 출력함
        }
      });

    } catch (error) {
      console.error('Error parsing JSON:', error);  // JSON 파싱 오류 로그 출력함
    }
  }
});

// 데이터베이스에 화재 데이터 삽입하는 함수임
async function insertFireData(temperature, humidity, sensor, firecheck) {
  try {
    const conn = await pool.getConnection();  // 데이터베이스에 연결
    const sql = "INSERT INTO fire (firetem, firehum, sennum, firecheck) VALUES (?, ?, ?, ?)";  // 삽입할 SQL 쿼리
    await conn.query(sql, [temperature, humidity, sensor, firecheck]);  // 쿼리 실행함
    conn.release();  // 연결 해제함
    console.log(`Data for sensor ${sensor} saved to database`);  // 삽입 성공 로그 출력함
  } catch (err) {
    console.error('Database insert error:', err);  // 삽입 실패 로그 출력함
  }
}

// 데이터 요청 엔드포인트 추가함
app.get('/data/:id', async (req, res) => {
  const { id } = req.params;  // 요청 경로에서 ID 가져옴
  let sql;  // 실행할 SQL 쿼리를 저장할 변수
  switch(id) {
    case '1':
      sql = "SELECT * FROM build WHERE bldnum = '1'";  // 건물 정보 쿼리
      break;
    case '2':
      sql = "SELECT * FROM sensor WHERE bldnum = '1'";  // 센서 정보 쿼리
      break;
    case '3':
      sql = "SELECT * FROM fire WHERE sennum = '1'";  // 센서 1 화재 정보 쿼리
      break;
    case '4':
      sql = "SELECT * FROM fire WHERE sennum = '2'";  // 센서 2 화재 정보 쿼리
      break;
    case '5':
      sql = "SELECT * FROM fire WHERE sennum = '3'";  // 센서 3 화재 정보 쿼리
      break;
    case '6':
      sql = "SELECT * FROM fire WHERE sennum = '4'";  // 센서 4 화재 정보 쿼리
      break;
    default:
      res.status(400).send('Invalid request');  // 유효하지 않은 요청에 대해 400 응답 보냄
      return;
  }

  try {
    const conn = await pool.getConnection();  // 데이터베이스 연결 가져옴
    const rows = await conn.query(sql);  // SQL 쿼리 실행함
    conn.release();  // 연결 해제함
    res.json(rows);  // 쿼리 결과를 JSON 형식으로 응답함
  } catch (err) {
    console.error('Database query error:', err);  // 쿼리 실패 로그 출력함
    res.status(500).send('Database query failed');  // 쿼리 실패에 대해 500 응답 보냄
  }
});

app.get('/state/:state', (req, res) => {
  const { state } = req.params;  // 요청 경로에서 상태 가져옴
  if (state === '1' || state === '2' || state === '0') {  // 유효한 상태인지 확인함
    firec = state;  // 상태 저장함
    const message = JSON.stringify({ firecontrol: parseInt(state) });  // 상태를 JSON 형식으로 변환함
    mqttClient.publish('fireAlarm/control', message, (err) => {  // fireAlarm/control 토픽으로 상태 발행함
      if (err) {
        console.error('Publish error:', err);  // 발행 실패 로그 출력함
        res.status(500).send('Publish failed');  // 발행 실패에 대해 500 응답 보냄
      } else {
        res.send(`${state}`);  // 상태를 응답으로 보냄
        console.log("Successfully published state:", state);  // 발행 성공 로그 출력함
      }
    });
  } else {
    res.status(400).send('Invalid state');  // 유효하지 않은 상태에 대해 400 응답 보냄
  }
});

// 정적 파일 제공하기 위한 Express 설정임
app.use(express.static('public'));

// 새 페이지로의 라우팅임
app.get('/arduino_recivetest', (req, res) => {
  res.sendFile(__dirname + '/public/prototype.html');  // prototype.html 파일 응답으로 보냄
});

// 모든 IP 주소에서 서버가 들을 수 있도록 설정함
app.listen(port, '0.0.0.0', () => {
  console.log(`Server running at http://172.20.10.14:${port}/`);  // 서버 실행 중 로그 출력함
});
