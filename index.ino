#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// Definições dos pinos
const int powerPin = 23;  // Pino para ligar o PC
const int resetPin = 22;  // Pino para reiniciar o PC
const int buzzerPin = 19; // Pino do buzzer
const int powerStatusPin = 21;  // Pino conectado ao LED de energia do PC
const int servoPin = 18; // Pino do servo motor para a luz

// Configurações da rede
const char* ssid = "home_";
const char* password = "Fantasma01";

// Criação do servidor na porta 20069
WebServer server(20069);

// Melodia e ritmo do refrão de "Never Gonna Give You Up"
int song1_chorus_melody[] = {
  466, 466, 440, 440, // b4f, a4f
  698, 698, 622, 466, 466, 440, 440, 622, 622, 554, 523, 466, // f5, e5f, b4f, etc.
  554, 554, 554, 554, // c5s
  554, 622, 523, 466, 440, 440, 440, 622, 554, // c5s, e5f, etc.
  466, 466, 440, 440, // b4f, a4f
  698, // f5
  698, 622, 466, 466, 440, 440, 880, 523, 554, 523, 466, // f5, e5f, a5f, etc.
  554, 554, 554, 554, // c5s
  554, 622, 523, 466, 440, 0, 440, 622, 554, 0 // c5s, rest
};

int song1_chorus_rhythm[] = {
  1, 1, 1, 1,
  3, 3, 6, 1, 1, 1, 1, 3, 3, 3, 1, 2,
  1, 1, 1, 1,
  3, 3, 3, 1, 2, 2, 2, 4, 8,
  1, 1, 1, 1,
  3, 3, 6, 1, 1, 1, 1, 3, 3, 3,
  1, 2,
  1, 1, 1, 1,
  3, 3, 3, 1, 2, 2, 2, 4, 8, 4
};

// Instância do servo motor
Servo servo;
bool isLightOn = false;

const int logMaxEntries = 20;
String commandLog = "";
void addToLog(String message, String clientTime) {
  int logEntries = 0;
  String logArray[logMaxEntries];
  int startIndex = 0;
  int endIndex = commandLog.indexOf("<br>", startIndex);

  while (endIndex != -1 && logEntries < logMaxEntries) {
    logArray[logEntries] = commandLog.substring(startIndex, endIndex);
    logEntries++;
    startIndex = endIndex + 4;
    endIndex = commandLog.indexOf("<br>", startIndex);
  }

  logArray[logEntries] = "[" + clientTime + "] " + message + "</br>";
  logEntries++;
  
  if (logEntries > logMaxEntries) {
    for (int i = 1; i < logMaxEntries; i++) {
      logArray[i - 1] = logArray[i];
    }
    logEntries--;
  }

  commandLog = "";
  for (int i = 0; i < logEntries; i++) {
    commandLog += logArray[i] + "<br>";
  }

  Serial.println(logArray[logEntries - 1]); // Imprime no monitor serial para debug
}


void handleLog() {
  String html = "<html><body>";
  html += "<h1>Log de Comandos</h1>";
  html += "<p>" + commandLog + "</p>";
  html += "<a href='/'>Voltar</a>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}


void beep(int times, int duration = 100) {
  for (int i = 0; i < times; i++) {
    tone(buzzerPin, 1000);
    delay(duration);
    noTone(buzzerPin);
    delay(100);
  }
}
void setup() {
  Serial.begin(9600);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando ao WiFi...");
  }

  beep(1, 300);
  beep(3, 100);
  Serial.println("Conectado ao WiFi!");

  pinMode(powerStatusPin, INPUT_PULLUP);
  pinMode(powerPin, OUTPUT);
  pinMode(resetPin, OUTPUT);
  digitalWrite(powerPin, HIGH);
  digitalWrite(resetPin, HIGH);
  
  servo.attach(servoPin);
  servo.detach();

  server.on("/", handleRoot);
  server.on("/power", handlePowerOn);
  server.on("/restart", handleRestart);
  server.on("/light", handleLightToggle);
  server.on("/playTune", handlePlayTune);
  server.on("/log", handleLog);
  server.on("/pc", handlePC);
  server.on("/status", handleStatus);
  server.on("/fds", handleFDS);
  
  server.begin();
  Serial.println("Servidor iniciado!");
}

