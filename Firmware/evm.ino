#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <mbedtls/md.h>

// --- Shared Hardware SPI (For SD Card ONLY) ---
#define SPI_MOSI 23
#define SPI_MISO 19
#define SPI_SCK 18
#define SD_CS 5

// --- Software SPI (For OLED ONLY) ---
#define OLED_SCL 22  // Screen Clock
#define OLED_SDA 21  // Screen Data
#define OLED_DC  17  // Data/Command
#define OLED_RST 16  // Reset

// --- Button & Buzzer Pins ---
#define BTN_CAND1 32
#define BTN_CAND2 33
#define BTN_CAND3 25
#define BTN_CONFIRM 26
#define BUZZER 27

// --- Safe LED Pins ---
#define LED_AUTH 13     // Green: ON when Ready/Authorized/Success
#define LED_PROCESS 4   // Yellow: ON when Waiting/Processing
#define LED_ERROR 14    // Red: Flashes on error

// --- OLED Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
// Initialize using Software SPI (-1 indicates no CS pin on this module)
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_SDA, OLED_SCL, OLED_DC, OLED_RST, -1);

// --- State Machine ---
enum EVMState { LOCKED, AUTHORIZED, CANDIDATE_SELECTED, STORE_VOTE, COMPLETE, SYS_ERROR, SHOW_RESULTS };
EVMState currentState = LOCKED;

// --- Globals ---
Preferences nvs;
String currentVoterID = "";
int selectedCandidate = 0;
unsigned long confirmPressTime = 0;
bool confirmActive = false;
bool adminDrawn = false;

// NVS Counts
uint32_t count1 = 0, count2 = 0, count3 = 0, totalVotes = 0;
String lastHash = "00000000000000000000000000000000";

// --- Function Prototypes ---
void displayMessage(String line1, String line2 = "");
void handleSerial();
void processVote();
void beep(int duration, int count = 1, unsigned int frequency = 2000);
String generateSHA256(String payload);
void logEvent(String event);

void setup() {
  Serial.begin(115200);
  
  // Initialize LED & Buzzer pins
  pinMode(LED_AUTH, OUTPUT);
  pinMode(LED_PROCESS, OUTPUT);
  pinMode(LED_ERROR, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  // THE FIX: Start with Yellow LED on (Waiting for Auth)
  digitalWrite(LED_AUTH, LOW);
  digitalWrite(LED_PROCESS, HIGH); 
  digitalWrite(LED_ERROR, LOW);

  // Initialize Button pins
  pinMode(BTN_CAND1, INPUT_PULLUP);
  pinMode(BTN_CAND2, INPUT_PULLUP);
  pinMode(BTN_CAND3, INPUT_PULLUP);
  pinMode(BTN_CONFIRM, INPUT_PULLUP);

  // 1. Initialize the Hardware SPI Bus for the SD Card
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);

  // 2. Initialize Software SPI OLED
  if(!display.begin(0, true)) {
    Serial.println("OLED Init Failed");
    while(1); // Halt
  }
  
  display.setTextColor(SH110X_WHITE);
  displayMessage("EVM SYSTEM", "Initializing...");
  beep(100, 1, 1500); // Startup chirp

  // 3. Initialize SD Card
  if(!SD.begin(SD_CS)) {
    digitalWrite(LED_PROCESS, LOW);
    digitalWrite(LED_ERROR, HIGH);
    displayMessage("SD Error", "Voting Locked");
    currentState = SYS_ERROR;
    beep(150, 3, 500); // Low error tone
    return;
  }
  logEvent("BOOT");

  // 4. Initialize NVS
  nvs.begin("evm_data", false);
  count1 = nvs.getUInt("cand1", 0);
  count2 = nvs.getUInt("cand2", 0);
  count3 = nvs.getUInt("cand3", 0);
  totalVotes = nvs.getUInt("total", 0);
  
  currentState = LOCKED;
  displayMessage("Waiting", "Authorization");
}

