// Thiết kế bởi RC Man
// Vi bước x 8
#include <LiquidCrystal_I2C.h>
#include <AccelStepper.h>
#include <Wire.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);

// ==========================================
bool COMMON_ANODE = true;  // Đặt TRUE nếu nối chung 5V. Đặt FALSE nếu nối chung GND.
// ==========================================

#define stepPin 2
#define dirPin 3
#define stepPin_2 5
#define dirPin_2 4
#define Switch 8
#define upButton A1
#define downButton A2
#define leftButton A0
#define rightButton A3
#define fButton 9

AccelStepper feeder(AccelStepper::DRIVER, stepPin, dirPin);
AccelStepper cutter(AccelStepper::DRIVER, stepPin_2, dirPin_2);

unsigned long wireLength = 0;
unsigned int stripLength1 = 0, stripLength2 = 0, wireQuantity = 0;
int state = 0, incrementSpeed = 1, wiresizePercent = 75, Cut = 1200;
float mmPerStep = 0.01415;
long prevVals[5] = { -1, -1, -1, -1, -1 };

// VỊ TRÍ CHỜ CỐ ĐỊNH - ĐIỂM GỐC CỦA DAO
const int SAFE_HOME = 500;

void setup() {
  lcd.init();
  lcd.backlight();
  pinMode(upButton, INPUT_PULLUP);
  pinMode(downButton, INPUT_PULLUP);
  pinMode(leftButton, INPUT_PULLUP);
  pinMode(rightButton, INPUT_PULLUP);
  pinMode(fButton, INPUT_PULLUP);
  pinMode(Switch, INPUT_PULLUP);

  if (COMMON_ANODE) {
    feeder.setPinsInverted(false, true, false);
    cutter.setPinsInverted(false, true, false);
  } else {
    feeder.setPinsInverted(false, false, false);
    cutter.setPinsInverted(false, false, false);
  }

  // Độ rộng xung
  feeder.setMinPulseWidth(15);
  cutter.setMinPulseWidth(15);

  feeder.setMaxSpeed(10000);
  feeder.setAcceleration(30000);
  cutter.setMaxSpeed(12000);
  cutter.setAcceleration(40000);

  lcd.setCursor(0, 0);
  lcd.print("  GO TO HOME >>>");

  while (digitalRead(Switch) == HIGH) {
    cutter.setSpeed(-1500);
    cutter.runSpeed();
  }

  cutter.setCurrentPosition(0);
  delay(200);
  cutter.runToNewPosition(SAFE_HOME);
}

// --- HÀM CẮT DÙNG TỌA ĐỘ TUYỆT ĐỐI + KHÓA CỨNG ---
void move_to_cut_pos(int steps) {
  // Đi tới điểm cắt dựa trên SAFE_HOME
  cutter.runToNewPosition(SAFE_HOME + steps);
}

void move_back_to_safe() {
  // Ép buộc quay về đúng điểm 350
  cutter.runToNewPosition(SAFE_HOME);

  // CHIÊU CUỐI: Reset lại tọa độ thư viện về đúng 350 để xóa sạch nhiễu tích lũy
  cutter.setCurrentPosition(SAFE_HOME);
  delay(50);
}

void cutting() {
  move_to_cut_pos(Cut);
  delay(80);
  move_back_to_safe();
  delay(80);
}

void currentlyCutting() {
  lcd.clear();
  int actualWiresizeSteps = map(wiresizePercent, 0, 100, 0, Cut - 150);

  for (int i = 0; i < wireQuantity; i++) {
    lcd.setCursor(0, 0);
    lcd.print("Cutting: " + (String)(i + 1) + "/" + (String)wireQuantity);

    // Xử lý đẩy dây dùng float chống tràn số
    long stripSteps1 = (long)((float)stripLength1 / mmPerStep);
    long stripSteps2 = (long)((float)stripLength2 / mmPerStep);
    long totalSteps = (long)((float)wireLength / mmPerStep);
    long middleSteps = totalSteps - (stripSteps1 + stripSteps2);

    if (stripLength1 == 0 && stripLength2 == 0) {
      feeder.runToNewPosition(feeder.currentPosition() + totalSteps);
      cutting();
    } else {
      feeder.runToNewPosition(feeder.currentPosition() + stripSteps1);
      move_to_cut_pos(actualWiresizeSteps);
      move_back_to_safe();

      feeder.runToNewPosition(feeder.currentPosition() + middleSteps);
      move_to_cut_pos(actualWiresizeSteps);
      move_back_to_safe();

      feeder.runToNewPosition(feeder.currentPosition() + stripSteps2);
      cutting();
    }
    if (digitalRead(leftButton) == LOW) {
      state = 0;
      break;
    }
  }
  state = 8;
  lcd.clear();
}

