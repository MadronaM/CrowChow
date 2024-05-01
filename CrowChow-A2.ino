#include <Wire.h>
#include <Shape.hpp>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define ROTATED_SCREEN_WIDTH 64
#define ROTATED_SCREEN_HEIGHT 128

#define NUM_FOOD 10
#define INTERVAL_MS 1000
#define BUZZER_PIN 7
#define TILT_THRESHOLD 7
#define MOTOR_PIN 5

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// I2C
Adafruit_LIS3DH lis = Adafruit_LIS3DH();
volatile sensors_event_t event;

unsigned long nextMoveTimes[NUM_FOOD];
int moveIntervals[NUM_FOOD] = {1000, 1500, 2000, 2500}; // Different intervals for each food

unsigned long previousMillis = 0;

class Food : public Rectangle {
  public: 
    boolean inSandwich = false;

    Food() : Rectangle(0, 0, 0, 0), inSandwich(false) {}

    Food(int x, int y, int width, int height) : Rectangle(x, y, width, height)
    {
    }
    bool getInSandwich() {
      return inSandwich;
    }

    bool setInSandwich(bool inSandwich) {
      inSandwich = inSandwich;
    }
};

Food _bread(10, -5, 29, 24);
Food _bacon(16, -5, 10, 2); // Bacon
Food _lettuce(20, -5, 9, 9); // Lettuce
Food _tomato(25, -5, 7, 7); // Tomato

Food foods[NUM_FOOD] = {
  _bacon,  // Bacon
  // _lettuce,  // Lettuce
  // _tomato,  // Tomato
  // _bread   // Bread
};

Food* falling[40];
int fallingCount;
Food* sandwich[NUM_FOOD];
int sandCount;

enum GameState {
  NEW_GAME,
  PLAYING,
  GAME_OVER
};

GameState _gameState = NEW_GAME;
unsigned long gameStartTime = 0;
int count = 0;