void loop() {
  if (currentState == SYS_ERROR) return; // Halt execution on critical hardware failure

  // 1. Always check for incoming serial authorization
  handleSerial();

  // 2. State Machine Handling
  switch (currentState) {
    
    case LOCKED:
      // Ensure Yellow is ON, others OFF
      digitalWrite(LED_AUTH, LOW);
      digitalWrite(LED_PROCESS, HIGH); 
      digitalWrite(LED_ERROR, LOW);
      break;

    case AUTHORIZED:
      {
        int b1 = digitalRead(BTN_CAND1) == LOW;
        int b2 = digitalRead(BTN_CAND2) == LOW;
        int b3 = digitalRead(BTN_CAND3) == LOW;

        // Multiple Button Protection
        if (b1 + b2 + b3 > 1) {
          digitalWrite(LED_ERROR, HIGH);
          displayMessage("Invalid", "Selection");
          beep(100, 2, 800); 
          delay(1000); 
          digitalWrite(LED_ERROR, LOW);
          displayMessage("Select", "Candidate");
          break;
        }

        // 50ms Debounce Check
        if (b1 || b2 || b3) {
          delay(50);
          if (digitalRead(BTN_CAND1) == LOW) selectedCandidate = 1;
          else if (digitalRead(BTN_CAND2) == LOW) selectedCandidate = 2;
          else if (digitalRead(BTN_CAND3) == LOW) selectedCandidate = 3;
          else break; // Bounced

          // Wait for the voter to lift their finger!
          while(digitalRead(BTN_CAND1) == LOW || digitalRead(BTN_CAND2) == LOW || digitalRead(BTN_CAND3) == LOW) {
            delay(10);
          }

          currentState = CANDIDATE_SELECTED;
          
          // THE FIX: Swap from Green to Yellow because we are waiting for Confirm
          digitalWrite(LED_AUTH, LOW);
          digitalWrite(LED_PROCESS, HIGH); 
          
          displayMessage("Candidate " + String(selectedCandidate), "Press Confirm");
          beep(50, 1, 3000); 
        }
      }
      break;

    case CANDIDATE_SELECTED:
      // --- Confirm Button Logic (Single Press) ---
      if (digitalRead(BTN_CONFIRM) == LOW) {
        delay(50); 
        if (digitalRead(BTN_CONFIRM) == LOW) {
          
          // Wait for the voter to lift the confirm button!
          while(digitalRead(BTN_CONFIRM) == LOW) {
            delay(10);
          }
          
          currentState = STORE_VOTE; 
          break; 
        }
      }
      
      // --- Change Candidate Logic ---
      // Allow changing candidate before pressing confirm
      if (digitalRead(BTN_CAND1) == LOW || digitalRead(BTN_CAND2) == LOW || digitalRead(BTN_CAND3) == LOW) {
          delay(50); 
          
          // Wait for the voter to lift their finger!
          while(digitalRead(BTN_CAND1) == LOW || digitalRead(BTN_CAND2) == LOW || digitalRead(BTN_CAND3) == LOW) {
            delay(10);
          }
          
          currentState = AUTHORIZED; 
          
          // THE FIX: Swap back to Green because we are ready for a new selection
          digitalWrite(LED_PROCESS, LOW);
          digitalWrite(LED_AUTH, HIGH); 
          
          displayMessage("Select", "Candidate"); // Redraw the selection screen
      }
      break;

    case STORE_VOTE:
      processVote();
      break;

    case COMPLETE:
      displayMessage("Vote Recorded", "Thank You");
      beep(150, 2, 2500); // Happy success dual tone
      delay(3000); // Show success for 3 seconds
      
      currentState = LOCKED;
      currentVoterID = "";
      selectedCandidate = 0;
      displayMessage("Waiting", "Authorization");
      
      // THE FIX: Instantly turn the Yellow LED back on for the LOCKED state
      digitalWrite(LED_AUTH, LOW);
      digitalWrite(LED_PROCESS, HIGH);
      break;

    case SHOW_RESULTS:
      if (!adminDrawn) {
        // Only turn on the Green LED to save USB power
        digitalWrite(LED_AUTH, HIGH);
        digitalWrite(LED_PROCESS, LOW);
        digitalWrite(LED_ERROR, LOW);
        
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println("--- SECURE TALLY ---");
        display.printf("Total Votes : %d\n", totalVotes);
        display.printf("Candidate 1 : %d\n", count1);
        display.printf("Candidate 2 : %d\n", count2);
        display.printf("Candidate 3 : %d\n", count3);
        display.display();

        adminDrawn = true; // Lock it so it doesn't redraw continuously
      }

      // Press the physical Confirm button to exit Admin Mode
      if (digitalRead(BTN_CONFIRM) == LOW) {
        delay(50);
        while(digitalRead(BTN_CONFIRM) == LOW) { delay(10); } // Wait for release
        
        digitalWrite(LED_AUTH, LOW);
        digitalWrite(LED_PROCESS, HIGH); // Turn Yellow ON for LOCKED state
        digitalWrite(LED_ERROR, LOW);
        currentState = LOCKED;
        displayMessage("Waiting", "Authorization");
      }
      break;
  }
}

// --- Helper Functions ---

