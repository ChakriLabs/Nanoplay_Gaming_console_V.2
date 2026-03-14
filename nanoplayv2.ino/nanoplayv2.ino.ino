#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <EEPROM.h>

// --- DISPLAY ---
// SH1106 Driver for 1.3" OLED
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// --- CONTROLS (DO NOT CHANGE) ---
#define BTN_UP    6
#define BTN_DOWN  7
#define BTN_LEFT  8
#define BTN_RIGHT 9

// --- GAME STATES ---
enum GameState { STARTUP, MENU_SELECT, MENU_DIFFICULTY, PLAY_DINO, PLAY_SNAKE, PLAY_MARIO, PLAY_SPACE, PLAY_FLAPPY, GAME_OVER };
GameState currentState = STARTUP;

// --- GLOBAL VARIABLES ---
int selectedGame = 0; // 0=Dino, 1=Snake, 2=Mario, 3=Space, 4=Flappy
int difficulty = 1;   // 0=Easy, 1=Medium, 2=Hard
int score = 0;
int highScores[5];    
unsigned long lastFrame = 0;

// Inputs
bool bUp, bDown, bLeft, bRight;

// --- BITMAPS (PROGMEM) ---

// 1. DINO ASSETS (Real Google Style)
const unsigned char dino_run1_bits[] PROGMEM = {
  0x00,0x00,0x00,0x00,0x03,0xE0,0x03,0xE0,0x03,0xE0,0x03,0xC0,0x0F,0xF0,0x0F,0xF8,
  0x0F,0xFC,0x0F,0xF0,0x07,0xF0,0x03,0xF0,0x02,0x30,0x02,0x20,0x02,0x20,0x03,0x60
}; // 16x16
const unsigned char dino_run2_bits[] PROGMEM = {
  0x00,0x00,0x00,0x00,0x03,0xE0,0x03,0xE0,0x03,0xE0,0x03,0xC0,0x0F,0xF0,0x0F,0xF8,
  0x0F,0xFC,0x0F,0xF0,0x07,0xF0,0x03,0xF0,0x02,0x70,0x00,0x20,0x00,0x20,0x01,0x80
}; // 16x16
const unsigned char dino_duck_bits[] PROGMEM = { 
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0xF0,0x0F,0xF0,0xFF,0xF8,
  0xFF,0xFC,0x3F,0xF0,0x1F,0xF0,0x0F,0x30,0x08,0x20,0x08,0x20,0x0C,0x60,0x00,0x00
}; // 16x16

const unsigned char cactus_bits[] PROGMEM = { 0x18, 0x5A, 0x5A, 0xDB, 0xDB, 0xDB, 0x18, 0x18 };
const unsigned char bird_bits[] PROGMEM = { 0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81 };

// 2. FLAPPY BIRD ASSETS
const unsigned char flappy_bits[] PROGMEM = {
  0x00,0x78,0x1C,0x84,0x27,0x02,0x58,0x82,0x58,0x44,0x27,0x38,0x1C,0x00,0x00,0x00
}; // 16x8

// 3. SNAKE, MARIO, SPACE (Standard)
const unsigned char snake_head_bits[] PROGMEM = { 0x3C, 0x7E, 0xDB, 0xFF, 0xFF, 0x7E, 0x3C, 0x00 };
const unsigned char apple_bits[] PROGMEM = { 0x18, 0x24, 0x5A, 0xFF, 0xFF, 0x7E, 0x3C, 0x00 };
const unsigned char mario_bits[] PROGMEM = { 0x18,0x3C,0x3D,0xFE,0xFE,0x3F,0xEF,0x46,0x82,0x82 }; 
const unsigned char ship_bits[] PROGMEM = { 0x18,0x18,0x3C,0x7E,0xFF,0xFF,0x66,0x00 }; 
const unsigned char alien_bits[] PROGMEM = { 0x3C,0x7E,0xDB,0xFF,0xA5,0x42,0x00,0x00 }; 

// --- GAME VARIABLES ---

// Dino
float dinoY = 48;
float dinoVel = 0;
bool dinoDuck = false;
int dinoFrame = 0;
struct Obstacle { int x; int type; int y; }; 
Obstacle dinoObj = {128, 0, 50};

// Snake
struct Point { int x, y; };
Point snakeBody[50];
int snakeLen, snakeDir; 
Point foodPos;

// Mario
float marioY = 50, marioVel = 0;
int marioObjX = 128, coinX = 128, coinY = 30;