// 'crow_on_a_transparent_background__by_prussiaart_dbk9wsi-fullview', 29x24px
const unsigned char crow [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 
	0x00, 0x70, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x00, 
	0x00, 0x7e, 0x00, 0x00, 0x00, 0x7f, 0x80, 0x00, 0x00, 0x7f, 0xc0, 0x00, 0x00, 0x7f, 0xe0, 0x00, 
	0x00, 0x3f, 0xf0, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x0f, 0xfc, 0x00, 0x00, 0x07, 0xfe, 0x00, 
	0x00, 0x05, 0x0f, 0x80, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void setup() {
  Serial.begin(9600);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!_display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  // Initialize accelerometer
  if (!lis.begin(0x18)) {  
    Serial.println("Could not start accelerometer");
    while (1) yield();
  }

  lis.setRange(LIS3DH_RANGE_2_G);   // 2, 4, 8 or 16 G!
  lis.setDataRate(LIS3DH_DATARATE_10_HZ);

  pinMode(MOTOR_PIN, OUTPUT);

  for (int i = 0; i < NUM_FOOD; ++i) {
    nextMoveTimes[i] = millis() + random(100, 1000); // Random initial delay
  }

  _bacon.setDrawFill(true);

  _bread.setX(ROTATED_SCREEN_WIDTH / 2 - _bread.getWidth() / 2);
  _bread.setY(ROTATED_SCREEN_HEIGHT - _bread.getHeight());
  sandwich[0] = &_bread;
  falling[0] = &_bacon;
  sandCount = 1;
  fallingCount = 1;

  _display.setRotation(3);
  _display.clearDisplay();
  _display.display();
}

// void loop() {
//   unsigned long currentMillis = millis();
//   _display.clearDisplay();
//   _display.setRotation(3);

//   switch (_gameState) {
//     case NEW_GAME:
//       displayStartScreen();
//       if (abs(readAccelerometer()) >= TILT_THRESHOLD) {
//         _gameState = PLAYING;
//         gameStartTime = millis(); // Start game time
//       }
      
//       break;
    
//     case PLAYING:
//       unsigned long currentTime = millis();
//       unsigned long elapsedTime = currentTime - gameStartTime;
//       Serial.print("ElapsedTime: ");
//       Serial.println(elapsedTime);
//       // Life is short, too bad
//       if (elapsedTime >= 13000) {
//         _gameState = GAME_OVER;
//       }
//       // Add your playing logic here
//       moveSandwich();
//       moveAndCheckCollision();
//       break;

//     case GAME_OVER:
//       displayGameOverScreen();
//       delay(2000);
//       _gameState = NEW_GAME;
//       break;
//   }
//   _display.display();
// }

void loop() {
  unsigned long currentMillis = millis();
  _display.clearDisplay();
  _display.setRotation(3);
  int motorIntensity = 128;

  switch (_gameState) {
    case NEW_GAME:
    count = 0;
      displayStartScreen();

      // Check for tilt to start only when in NEW_GAME state
      if (_gameState == NEW_GAME && abs(readAccelerometer()) >= TILT_THRESHOLD) {
        _gameState = PLAYING;
        gameStartTime = millis(); // Start game time
        Serial.println("Game started");
      }
      break;
    
    case PLAYING:
      unsigned long currentTime = millis();
      unsigned long elapsedTime = currentTime - gameStartTime;
      Serial.print("ElapsedTime: ");
      Serial.println(elapsedTime);
      // Life is short, too bad
      if (elapsedTime >= 13000) {
        Serial.println("Game over: Time limit reached");
        _gameState = GAME_OVER;
        break; // Exit the case PLAYING loop
      }

      // Add your playing logic here
      moveSandwich();
      moveAndCheckCollision();
      if (count == 1) {
        happyTone();
        _gameState = GAME_OVER;
        displayGameOverScreen();
        Serial.println("after change to game over state");
      }
      _display.display();
      break;

    case GAME_OVER:
      Serial.println("game over");
      displayGameOverScreen();
      delay(2000);
      if (count == 0) {
        analogWrite(MOTOR_PIN, motorIntensity);
        delay(1000);
        analogWrite(MOTOR_PIN, 0);
      }
      _gameState = NEW_GAME;
      break;
  }
  _display.display();
}

void happyTone() {
  // Define the notes for the happy tone melody
  int melody[] = {262, 330, 392, 523, 392, 330, 262};
  int noteDuration = 200; // Duration of each note in milliseconds

  // Play the melody
  for (int i = 0; i < sizeof(melody) / sizeof(melody[0]); i++) {
    tone(BUZZER_PIN, melody[i], noteDuration);
    delay(noteDuration * 1.3); // Add a slight pause between notes
  }

  // Stop playing the tone
  noTone(BUZZER_PIN);
}


void displayStartScreen() {
  _display.clearDisplay();
  _display.setTextSize(2);      // Larger size for "BLT"
  _display.setTextColor(SSD1306_WHITE); // Draw white text
  _display.setRotation(3); // Rotate display 90 degrees clockwise
  _display.setCursor(ROTATED_SCREEN_WIDTH + 2, ROTATED_SCREEN_HEIGHT/ 4); // Center text vertically and horizontally
  _display.print("Crow");

  _display.setTextSize(1.5);
  _display.setCursor(ROTATED_SCREEN_WIDTH + 2, ROTATED_SCREEN_HEIGHT / 2); // Center text vertically and horizontally
  _display.print("Chow");

  _display.setTextSize(1);
  _display.setCursor(ROTATED_SCREEN_WIDTH + 2, ROTATED_SCREEN_HEIGHT / 2 + 30); // Move cursor down for "Tilt to start" and add a couple of newlines
  _display.print("\n\nTilt to start");

  _display.setRotation(1);
  _display.display();
  analogWrite(MOTOR_PIN, 255);
  delay(1000);
  analogWrite(MOTOR_PIN, 0);
  delay(1000);
}


void displayGameOverScreen() {
  _display.setRotation(3);
  _display.clearDisplay();
  _display.setTextSize(2);
  _display.setTextColor(SSD1306_WHITE);
  _display.setCursor(SCREEN_HEIGHT / 2 - 8, SCREEN_WIDTH / 4); // Center text vertically and horizontally
  _display.print("Game Over");
  _display.display();
}


int readAccelerometer() {
  lis.read();      // get X Y and Z data at once
  sensors_event_t event;
  lis.getEvent(&event);
  return event.acceleration.y;
}

//make sure y-value of collided item is locked
void moveAndCheckCollision() {
  for (int i = 0; i < sandCount; i++) {
    // Check collision with bread
    Food* lastFood = sandwich[sandCount - 1];
    Food* currFood = falling[i];
    // Serial.print(lastFood -> getX());
    // Serial.println("moving and checking collison");

    if (currFood->overlaps(*lastFood)) {
      count++;
      // tone(BUZZER_PIN, 2000, 200);
      happyTone();
      analogWrite(MOTOR_PIN, 124);
      delay(1000);
      analogWrite(MOTOR_PIN, 0);

      // Serial.println("Food collided with bread!");

      // add item to sandwich
      currFood->setY(lastFood->getY() + currFood->getHeight());
      currFood->setX(lastFood->getX() + currFood->getWidth());
      sandwich[sandCount] = currFood;
      delete falling[i];
      falling[i] = nullptr;
      fallingCount--;
      sandCount++;

      // Shift remaining items in the falling array to the left
      for (int j = i + 1; j < fallingCount; ++j) {
        falling[j - 1] = falling[j];
      }
      falling[fallingCount] = nullptr; // Set last element to nullptr
      // Serial.print("Falling array: ");
      // Serial.println(fallingCount);
      currFood->draw(_display);
      
      // update to next topping
    } else {
      
      // Move the food down the screen
      currFood->setY(currFood->getY() + 1);

      // If the food reaches the bottom, reset its position to the top
      if (currFood->getTop() > ROTATED_SCREEN_HEIGHT) {
        // Serial.println("Reset food position to top");
        currFood->setY(0 - currFood->getHeight());
        // You can add additional logic here if needed
      }
      currFood->draw(_display);
    }
  }
}

void moveSandwich() {
  int accValue = readAccelerometer();
  // Serial.print("accValue is: ");
  // Serial.println(accValue);
  // Serial.print("Number of items in sandwich: ");
  // Serial.println(sandCount);

  // Calculate the change in position based on accelerometer reading and define the range of movement for sandwich
  int dx = map(accValue, -10, 10, 0, ROTATED_SCREEN_WIDTH - _bread.getWidth()); // Map accelerometer range to desired sandwich movement range
  for (int i = 0; i < sandCount; i++) {
    (*sandwich[i]).setX(dx);
    if (sandwich[i] == &_bread) {
      _display.drawBitmap(_bread.getX(),  _bread.getY(), crow, 29, 24, WHITE);
    } else {
      (*sandwich[i]).draw(_display);
    }

  }
  _display.display();

  // Serial.print("dx is: ");
  // Serial.println(dx);
}


