#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>


const char* ssid = "Yagil";
const char* password = "y2468024680";

// HTML content
constexpr const char* index_html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Security System</title>
<style>
table {
  width: 100%;
  border-collapse: collapse;
}
th, td {
  border: 1px solid black;
  padding: 8px;
  text-align: left;
}
th {
  background-color: #f2f2f2;
}
</style>
<script>
// Initialize IndexedDB
let db;
window.onload = function() {
  let request = window.indexedDB.open("SecuritySystemDB", 1);

  request.onerror = function(event) {
    console.error("Database error: " + event.target.errorCode);
  };

  request.onsuccess = function(event) {
    db = event.target.result;
    updateTable();
  };

  request.onupgradeneeded = function(event) {
    let db = event.target.result;
    db.createObjectStore("records", {keyPath: "id", autoIncrement: true});
  };

  document.getElementById("clearButton").addEventListener("click", clearAllRecords);
};

function addRecord(record) {
  let transaction = db.transaction(["records"], "readwrite");
  let store = transaction.objectStore("records");
  store.add(record);
}

function updateTable() {
  let transaction = db.transaction(["records"], "readonly");
  let store = transaction.objectStore("records");
  let cursorRequest = store.openCursor();

  const table = document.getElementById('historyTable');
  table.innerHTML = '<tr><th>Time</th><th>Distance (cm)</th></tr>'; // Clear table and add headers

  cursorRequest.onsuccess = function(e) {
    let cursor = e.target.result;
    if (cursor) {
      let record = cursor.value;
      let row = `<tr><td>${new Date(record.time).toLocaleString()}</td><td>${record.distance.toFixed(2)}</td></tr>`;
      table.innerHTML += row;
      cursor.continue();
    }
  };
}

function clearAllRecords() {
  let transaction = db.transaction(["records"], "readwrite");
  let store = transaction.objectStore("records");
  let clearRequest = store.clear(); // Clear all records from the store

  clearRequest.onsuccess = function() {
    console.log('All records have been cleared.');
    updateTable(); // Update the table to reflect the cleared records
  };

  clearRequest.onerror = function(e) {
    console.error('Error clearing the records.', e.target.errorCode);
  };
}

function openGate() {
  fetch("/open_gate", {
    method: 'POST', // Specify the method
    headers: {
      'Content-Type': 'application/json'
    }
  }).then(response => {
    if (response.ok) {
      console.log("Gate opened successfully");
    } else {
      console.error("Failed to open gate");
    }
    console.log("Gate opened");
  }).catch(error => {
    console.error("Network error: Could not connect to server");
  });
}

setInterval(() => {
  fetch("/distance")
    .then(response => response.json())
    .then(data => {
      document.getElementById('distance').textContent = `Distance: ${data.distance.toFixed(2)} cm`;
      if (data.alert) {
        document.body.style.backgroundColor = 'red';
        // Add new record to IndexedDB only if there is an alert
        addRecord({time: Date.now(), distance: data.distance});
        updateTable(); // Refresh the table with the new data
      } else {
        document.body.style.backgroundColor = 'white';
      }
    })
    .catch(error => {
      document.getElementById('distance').textContent = 'Failed to retrieve data.';
    });
}, 1000);
</script>


</head>
<body>
<h1>Security System</h1>
<p id="distance">Loading...</p>
<button id="clearButton">Clear All Records</button>
<button id="GateOpenerBtn" onclick="openGate()">Open Gate</button>
<table id="historyTable">
  <tr><th>Time</th><th>Distance (cm)</th></tr>
</table>
</body>
</html>

)rawliteral";

AsyncWebServer server(80);
enum PIN {
  TRIG = 18,
  ECHO = 32,
  RING = 13,
  BLUE_LIGHT = 2,
  RED_LIGHT = 4,
};

float baselineDistance = -1;
constexpr float DISTANCE_THRESHOLD = 5.0;  // Change threshold as needed

Servo myServo;

void setup() {
  Serial.begin(9600);
  pinMode(PIN::TRIG, OUTPUT);
  pinMode(PIN::ECHO, INPUT);
  pinMode(PIN::RING, OUTPUT);
  pinMode(PIN::BLUE_LIGHT, OUTPUT);
  pinMode(PIN::RED_LIGHT, OUTPUT);
  myServo.attach(5);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/open_gate", HTTP_POST, [](AsyncWebServerRequest *request) {
    myServo.write(-180);
    request->send(200);
  });

  server.on("/distance", HTTP_GET, [](AsyncWebServerRequest *request) {
    float currentDistance = measureDistance();
    bool alert = (baselineDistance != -1) && (abs(currentDistance - baselineDistance) > DISTANCE_THRESHOLD);
    if (baselineDistance == -1) baselineDistance = currentDistance;

    String json = "{\"distance\":" + String(currentDistance, 2) + ",\"alert\":" + (alert ? "true" : "false") + "}";
    request->send(200, "application/json", json);

    if (alert) {
      
      myServo.write(180);
      digitalWrite(PIN::RING, HIGH);
      delay(1000);
      digitalWrite(PIN::RING, LOW);

      digitalWrite(PIN::BLUE_LIGHT, HIGH);
      delay(1000);
      digitalWrite(PIN::BLUE_LIGHT, LOW);
      digitalWrite(PIN::RED_LIGHT, HIGH);
      delay(1000);
      digitalWrite(PIN::RED_LIGHT, LOW);
    }
  });

  server.begin();
}

void loop() {
  
}

unsigned long measureDuration() {
  digitalWrite(PIN::TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN::TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN::TRIG, LOW);

  return pulseIn(PIN::ECHO, HIGH, 30000);
}

float calculateDistance(unsigned long duration) {
  constexpr float WAVE_SPEED = 0.034;
  return duration ? (duration * WAVE_SPEED / 2) : -1;
}

float measureDistance() {
  return calculateDistance(measureDuration());
}