// Space
float shipX = 64;
struct Bullet { int x, y; bool active; };
Bullet bullet;
int alienX=0, alienY=0, alienDir=1;

// Flappy Bird
float birdY = 32;
float birdVel = 0;
int pipeX = 128;
int pipeGapY = 30; // Center of the gap
const int pipeGap = 26;

// ================= SETUP =================
void setup() {
  pinMode(BTN_UP, INPUT_PULLUP); pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP); pinMode(BTN_RIGHT, INPUT_PULLUP);
  
  // Init High Scores
  for(int i=0; i<5; i++) {
    int val = EEPROM.read(i*2);
    if(val == 255) { EEPROM.write(i*2, 0); highScores[i] = 0; }
    else highScores[i] = val;
  }

  randomSeed(analogRead(0));
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  // Startup Screen
  u8g2.firstPage();
  do {
    u8g2.drawFrame(2, 2, 124, 60);
    u8g2.drawStr(30, 20, "Welcome to");
    u8g2.drawStr(25, 38, "Chakri Stark");
    u8g2.drawStr(22, 54, "Game Station");
  } while ( u8g2.nextPage() );
  delay(3000); 
  
  currentState = MENU_SELECT;
}

// ================= MAIN LOOP =================
void loop() {
  bUp = !digitalRead(BTN_UP); bDown = !digitalRead(BTN_DOWN);
  bLeft = !digitalRead(BTN_LEFT); bRight = !digitalRead(BTN_RIGHT);

  int frameDelay = 30;
  if(difficulty == 0) frameDelay = 50; 
  else if(difficulty == 2) frameDelay = 15;

  if (millis() - lastFrame > frameDelay) { 
    lastFrame = millis();
    updateGameLogic();
  }

  u8g2.firstPage();
  do { drawGameGraphics(); } while ( u8g2.nextPage() );
}

// ================= LOGIC =================
void updateGameLogic() {
  switch (currentState) {
    case MENU_SELECT:
      if (bRight) { selectedGame++; if(selectedGame > 4) selectedGame=0; delay(200); }
      if (bLeft)  { selectedGame--; if(selectedGame < 0) selectedGame=4; delay(200); }
      if (bUp) { delay(200); currentState = MENU_DIFFICULTY; }
      break;

    case MENU_DIFFICULTY:
      if (bRight) { difficulty++; if(difficulty > 2) difficulty=0; delay(200); }
      if (bLeft)  { difficulty--; if(difficulty < 0) difficulty=2; delay(200); }
      if (bUp) { startGame(); delay(200); }
      break;

    case PLAY_DINO:   runDino(); break;
    case PLAY_SNAKE:  runSnake(); break;
    case PLAY_MARIO:  runMario(); break;
    case PLAY_SPACE:  runSpace(); break;
    case PLAY_FLAPPY: runFlappy(); break;

    case GAME_OVER:
      if (bUp || bDown || bLeft || bRight) {
        saveHighScore(); delay(500); currentState = MENU_SELECT; 
      }
      break;
  }
}

// ================= HELPERS =================
void startGame() {
  score = 0;
  if(selectedGame==0) { // Dino
    dinoY=48; dinoObj={128,0,50}; dinoDuck=false; currentState = PLAY_DINO;
  }
  else if(selectedGame==1) { // Snake
    snakeLen=3; snakeBody[0]={10,8}; snakeDir=1; foodPos={random(2,30), random(2,14)}; currentState = PLAY_SNAKE;
  }
  else if(selectedGame==2) { // Mario
    marioY=50; marioObjX=128; coinX=128; currentState = PLAY_MARIO;
  }
  else if(selectedGame==3) { // Space
    shipX=64; alienY=0; alienX=64; bullet.active=false; currentState = PLAY_SPACE;
  }
  else if(selectedGame==4) { // Flappy
    birdY=32; birdVel=0; pipeX=128; currentState = PLAY_FLAPPY;
  }
}

void saveHighScore() {
  if (score > highScores[selectedGame]) {
    highScores[selectedGame] = score;
    EEPROM.write(selectedGame*2, score);
  }
}

// ================= GAME ENGINES =================