// --- MENU & CÁC HÀM KHÁC (GIỮ NGUYÊN ĐỂ BẠN DỄ DÙNG) ---
long changeValue(long val, int idx) {
  long limit = (idx == 4) ? 100 : 100000;
  if (!digitalRead(upButton)) {
    delay(100);
    val += incrementSpeed;
    if (val > limit) val = limit;
  }
  if (!digitalRead(downButton)) {
    delay(100);
    val = (val - (long)incrementSpeed < 0) ? 0 : val - incrementSpeed;
  }
  if (!digitalRead(fButton)) {
    delay(200);
    incrementSpeed = (incrementSpeed == 1) ? 10 : 1;
    lcd.clear();
  }
  if (prevVals[idx] != val) {
    lcd.clear();
    prevVals[idx] = val;
  }
  String titles[] = { "TOTAL LENGTH", "STRIP 1", "STRIP 2", "QUANTITY", "WIRE SIZE (%)" };
  lcd.setCursor(0, 0);
  lcd.print(titles[idx] + ":");
  lcd.setCursor(0, 1);
  if (idx == 4) {
    lcd.print((String)val + " %");
    if (val >= 100) {
      lcd.setCursor(12, 1);
      lcd.print("-> MAX");
    }
  } else {
    lcd.print((String)val + (idx < 3 ? " mm" : ""));
  }
  displayNav();
  return val;
}

void loop() {
  if (!digitalRead(rightButton)) {
    state = (state == 8) ? 0 : state + 1;
    delay(200);
    lcd.clear();
  }
  if (!digitalRead(leftButton) && state > 0 && state < 7) {
    state -= 1;
    delay(200);
    lcd.clear();
  }
  if (state == 0) {
    if (!digitalRead(upButton)) {
      feeder.move(400);
      feeder.runToPosition();
    }
    if (!digitalRead(downButton)) {
      feeder.move(-400);
      feeder.runToPosition();
    }
    if (!digitalRead(fButton)) cutting();
  }
  switch (state) {
    case 0: homeScreen(); break;
    case 1: wireLength = changeValue(wireLength, 0); break;
    case 2: stripLength1 = changeValue(stripLength1, 1); break;
    case 3: stripLength2 = changeValue(stripLength2, 2); break;
    case 4: wireQuantity = changeValue(wireQuantity, 3); break;
    case 5: wiresizePercent = (int)changeValue(wiresizePercent, 4); break;
    case 6: confirmScreen(); break;
    case 7: currentlyCutting(); break;
    case 8: finishedScreen(); break;
  }
}
void homeScreen() {
  lcd.setCursor(0, 0);
  lcd.print("  Wire Cut & Strip");
  lcd.setCursor(0, 1);
  lcd.print("^ Feed Forward");
  lcd.setCursor(0, 2);
  lcd.print("v Feed Reverse");
  lcd.setCursor(0, 3);
  lcd.print("* Cut          NEXT>");
}
void displayNav() {
  lcd.setCursor(0, 2);
  lcd.print("Step: x" + (String)incrementSpeed);
  lcd.setCursor(0, 3);
  lcd.print("<BACK          NEXT>");
}
void confirmScreen() {
  lcd.setCursor(0, 0);
  lcd.print("READY TO CUT?");
  lcd.setCursor(0, 1);
  lcd.print((String)wireLength + "mm x " + (String)wireQuantity + "pcs");
  lcd.setCursor(0, 2);
  lcd.print("WS: " + (String)wiresizePercent + "%");
  lcd.setCursor(0, 3);
  lcd.print("<BACK          START");
}
void finishedScreen() {
  lcd.setCursor(0, 1);
  lcd.print("  CUT COMPLETE!  ");
  lcd.setCursor(0, 3);
  lcd.print("               NEXT>");
}