#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <FS.h>
#include <SPIFFS.h>

const char *ssid = "Sahakiyat's RaspberryPi";
const char *password = "zxcvbnml";

WebServer server(80);
const int ledPin = LED_BUILTIN;

unsigned long lastUploadTime = 0;  // Store last upload timestamp

const char uploadPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Upload Image</title>
    <script>
        function checkFile() {
            var fileInput = document.getElementById('file');
            if (!fileInput.files.length) {
                alert("Please select a file before uploading.");
                return false;
            }
            return true;
        }
    </script>
</head>
<body>
    <h2>Upload Image</h2>
    <form method="POST" action="/upload" enctype="multipart/form-data" onsubmit="return checkFile()">
        <input type="file" name="file" accept="image/*" id="file" required>
        <input type="submit" value="Upload">
    </form>
</body>
</html>
)rawliteral";

void handleUploadPage() {
    server.send(200, "text/html", uploadPage);
}

void handleUpload() {
    static File file;
    static bool writing = false;

    HTTPUpload &upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        Serial.println("Upload started...");
        file = SPIFFS.open("/image.jpg", FILE_WRITE);
        if (!file) {
            Serial.println("Failed to open file for writing");
            server.send(500, "text/plain", "Failed to open file for writing");
            return;
        }
        writing = true;
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        Serial.println("Writing data...");
        if (writing) {
            file.write(upload.buf, upload.currentSize);
            digitalWrite(ledPin, !digitalRead(ledPin)); // Blink LED during upload
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        Serial.println("Upload finished");
        if (writing) {
            file.close();
            writing = false;
        }

        // Blink LED rapidly to indicate completion
        for (int i = 0; i < 10; i++) {
            digitalWrite(ledPin, HIGH);
            delay(50);
            digitalWrite(ledPin, LOW);
            delay(50);
        }

        // Update last upload timestamp
        lastUploadTime = millis();

        server.send(200, "text/plain", "Upload successful!");
    }
}

const char imagePage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>View Image</title>
    <script>
        let lastUploadTime = 0;

        function checkForUpdate() {
            fetch('/upload-time')
                .then(response => response.text())
                .then(timestamp => {
                    if (parseInt(timestamp) > lastUploadTime) {
                        lastUploadTime = parseInt(timestamp);
                        location.reload(); // Refresh page if new upload detected
                    }
                })
                .catch(err => console.error("Error checking update:", err));
        }

        setInterval(checkForUpdate, 3000); // Check every 3 seconds
    </script>
</head>
<body>
    <h2>Stored Image</h2>
    <img src="/image.jpg" alt="Stored Image" style="max-width:100%; height:auto;">
    <br>
    <form method="GET" action="/sstv">
        <input type="submit" value="Convert to SSTV">
    </form>
</body>
</html>
)rawliteral";

void handleImagePage() {
    server.send(200, "text/html", imagePage);
}

void handleImageFile() {
    File file = SPIFFS.open("/image.jpg", FILE_READ);
    if (!file) {
        server.send(404, "text/plain", "File not found");
        return;
    }
    server.streamFile(file, "image/jpeg");
    file.close();
}

void handleSSTV() {
    Serial.println("Converting image to SSTV audio...");
    server.send(200, "text/plain", "SSTV conversion not implemented yet");
}

// API to return last upload time
void handleUploadTime() {
    server.send(200, "text/plain", String(lastUploadTime));
}

void setup() {
    Serial.begin(115200);
    pinMode(ledPin, OUTPUT);
    neopixelWrite(ledPin, 0,0,255);

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        neopixelWrite(ledPin, 255,0,0);
    }
    Serial.println("\nWiFi connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    neopixelWrite(ledPin, 0,255,255);

    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS initialization failed");
        return;
    }

    server.on("/", HTTP_GET, handleUploadPage);
    server.on("/upload", HTTP_POST, handleUpload);
    server.on("/image", HTTP_GET, handleImagePage);
    server.on("/image.jpg", HTTP_GET, handleImageFile);
    server.on("/sstv", HTTP_GET, handleSSTV);
    server.on("/upload-time", HTTP_GET, handleUploadTime); // New API for checking updates
    
    server.begin();
    Serial.println("Web server started");
    neopixelWrite(ledPin, 0,0,0);
}

void loop() {
    server.handleClient();
}