void loop() {
  server.handleClient();
}

// Função para verificar o status do PC
bool isPCOn() {
  return digitalRead(powerStatusPin) == LOW;
}

void handleRoot() {
  String html = "<!DOCTYPE html>";
  html += "<html lang='pt-br'>";
  html += "<head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Controle do PC e Luz</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f4f4f9; text-align: center; padding: 20px; }";
  html += "h1 { color: #333; }";
  html += "button { padding: 20px 30px; margin: 20px; font-size: 16px; background-color: #28a745; color: white; border: none; border-radius: 5px; cursor: pointer; }";
  html += "button:hover { background-color: #218838; }";
  html += ".red-button { background-color: #dc3545; color: white; }"; 
  html += ".red-button:hover { background-color: #c82333; }";
  html += ".light-on { background-color: #ffc107; color: black; }"; 
  html += ".light-on:hover { background-color: #e0a800; }"; 
  html += "#buttons { display: grid; justify-content: space-evenly; }";
  html += "#log { margin-top: 20px; text-align: left; background: #e9ecef; padding: 10px; border-radius: 5px; }";
  html += "#status { margin-top: 20px; padding: 10px; background: #d1ecf1; border-radius: 5px; }";  // Estilo para o status
  html += "</style>";
  html += "<script>";
  
  // Função para capturar a hora do cliente e enviar o comando com horário
  html += "function sendCommand(route) {";
  html += "  let now = new Date().toLocaleString();";  // Captura o horário do cliente
  html += "  fetch(route + '?time=' + encodeURIComponent(now)).then(response => response.text()).then(data => {";
  html += "    let statusDiv = document.getElementById('status');";
  html += "    statusDiv.style.display = 'block';";  // Exibe o campo de status quando houver resposta
  html += "    statusDiv.innerText = data;";  // Atualiza o status com a resposta";
  html += "    updateStatus();";
  html += "  });";
  html += "}";

  html += "function updateStatus() {";
  html += "  fetch('/status').then(response => response.text()).then(data => {";
  html += "    document.getElementById('buttons').innerHTML = data;";  // Atualiza os botões periodicamente
  html += "  });";
  html += "}";

  html += "setInterval(updateStatus, 3000);"; // Atualiza os botões a cada 3 segundos
  html += "</script>";
  html += "</head><body>";
  html += "<h1>Controle do PC e Luz</h1>";
  html += "<div id='buttons'>";
  html += generateButtonsHTML();  // Função que gera o HTML dos botões
  html += "</div>";
  html += "<div id='status' style='display:none'><p>Status: Aguardando...</p></div>";
  html += "<div id='log'><h3>Log de Comandos:</h3><p>" + commandLog + "</p></div>"; // Exibe o log
  html += "</body></html>";

  server.send(200, "text/html", html);
}

String generateButtonsHTML() {
  String html = "";
  if (isPCOn()) {
    html += "<button class='red-button' onclick=\"sendCommand('/power')\">Desligar PC</button>";
  } else {
    html += "<button onclick=\"sendCommand('/power')\">Ligar PC</button>";
  }

  html += "<button onclick=\"sendCommand('/restart')\">Reiniciar PC</button>";

  if (isLightOn) {
    html += "<button class='light-on' onclick=\"sendCommand('/light')\">Desligar Luz</button>"; 
  } else {
    html += "<button onclick=\"sendCommand('/light')\">Ligar Luz</button>"; 
  }
  html += "<div><button onclick=\"sendCommand('/playTune')\">Tocar Melodia</button>";
  html += "<button onclick=\"window.location.href='/log'\">Ver Log</button></div>";
  return html;
}