// --- 1. DINO RUN (Updated Look) ---
void runDino() {
  dinoDuck = bDown;
  if (bUp && dinoY >= 48) dinoVel = -6;
  
  dinoY += dinoVel;
  if (dinoY < 48) dinoVel += 0.8; else { dinoY = 48; dinoVel = 0; }
  
  dinoFrame++; // For animation

  dinoObj.x -= (4 + difficulty);
  if (dinoObj.x < -10) { 
    dinoObj.x = 128; score++; 
    dinoObj.type = random(0, 10) > 7 ? 1 : 0; 
    dinoObj.y = (dinoObj.type == 1) ? 35 : 50; // 35 is bird height
  }

  // Hitbox
  if (dinoObj.x < 24 && dinoObj.x > 8) {
    if (dinoObj.type == 0 && dinoY > 40) currentState = GAME_OVER; // Hit Cactus
    if (dinoObj.type == 1 && !dinoDuck && dinoY > 30) currentState = GAME_OVER; // Hit Bird
  }
}

// --- 2. FLAPPY BIRD (Replaces Traffic) ---
void runFlappy() {
  // Physics
  if (bUp) birdVel = -3; // Flap
  birdY += birdVel;
  birdVel += 0.4; // Gravity

  // Pipe Movement
  pipeX -= (2 + difficulty);
  if(pipeX < -12) {
    pipeX = 128;
    pipeGapY = random(15, 50); // Random height
    score++;
  }

  // Collisions
  if(birdY > 63 || birdY < 0) currentState = GAME_OVER; // Floor/Ceiling
  
  // Pipe Collision (Left/Right bounds of pipe)
  if(pipeX < 24 && pipeX > 4) {
    // Check if bird is NOT in the gap
    if(birdY < (pipeGapY - pipeGap/2) || birdY > (pipeGapY + pipeGap/2)) {
      currentState = GAME_OVER;
    }
  }
}

// --- 3. SNAKE ---
void runSnake() {
  static int moveCounter = 0;
  if(++moveCounter < (3 - difficulty)) return; moveCounter = 0;

  if (bUp && snakeDir != 2) snakeDir = 0;
  if (bRight && snakeDir != 3) snakeDir = 1;
  if (bDown && snakeDir != 0) snakeDir = 2;
  if (bLeft && snakeDir != 1) snakeDir = 3;

  for (int i = snakeLen; i > 0; i--) snakeBody[i] = snakeBody[i - 1];
  if (snakeDir == 0) snakeBody[0].y--; if (snakeDir == 1) snakeBody[0].x++;
  if (snakeDir == 2) snakeBody[0].y++; if (snakeDir == 3) snakeBody[0].x--;

  if (snakeBody[0].x < 0 || snakeBody[0].x > 31 || snakeBody[0].y < 1 || snakeBody[0].y > 15) currentState = GAME_OVER;
  for (int i = 1; i < snakeLen; i++) if (snakeBody[0].x == snakeBody[i].x && snakeBody[0].y == snakeBody[i].y) currentState = GAME_OVER;

  if (snakeBody[0].x == foodPos.x && snakeBody[0].y == foodPos.y) {
    score++; if(snakeLen < 39) snakeLen++; foodPos = {random(1,30), random(2,14)};
  }
}

// --- 4. MARIO ---
void runMario() {
  if (bUp && marioY >= 50) marioVel = -7;
  marioY += marioVel;
  if (marioY < 50) marioVel += 0.8; else { marioY = 50; marioVel = 0; }
  
  marioObjX -= (3 + difficulty); coinX -= (3 + difficulty);
  if (marioObjX < -10) marioObjX = 128 + random(0,50);
  if (coinX < -10) { coinX = 128 + random(20,100); coinY = random(25, 40); }
  
  if (abs(coinX - 10) < 10 && abs(marioY - coinY) < 15) { score += 5; coinX = -20; }
  if (marioObjX < 20 && marioObjX > 5 && marioY > 42) currentState = GAME_OVER;
  if(millis() % 20 == 0) score++;
}

// --- 5. SPACE ---
void runSpace() {
  if(bLeft && shipX > 2) shipX-=3; if(bRight && shipX < 118) shipX+=3;
  if(bUp && !bullet.active) bullet = {(int)shipX+3, 50, true};
  if(bullet.active) { bullet.y -= 5; if(bullet.y < 0) bullet.active = false; }
  
  alienX += alienDir * (2 + difficulty);
  if(alienX > 118 || alienX < 0) { alienDir *= -1; alienY += 6; }
  if(alienY > 50) currentState = GAME_OVER;
  
  if(bullet.active && abs(bullet.x - alienX) < 10 && abs(bullet.y - alienY) < 10) {
    score += 10; bullet.active = false; alienY = 0; alienX = random(10, 100);
  }
}