void handleSerial() {
  if (Serial.available()) {
    String payload = Serial.readStringUntil('\n');
    payload.trim();

    // Format: AUTH|VoterID|CRC
    if (payload.startsWith("AUTH|")) {
      int firstPipe = payload.indexOf('|');
      int secondPipe = payload.lastIndexOf('|');

      if (firstPipe != -1 && secondPipe != -1 && firstPipe != secondPipe) {
        String voterID = payload.substring(firstPipe + 1, secondPipe);
        String crcStr = payload.substring(secondPipe + 1);

        if (crcStr == "1234") { 
           if (currentState == LOCKED) {
              currentVoterID = voterID;
              currentState = AUTHORIZED;
              
              // THE FIX: Turn off Yellow, Turn on Green Auth LED
              digitalWrite(LED_PROCESS, LOW); 
              digitalWrite(LED_AUTH, HIGH); 
              
              displayMessage("Authorized", "Select Candidate");
              beep(100, 1, 2000); // Alert officer machine is armed
              logEvent("AUTH_RECEIVED:" + currentVoterID);
           }
        }
      }
    }
    
    // --- Admin Request Logic ---
    if (payload.startsWith("ADMIN|")) {
      String pass = payload.substring(6);
      if (pass == "admin123") { // Hardcoded master password
        currentState = SHOW_RESULTS;
        adminDrawn = false;
        beep(200, 2, 1000); // Two low beeps to indicate mode change
        
        // Transmit the results back to the laptop: RESULTS|C1|C2|C3|TOTAL
        Serial.printf("RESULTS|%d|%d|%d|%d\n", count1, count2, count3, totalVotes);
        logEvent("ADMIN_RESULTS_ACCESSED");
      }
    }

    // --- Factory Reset Logic ---
    if (payload.startsWith("RESET|")) {
      String pass = payload.substring(6);
      if (pass == "admin123") { 
        digitalWrite(LED_AUTH, LOW);
        digitalWrite(LED_PROCESS, HIGH); // Turn Yellow ON while wiping
        digitalWrite(LED_ERROR, LOW);
        
        displayMessage("WIPING MEMORY", "Please Wait...");
        beep(500, 1, 1000);
        
        nvs.clear(); // Wipe the NVS memory
        SD.remove("/votes.csv"); // Delete the SD card audit log
        SD.remove("/events.csv");
        
        delay(1000);
        ESP.restart(); // Reboot the ESP32 completely
      }
    }
  }
}

void processVote() {
  // Yellow LED is already ON from CANDIDATE_SELECTED state
  displayMessage("Saving...", "Please Wait");

  // Step 1: Increment Count
  totalVotes++;
  if (selectedCandidate == 1) count1++;
  if (selectedCandidate == 2) count2++;
  if (selectedCandidate == 3) count3++;

  // Step 2: Save to NVS
  nvs.putUInt("cand1", count1);
  nvs.putUInt("cand2", count2);
  nvs.putUInt("cand3", count3);
  nvs.putUInt("total", totalVotes);

  // Step 3 & 4: Hash & Save to SD
  String timestamp = String(millis()); // Placeholder: Requires RTC for real dates
  String dataToHash = String(totalVotes) + timestamp + lastHash;
  String currentHash = generateSHA256(dataToHash);

  File voteLog = SD.open("/votes.csv", FILE_APPEND);
  if (voteLog) {
    voteLog.print(totalVotes);
    voteLog.print(",");
    voteLog.print(timestamp);
    voteLog.print(",");
    voteLog.print(lastHash);
    voteLog.print(",");
    voteLog.println(currentHash);
    voteLog.close();
    
    lastHash = currentHash; // Advance the chain
    logEvent("VOTE_RECORDED");

    // Step 5: Send Success
    Serial.println("SUCCESS|" + currentVoterID);
    
    // THE FIX: Turn off Yellow, Turn on Green for the "Vote Recorded" success screen
    digitalWrite(LED_PROCESS, LOW); 
    digitalWrite(LED_AUTH, HIGH); 
    
    currentState = COMPLETE;
  } else {
    // Critical Failure: SD Write Failed
    digitalWrite(LED_PROCESS, LOW);
    digitalWrite(LED_ERROR, HIGH); // Turn ON Error LED permanently
    displayMessage("SD Error", "Voting Locked");
    logEvent("SD_WRITE_ERROR");
    beep(200, 4, 500); // Continuous error tones
    currentState = SYS_ERROR; 
  }
}

void logEvent(String event) {
  File eventLog = SD.open("/events.csv", FILE_APPEND);
  if (eventLog) {
    eventLog.println(String(millis()) + "," + event);
    eventLog.close();
  }
}

void displayMessage(String line1, String line2) {
  display.clearDisplay();
  display.setTextSize(1);
  
  // Center Line 1
  int16_t x1, y1; uint16_t w1, h1;
  display.getTextBounds(line1, 0, 0, &x1, &y1, &w1, &h1);
  display.setCursor((SCREEN_WIDTH - w1) / 2, 20);
  display.println(line1);

  // Center Line 2
  if (line2.length() > 0) {
    int16_t x2, y2; uint16_t w2, h2;
    display.getTextBounds(line2, 0, 0, &x2, &y2, &w2, &h2);
    display.setCursor((SCREEN_WIDTH - w2) / 2, 40);
    display.println(line2);
  }
  
  display.display();
}

void beep(int duration, int count, unsigned int frequency) {
  for (int i = 0; i < count; i++) {
    tone(BUZZER, frequency);
    delay(duration);
    noTone(BUZZER);
    if (i < count - 1) delay(100);
  }
}

String generateSHA256(String payload) {
  byte shaResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
 
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *)payload.c_str(), payload.length());
  mbedtls_md_finish(&ctx, shaResult);
  mbedtls_md_free(&ctx);
 
  String hashStr = "";
  for(int i = 0; i < 32; i++) {
    char str[3];
    sprintf(str, "%02x", (int)shaResult[i]);
    hashStr += str;
  }
  return hashStr;
}