void handleStatus() {
  String html = generateButtonsHTML();  // Retorna o estado atualizado dos botões
  server.send(200, "text/html", html);
}

// Função para ligar a luz
void handleLightOn() {
  String clientTime = server.hasArg("time") ? server.arg("time") : "0";
  servo.write(27);
  delay(200);    
  isLightOn = true;  
  addToLog("Luz ligada", clientTime);
  server.send(200, "text/html", "Luz ligada!");
}

// Função para desligar a luz
void handleLightOff() {
  String clientTime = server.hasArg("time") ? server.arg("time") : "0";
  servo.write(79);
  delay(200);     
  isLightOn = false; 
  addToLog("Luz desligada", clientTime);
  server.send(200, "text/html", "Luz desligada!");
}

// Função para alternar o estado da luz (ligar ou desligar)
void handleLightToggle() {
  servo.attach(servoPin);
  if (isLightOn) {
    handleLightOff();
  } else {
    handleLightOn();
  }
  delay(200);  
  servo.write(55); 
  delay(200);   
  servo.detach();
}

void handlePowerOn() {
  String clientTime = server.hasArg("time") ? server.arg("time") : "0";
  if (isPCOn()) {
    // O PC já está ligado, então vamos tentar desligar
    addToLog("PC já está ligado, tentando desligar...", clientTime);
    server.send(200, "text/html", "PC já está ligado, tentando desligar... (20sec)");

    // Envia o comando para desligar o PC
    digitalWrite(powerPin, LOW);
    delay(100);
    digitalWrite(powerPin, HIGH);
    beep(1);

    // Verificação periódica para confirmar se o PC desligou (até 10 segundos)
    for (int elapsed = 0; elapsed < 20000; elapsed += 1000) {
      delay(1000);
      if (!isPCOn()) {
        addToLog("PC Desligado com sucesso", clientTime);
        beep(1, 30);
        beep(1, 10);
        return;
      }
    }

    // Caso o PC ainda não tenha desligado após o timeout
    addToLog("PC ainda ligado após tentativa de desligamento", clientTime);
    beep(1, 500);
  } else {
    // O PC está desligado, ligando o PC instantaneamente
    addToLog("PC desligado, ligando...", clientTime);
    server.send(200, "text/html", "PC ligando...");

    // Envia o comando para ligar o PC
    digitalWrite(powerPin, LOW);
    delay(100);
    digitalWrite(powerPin, HIGH);
    
    addToLog("PC Ligado", clientTime);
    beep(1, 30);
    beep(1, 10);
  }
}

void handleRestart() {
  String clientTime = server.hasArg("time") ? server.arg("time") : "0";
  digitalWrite(resetPin, LOW);
  delay(100);
  digitalWrite(resetPin, HIGH);
  beep(3);
  addToLog("PC Reiniciado", clientTime);
  server.send(200, "text/html", "PC Reiniciando...(20)!");
}

// Função para tocar a melodia de "Never Gonna Give You Up"
void handlePlayTune() {
  String clientTime = server.hasArg("time") ? server.arg("time") : "0";
  server.send(200, "text/html", "Tocando Musica!");
  for (int i = 0; i < sizeof(song1_chorus_melody) / sizeof(int); i++) {
    if (song1_chorus_melody[i] == 0) {
      delay(song1_chorus_rhythm[i] * 100);
    } else {
      tone(buzzerPin, song1_chorus_melody[i], song1_chorus_rhythm[i] * 100);
      delay(song1_chorus_rhythm[i] * 150);
    }
  }
  addToLog("MusicaTocada", clientTime);
  noTone(buzzerPin); // Desliga o buzzer após a melodia
}

void handleFDS() {
  server.send(200, "text/html", "<html><body><h1>Blz👍</h1></body></hmtl>");
  delay(1000);
  ESP.restart();  
}

void handlePC(){
    if (isPCOn()) {
    server.send(200, "text/html", "Pc Ligado :)");
  } else {
    server.send(200, "text/html", "Pc Desligado :(");
  }
}