// ================= DRAWING =================
void drawGameGraphics() {
  if (currentState == MENU_SELECT) {
    u8g2.drawFrame(0,0,128,64);
    u8g2.drawStr(32, 12, "GAME MENU");
    u8g2.drawHLine(0, 14, 128);
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.setCursor(10, 40);
    if(selectedGame==0) u8g2.print("< DINO RUN >");
    else if(selectedGame==1) u8g2.print("< SNAKE >");
    else if(selectedGame==2) u8g2.print("< MARIO >");
    else if(selectedGame==3) u8g2.print("< SPACE >");
    else if(selectedGame==4) u8g2.print("< FLAPPY >");
    
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setCursor(20, 58); u8g2.print("High Score: "); u8g2.print(highScores[selectedGame]);
  }
  else if (currentState == MENU_DIFFICULTY) {
    u8g2.drawStr(30, 20, "DIFFICULTY:");
    u8g2.setCursor(25, 45);
    if(difficulty==0) u8g2.print("<  EASY  >");
    else if(difficulty==1) u8g2.print("< MEDIUM >");
    else u8g2.print("<  HARD  >");
  }
  else if (currentState == GAME_OVER) {
    u8g2.drawStr(35, 30, "GAME OVER");
    u8g2.setCursor(40, 50); u8g2.print("Score: "); u8g2.print(score);
  }
  else {
    u8g2.setCursor(100, 10); u8g2.print(score);

    if (currentState == PLAY_DINO) {
      // Draw Dino (Animation)
      if(dinoDuck) {
        u8g2.drawXBMP(10, (int)dinoY+6, 16, 16, dino_duck_bits); // Ducking
      } else {
        // Switch frames every 4 ticks
        if((dinoFrame/4) % 2 == 0) u8g2.drawXBMP(10, (int)dinoY, 16, 16, dino_run1_bits);
        else u8g2.drawXBMP(10, (int)dinoY, 16, 16, dino_run2_bits);
      }
      
      // Draw Obstacle
      if(dinoObj.type == 0) u8g2.drawXBMP(dinoObj.x, 50, 8, 8, cactus_bits);
      else u8g2.drawXBMP(dinoObj.x, dinoObj.y, 8, 8, bird_bits); // Flying Bird
      
      u8g2.drawHLine(0, 62, 128);
    }
    else if (currentState == PLAY_SNAKE) {
      u8g2.drawFrame(0,0,128,64); 
      for(int i=1; i<snakeLen; i++) u8g2.drawDisc(snakeBody[i].x*4 + 2, snakeBody[i].y*4 + 2, 2);
      u8g2.drawXBMP(snakeBody[0].x*4, snakeBody[0].y*4, 8, 8, snake_head_bits);
      u8g2.drawXBMP(foodPos.x*4, foodPos.y*4, 8, 8, apple_bits);
    }
    else if (currentState == PLAY_FLAPPY) {
      // Draw Bird
      u8g2.drawXBMP(16, (int)birdY, 16, 8, flappy_bits);
      // Draw Pipes with Rims
      int topH = pipeGapY - pipeGap/2;
      int botY = pipeGapY + pipeGap/2;
      int botH = 64 - botY;
      
      // Top Pipe
      u8g2.drawBox(pipeX+2, 0, 10, topH - 4); // Body
      u8g2.drawBox(pipeX, topH - 4, 14, 4);   // Rim
      
      // Bottom Pipe
      u8g2.drawBox(pipeX, botY, 14, 4);       // Rim
      u8g2.drawBox(pipeX+2, botY+4, 10, botH-4); // Body
    }
    else if (currentState == PLAY_MARIO) {
      u8g2.drawXBMP(10, (int)marioY, 10, 10, mario_bits);
      u8g2.drawBox(marioObjX, 50, 10, 10);
      u8g2.drawDisc(coinX, coinY, 3);
      u8g2.drawHLine(0, 60, 128);
    }
    else if (currentState == PLAY_SPACE) {
      u8g2.drawXBMP((int)shipX, 54, 8, 8, ship_bits);
      u8g2.drawXBMP(alienX, alienY, 8, 8, alien_bits);
      if(bullet.active) u8g2.drawVLine(bullet.x, bullet.y, 4);
    }
  }
}


