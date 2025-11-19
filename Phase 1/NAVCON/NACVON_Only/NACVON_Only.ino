/*
  NACVON_Only.ino — Full NAVCON with separate Debug and Data ports
  - Debug output goes to USB Serial (Serial Monitor)
  - Data packets go to GPIO 16/17 for Python communication
  - Complete functionality preserved
*/

#include <Arduino.h>

// ==================== Configuration ====================
#ifndef DEBUG_BAUD
#define DEBUG_BAUD 115200
#endif
#ifndef DATA_BAUD
#define DATA_BAUD 115200
#endif

// ESP32 Pin Configuration
#define DATA_RX_PIN 16    // ESP32 receives Python data on GPIO 16
#define DATA_TX_PIN 17    // ESP32 sends to Python on GPIO 17

// Serial Port Definitions
#define DEBUG_SERIAL Serial              // USB Serial for debug output
HardwareSerial DATA_SERIAL(1);           // Hardware Serial1 for Python communication

// ==================== Constants ====================
static const int ANGLE_TOLERANCE = 5;
static const int ANGLE_HIGH_THRESHOLD = 45;
static const int STEERING_CORRECTION = 5;
static const int MIN_SPEED = 10;
static const int MAX_SPEED = 255;
static const uint32_t SENSOR_TIMEOUT_MS = 2000;
static const uint32_t SEQUENCE_TIMEOUT_MS = 5000;

// Defensive color macro removal
#ifdef WHITE
#undef WHITE
#endif
#ifdef BLACK
#undef BLACK
#endif
#ifdef RED
#undef RED
#endif
#ifdef GREEN
#undef GREEN
#endif
#ifdef BLUE
#undef BLUE
#endif

// ==================== ENUMS ====================
enum SystemState : uint8_t { SYS_IDLE=0, SYS_CAL=1, SYS_MAZE=2, SYS_SOS=3 };
enum SubsystemID : uint8_t { SUB_HUB=0, SUB_SNC=1, SUB_MDPS=2, SUB_SS=3 };
enum ColorType : uint8_t { WHITE=0, RED=1, GREEN=2, BLUE=3, BLACK=4, INVALID=255 };
enum NAVCONAction : uint8_t {
  ACT_STOP = 0,
  ACT_FORWARD,
  ACT_BACKWARD,
  ACT_TURN_LEFT,
  ACT_TURN_RIGHT,
  ACT_TURN_180_LEFT,
  ACT_TURN_180_RIGHT,
  ACT_TURN_360_LEFT,
  ACT_DIFFERENTIAL_STEER,
  ACT_WALL_FOLLOW_TURN,
  ACT_STEERING_CORRECTION,
  ACT_ERROR_STOP
};
enum WallState : uint8_t { WF_IDLE=0, WF_REVERSE, WF_TURN, WF_FORWARD };

// ==================== FORWARD DECLARATIONS ====================
SystemState getSystemState(uint8_t c);
SubsystemID getSubsystemID(uint8_t c);
uint8_t getInternalState(uint8_t c);
uint8_t makeControl(SystemState s, SubsystemID sub, uint8_t ist);
uint8_t clamp8(int v);
const char* colorToString(ColorType c);
ColorType decodeColor(uint8_t colorBits);
const char* actionToString(NAVCONAction a);

// ==================== STRUCT DEFINITIONS ====================
struct SCSPacket {
  uint8_t control;
  uint8_t dat1;
  uint8_t dat0;
  uint8_t dec;
  
  SCSPacket() : control(0), dat1(0), dat0(0), dec(0) {}
  
  bool isEmpty() const { 
    return control == 0 && dat1 == 0 && dat0 == 0 && dec == 0; 
  }
};

struct SensorData {
  ColorType s1, s2, s3;
  int16_t theta;
  bool haveColours;
  bool haveAngle;
  uint32_t tColours;
  uint32_t tAngle;

  SensorData() : s1(WHITE), s2(WHITE), s3(WHITE), theta(0), 
                 haveColours(false), haveAngle(false), tColours(0), tAngle(0) {}

  bool isDataFresh(uint32_t maxAge = SENSOR_TIMEOUT_MS) const {
    uint32_t now = millis();
    return haveColours && haveAngle && (now - tColours) <= maxAge && (now - tAngle) <= maxAge;
  }
  
  bool isAngleValid() const { 
    return haveAngle && theta >= -45 && theta <= 45; 
  }
  
  int16_t getAbsAngle() const { 
    return abs(theta); 
  }

  bool hasObstacle() const {
    return (s1 == BLACK || s1 == BLUE || s2 == BLACK || s2 == BLUE || s3 == BLACK || s3 == BLUE);
  }
  
  bool hasGreenLine() const { 
    return (s1 == GREEN || s2 == GREEN || s3 == GREEN); 
  }
  
  bool hasRedLine() const { 
    return (s1 == RED || s2 == RED || s3 == RED); 
  }
  
  bool isEndOfMaze() const { 
    return (s1 == RED && s2 == RED && s3 == RED); 
  }
  
  bool hasObstacleOnLeft() const { 
    return (s1 == BLACK || s1 == BLUE); 
  }
  
  bool hasObstacleOnRight() const { 
    return (s3 == BLACK || s3 == BLUE); 
  }
  
  bool hasObstacleOnBothSides() const { 
    return hasObstacleOnLeft() && hasObstacleOnRight(); 
  }
};

struct MotorData {
  int16_t vL, vR;
  uint16_t dist;
  int16_t lastRot;
  uint8_t rotDir;
  bool haveSpeed, haveDist, haveRot;
  uint32_t tSpeed, tDist, tRot;

  MotorData() : vL(0), vR(0), dist(0), lastRot(0), rotDir(0),
                haveSpeed(false), haveDist(false), haveRot(false),
                tSpeed(0), tDist(0), tRot(0) {}
};

struct Settings {
  int16_t vop;
  int16_t vcor;
  bool haveCal;

  Settings() : vop(50), vcor(25), haveCal(false) {}

  void updateCalibration(int16_t vL, int16_t vR) {
    int16_t avgSpeed = (vL + vR) / 2;
    vop = constrain(avgSpeed, MIN_SPEED, MAX_SPEED);
    vcor = max(MIN_SPEED, vop / 2);
    haveCal = true;
    DEBUG_SERIAL.printf("Settings updated: vL=%d vR=%d → vop=%d vcor=%d\n", vL, vR, vop, vcor);
  }
};

struct NAVCONCommand {
  NAVCONAction action;
  int16_t leftSpeed, rightSpeed;
  int16_t rotationDeg;
  uint8_t rotationDir;
  String why;
  String specification;
  bool ready;
  uint32_t timestamp;

  NAVCONCommand() : action(ACT_STOP), leftSpeed(0), rightSpeed(0), rotationDeg(0),
                    rotationDir(2), ready(false), timestamp(0) {}

  void setSpeedCommand(int16_t left, int16_t right, const String& reason) {
    leftSpeed = constrain(left, 0, MAX_SPEED);
    rightSpeed = constrain(right, 0, MAX_SPEED);
    why = reason;
    ready = true;
    timestamp = millis();
  }

  void setRotationCommand(int16_t degrees, uint8_t direction, const String& reason) {
    rotationDeg = abs(degrees);
    rotationDir = direction;
    why = reason;
    ready = true;
    timestamp = millis();
  }
};

struct WallFollow {
  WallState state;
  int16_t detectedAngle;
  int16_t calculatedTurn;
  bool obstacleOnLeft;
  uint32_t stateStartTime;

  WallFollow() : state(WF_IDLE), detectedAngle(0), calculatedTurn(0),
                 obstacleOnLeft(false), stateStartTime(0) {}

  void initiate(int16_t angle, bool leftSide = false) {
    state = WF_REVERSE;
    detectedAngle = angle;
    calculatedTurn = 90 + abs(angle);
    obstacleOnLeft = leftSide;
    stateStartTime = millis();
    DEBUG_SERIAL.printf("Wall follow initiated: angle=%d°, turn=%d°, side=%s\n",
                        angle, calculatedTurn, leftSide ? "LEFT" : "RIGHT");
  }

  void reset() {
    state = WF_IDLE;
    detectedAngle = 0;
    calculatedTurn = 0;
    obstacleOnLeft = false;
    stateStartTime = 0;
  }

  bool isActive() const { 
    return state != WF_IDLE; 
  }

  bool isTimedOut() const { 
    return isActive() && (millis() - stateStartTime) > SEQUENCE_TIMEOUT_MS; 
  }

  bool advance() {
    if (!isActive()) return false;
    switch(state) {
      case WF_REVERSE: state = WF_TURN; return true;
      case WF_TURN: state = WF_FORWARD; return true;
      case WF_FORWARD: reset(); return false;
      default: return false;
    }
  }
};

struct MazeSM {
  SubsystemID expectSub;
  uint8_t expectIST;
  bool strict;
  uint32_t lastPacketTime;

  MazeSM() : expectSub(SUB_SS), expectIST(1), strict(false), lastPacketTime(0) {}

  void reset() {
    expectSub = SUB_SS;
    expectIST = 1;
    strict = false;
    lastPacketTime = 0;
  }

  void advanceAfter(const SCSPacket &p) {
    SubsystemID sub = getSubsystemID(p.control);
    uint8_t ist = getInternalState(p.control);
    lastPacketTime = millis();
    
    if (sub == SUB_SS && ist == 1) {
      expectSub = SUB_SS;
      expectIST = 2;
    } else if (sub == SUB_SS && ist == 2) {
      expectSub = SUB_MDPS;
      expectIST = 1;
    } else if (sub == SUB_MDPS && ist == 1) {
      expectSub = SUB_MDPS;
      expectIST = 2;
    } else if (sub == SUB_MDPS && ist == 2) {
      expectSub = SUB_MDPS;
      expectIST = 3;
    } else if (sub == SUB_MDPS && ist == 3) {
      expectSub = SUB_MDPS;
      expectIST = 4;
    } else if (sub == SUB_MDPS && ist == 4) {
      expectSub = SUB_SS;
      expectIST = 1;
    }
  }

  bool isExpected(const SCSPacket &p) const {
    if (!strict) return true;
    return (getSubsystemID(p.control) == expectSub && getInternalState(p.control) == expectIST);
  }

  bool isStale() const {
    return lastPacketTime > 0 && (millis() - lastPacketTime) > SENSOR_TIMEOUT_MS;
  }
};

struct NAVCONState {
  SensorData sensors;
  MotorData motors;
  Settings settings;
  WallFollow wallFollow;
  MazeSM stateMachine;
  uint32_t decisionCycle;
  uint32_t errorCount;
  bool inMazeState;
  bool startupSent;

  NAVCONState() : decisionCycle(0), errorCount(0), inMazeState(false), startupSent(false) {}

  void reset() {
    sensors = SensorData();
    motors = MotorData();
    settings = Settings();
    wallFollow.reset();
    stateMachine.reset();
    decisionCycle = 0;
    errorCount = 0;
    inMazeState = false;
    startupSent = false;
  }

  bool hasValidInputs() const {
    return sensors.isDataFresh() && sensors.isAngleValid() && settings.haveCal;
  }

  void incrementError() {
    errorCount++;
    if (errorCount >= 5) {
      DEBUG_SERIAL.println("ERROR: Too many consecutive errors, resetting wall follow");
      wallFollow.reset();
      errorCount = 0;
    }
  }
};

// ==================== HELPER FUNCTION IMPLEMENTATIONS ====================
SystemState getSystemState(uint8_t c) { 
  return (SystemState)((c>>6)&0x03); 
}

SubsystemID getSubsystemID(uint8_t c) { 
  return (SubsystemID)((c>>4)&0x03); 
}

uint8_t getInternalState(uint8_t c) { 
  return (c & 0x0F); 
}

uint8_t makeControl(SystemState s, SubsystemID sub, uint8_t ist) {
  return ((s&0x03)<<6)|((sub&0x03)<<4)|(ist&0x0F);
}

uint8_t clamp8(int v) { 
  return (uint8_t)constrain(v, 0, 255); 
}

const char* colorToString(ColorType c) {
  switch(c) {
    case WHITE: return "WHITE";
    case RED: return "RED";
    case GREEN: return "GREEN";
    case BLUE: return "BLUE";
    case BLACK: return "BLACK";
    case INVALID: return "INVALID";
    default: return "UNKNOWN";
  }
}

ColorType decodeColor(uint8_t colorBits) {
  colorBits &= 0x07;
  if (colorBits <= BLACK) return (ColorType)colorBits;
  return INVALID;
}

const char* actionToString(NAVCONAction a) {
  switch(a) {
    case ACT_STOP: return "STOP";
    case ACT_FORWARD: return "FORWARD";
    case ACT_BACKWARD: return "BACKWARD";
    case ACT_TURN_LEFT: return "TURN_LEFT";
    case ACT_TURN_RIGHT: return "TURN_RIGHT";
    case ACT_TURN_180_LEFT: return "TURN_180_LEFT";
    case ACT_TURN_180_RIGHT: return "TURN_180_RIGHT";
    case ACT_TURN_360_LEFT: return "TURN_360_LEFT";
    case ACT_DIFFERENTIAL_STEER: return "DIFFERENTIAL_STEER";
    case ACT_WALL_FOLLOW_TURN: return "WALL_FOLLOW_TURN";
    case ACT_STEERING_CORRECTION: return "STEERING_CORRECTION";
    case ACT_ERROR_STOP: return "ERROR_STOP";
    default: return "UNKNOWN";
  }
}

// Global instance - declared after all struct definitions and helper functions
NAVCONState navcon;

// ==================== CORE FUNCTIONS ====================
bool validatePacketSemantics(const SCSPacket &p) {
  SystemState sys = getSystemState(p.control);
  SubsystemID sub = getSubsystemID(p.control);
  uint8_t ist = getInternalState(p.control);

  if (sys > SYS_SOS) return false;
  if (sub > SUB_SS) return false;

  switch(sub) {
    case SUB_SS:
      if (!(ist == 1 || ist == 2 || ist == 3)) return false;
      break;
    case SUB_MDPS:
      if (!(ist == 0 || ist == 1 || ist == 2 || ist == 3 || ist == 4)) return false;
      if (ist == 2 && !(p.dec == 2 || p.dec == 3)) return false;
      break;
    case SUB_SNC:
      if (!(ist == 1 || ist == 2 || ist == 3)) return false;
      break;
    case SUB_HUB:
      break;
  }
  return true;
}

NAVCONCommand makeErrorCommand(const String& reason) {
  NAVCONCommand cmd;
  cmd.action = ACT_ERROR_STOP;
  cmd.leftSpeed = 0;
  cmd.rightSpeed = 0;
  cmd.why = reason;
  cmd.specification = "Error Handling";
  cmd.ready = true;
  cmd.timestamp = millis();
  return cmd;
}

NAVCONCommand decideNavigationAction() {
  NAVCONCommand cmd;
  cmd.ready = false;
  cmd.timestamp = millis();

  if (!navcon.hasValidInputs()) {
    if (!navcon.sensors.isDataFresh())
      return makeErrorCommand("Stale sensor data (timeout=" + String(SENSOR_TIMEOUT_MS) + "ms)");
    if (!navcon.sensors.isAngleValid())
      return makeErrorCommand("Invalid angle: " + String(navcon.sensors.theta) + "° (must be -45° to +45°)");
    if (!navcon.settings.haveCal)
      return makeErrorCommand("No calibration data available");
    return makeErrorCommand("Unknown input validation failure");
  }

  if (navcon.wallFollow.isTimedOut()) {
    DEBUG_SERIAL.println("Wall follow sequence timed out, resetting");
    navcon.wallFollow.reset();
  }

  const SensorData& s = navcon.sensors;
  const int16_t angle = s.theta;
  const int16_t absAngle = s.getAbsAngle();

  DEBUG_SERIAL.printf("NAVCON Decision: S1=%s S2=%s S3=%s θ=%d° (|θ|=%d°)\n",
                      colorToString(s.s1), colorToString(s.s2), colorToString(s.s3), angle, absAngle);
  
  // Debug obstacle detection
  DEBUG_SERIAL.printf("🔍 Obstacle Debug: hasObstacle=%s, hasLeft=%s, hasRight=%s, hasBothSides=%s\n",
                      s.hasObstacle() ? "YES" : "NO",
                      s.hasObstacleOnLeft() ? "YES" : "NO", 
                      s.hasObstacleOnRight() ? "YES" : "NO",
                      s.hasObstacleOnBothSides() ? "YES" : "NO");

  // Reset wall following if we detect both sides blocked (emergency situation)
  if (s.hasObstacleOnBothSides()) {
    DEBUG_SERIAL.println("🚨 Both sides blocked - resetting any active wall following");
    navcon.wallFollow.reset();
  }

  // End of maze
  if (s.isEndOfMaze()) {
    cmd.action = ACT_TURN_360_LEFT;
    cmd.setRotationCommand(360, 2, "End of maze detected - all sensors RED");
    cmd.specification = "MARV Spec 8: End of Maze";
    DEBUG_SERIAL.println("✅ END OF MAZE: 360° LEFT turn");
    return cmd;
  }

  // Continue wall following sequence if active
  if (navcon.wallFollow.isActive()) {
    switch(navcon.wallFollow.state) {
      case WF_REVERSE:
        cmd.action = ACT_BACKWARD;
        cmd.setSpeedCommand(navcon.settings.vcor, navcon.settings.vcor, "Wall follow step 1: Reverse from obstacle");
        cmd.specification = "MARV Spec 6: Wall Following";
        navcon.wallFollow.advance();
        break;
      case WF_TURN:
        cmd.action = ACT_WALL_FOLLOW_TURN;
        cmd.setRotationCommand(navcon.wallFollow.calculatedTurn, 3, "Wall follow step 2: " + String(navcon.wallFollow.calculatedTurn) + "° RIGHT turn");
        cmd.specification = "MARV Spec 6: Wall Following";
        navcon.wallFollow.advance();
        break;
      case WF_FORWARD:
        cmd.action = ACT_FORWARD;
        cmd.setSpeedCommand(navcon.settings.vop, navcon.settings.vop, "Wall follow step 3: Forward parallel to wall");
        cmd.specification = "MARV Spec 6: Wall Following";
        navcon.wallFollow.advance();
        break;
      default:
        navcon.wallFollow.reset();
        break;
    }
    return cmd;
  }

  // Both sides blocked: emergency
  if (s.hasObstacleOnBothSides()) {
    cmd.action = ACT_TURN_180_RIGHT;
    cmd.setRotationCommand(180, 3, "Obstacles on both sides - emergency 180° turn");
    cmd.specification = "Emergency Obstacle Avoidance";
    DEBUG_SERIAL.println("⚠️ BOTH SIDES BLOCKED: 180° RIGHT turn");
    return cmd;
  }

  // Both sides blocked: emergency (CHECK THIS BEFORE general obstacle!)
  if (s.hasObstacleOnBothSides()) {
    cmd.action = ACT_TURN_180_RIGHT;
    cmd.setRotationCommand(180, 3, "Obstacles on both sides - emergency 180° turn");
    cmd.specification = "Emergency Obstacle Avoidance";
    DEBUG_SERIAL.println("⚠️ BOTH SIDES BLOCKED: 180° RIGHT turn");
    return cmd;
  }

  // Red line
  if (s.hasRedLine()) {
    if (absAngle <= ANGLE_TOLERANCE) {
      cmd.action = ACT_FORWARD;
      cmd.setSpeedCommand(navcon.settings.vop, navcon.settings.vop, "Red line at acceptable angle (" + String(angle) + "° ≤ 5°)");
      cmd.specification = "MARV Spec 4: Line Crossing";
      DEBUG_SERIAL.println("✅ RED LINE: Cross forward");
    } else if (absAngle >= ANGLE_HIGH_THRESHOLD) {
      cmd.action = ACT_STEERING_CORRECTION;
      cmd.setRotationCommand(STEERING_CORRECTION, (angle > 0) ? 3 : 2, "Red line high angle (" + String(angle) + "° > 45°) - steer toward perpendicular");
      cmd.specification = "MARV Spec 5: High Angle Correction";
      DEBUG_SERIAL.printf("⚠️ RED LINE: High angle correction - angle=%d° → dir=%s\n", angle, (angle > 0) ? "RIGHT" : "LEFT");
    } else {
      cmd.action = ACT_BACKWARD;
      cmd.setSpeedCommand(navcon.settings.vcor, navcon.settings.vcor, "Red line moderate angle (" + String(angle) + "°) - reverse and re-approach");
      cmd.specification = "MARV Spec 4: Line Approach";
      DEBUG_SERIAL.println("⚠️ RED LINE: Reverse and re-approach");
    }
    return cmd;
  }

  // Green line
  if (s.hasGreenLine()) {
    if (absAngle <= ANGLE_TOLERANCE) {
      cmd.action = ACT_FORWARD;
      cmd.setSpeedCommand(navcon.settings.vop, navcon.settings.vop, "Green line at acceptable angle (" + String(angle) + "° ≤ 5°)");
      cmd.specification = "MARV Spec 4: Line Crossing";
      DEBUG_SERIAL.println("✅ GREEN LINE: Cross forward");
    } else if (absAngle >= ANGLE_HIGH_THRESHOLD) {  // Make sure this uses >= not >
      cmd.action = ACT_STEERING_CORRECTION;
      cmd.setRotationCommand(STEERING_CORRECTION, (angle > 0) ? 2 : 3, "Green line high angle (" + String(angle) + "° ≥ 45°) - steer toward line");
      cmd.specification = "MARV Spec 5: High Angle Correction";
      DEBUG_SERIAL.printf("⚠️ GREEN LINE: High angle correction - angle=%d° → dir=%s\n", angle, (angle > 0) ? "LEFT" : "RIGHT");
    } else {
      cmd.action = ACT_BACKWARD;
      cmd.setSpeedCommand(navcon.settings.vcor, navcon.settings.vcor, "Green line moderate angle (" + String(angle) + "°) - reverse and re-approach");
      cmd.specification = "MARV Spec 4: Line Approach";
      DEBUG_SERIAL.println("⚠️ GREEN LINE: Reverse and re-approach");
    }
    return cmd;
  }

  // Black/Blue obstacle
  if (s.hasObstacle()) {
    if (absAngle <= ANGLE_HIGH_THRESHOLD) {
      bool leftSide = s.hasObstacleOnLeft();
      navcon.wallFollow.initiate(angle, leftSide);
      cmd.action = ACT_BACKWARD;
      cmd.setSpeedCommand(navcon.settings.vcor, navcon.settings.vcor, "Obstacle at " + String(angle) + "° - initiate wall following");
      cmd.specification = "MARV Spec 6: Wall Following";
      DEBUG_SERIAL.printf("⚠️ OBSTACLE: Start wall follow (θ=%d°, side=%s)\n", angle, leftSide ? "LEFT" : "RIGHT");
    } else {
      cmd.action = ACT_STEERING_CORRECTION;
      cmd.setRotationCommand(STEERING_CORRECTION, (angle > 0) ? 2 : 3, "Obstacle high angle (" + String(angle) + "° > 45°) - steer away");
      cmd.specification = "MARV Spec 7: High Angle Avoidance";
      DEBUG_SERIAL.printf("⚠️ OBSTACLE: High angle - steer away - angle=%d° → dir=%s\n", angle, (angle > 0) ? "LEFT" : "RIGHT");
    }
    return cmd;
  }

  // Clear path with micro-corrections
  if (s.s1 == WHITE && s.s2 == WHITE && s.s3 == WHITE) {
    if (absAngle > 2 && absAngle <= ANGLE_TOLERANCE) {
      int16_t speedDiff = min<int16_t>(absAngle * 2, navcon.settings.vop / 3);
      cmd.action = ACT_DIFFERENTIAL_STEER;
      if (angle > 0) {
        cmd.setSpeedCommand(navcon.settings.vop, navcon.settings.vop - speedDiff,
                            "Clear path small angle (" + String(angle) + "°) - differential steering");
      } else {
        cmd.setSpeedCommand(navcon.settings.vop - speedDiff, navcon.settings.vop,
                            "Clear path small angle (" + String(angle) + "°) - differential steering");
      }
      cmd.specification = "Path Following Optimization";
      DEBUG_SERIAL.printf("✅ CLEAR PATH: Differential steering (diff=%d)\n", speedDiff);
    } else {
      cmd.action = ACT_FORWARD;
      cmd.setSpeedCommand(navcon.settings.vop, navcon.settings.vop, "Clear path - continue forward");
      cmd.specification = "Normal Navigation";
      DEBUG_SERIAL.println("✅ CLEAR PATH: Forward");
    }
    return cmd;
  }

  // Unknown/mixed situation
  cmd.action = ACT_STOP;
  cmd.setSpeedCommand(0, 0, "Unknown sensor combination - stop for safety");
  cmd.specification = "Safety Stop";
  DEBUG_SERIAL.printf("❓ UNKNOWN: S1=%s S2=%s S3=%s - STOP\n",
                      colorToString(s.s1), colorToString(s.s2), colorToString(s.s3));
  return cmd;
}

SCSPacket generateNAVCONPacket(const NAVCONCommand &cmd) {
  SCSPacket packet;
  packet.control = makeControl(SYS_MAZE, SUB_SNC, 3);
  
  switch(cmd.action) {
    case ACT_FORWARD:
    case ACT_BACKWARD:
    case ACT_DIFFERENTIAL_STEER:
      packet.dat1 = clamp8(cmd.rightSpeed);
      packet.dat0 = clamp8(cmd.leftSpeed);
      packet.dec = (cmd.action == ACT_BACKWARD) ? 1 : 0;
      break;
    case ACT_TURN_LEFT:
      packet.dat1 = (cmd.rotationDeg >> 8) & 0xFF;
      packet.dat0 = (cmd.rotationDeg & 0xFF);
      packet.dec = 2;
      break;
    case ACT_TURN_RIGHT:
    case ACT_WALL_FOLLOW_TURN:
      packet.dat1 = (cmd.rotationDeg >> 8) & 0xFF;
      packet.dat0 = (cmd.rotationDeg & 0xFF);
      packet.dec = 3;
      break;
    case ACT_STEERING_CORRECTION:
      packet.dat1 = (cmd.rotationDeg >> 8) & 0xFF;
      packet.dat0 = (cmd.rotationDeg & 0xFF);
      packet.dec = cmd.rotationDir;
      break;
    case ACT_TURN_180_LEFT:
      packet.dat1 = 0;
      packet.dat0 = 180;
      packet.dec = 2;
      break;
    case ACT_TURN_180_RIGHT:
      packet.dat1 = 0;
      packet.dat0 = 180;
      packet.dec = 3;
      break;
    case ACT_TURN_360_LEFT:
      packet.dat1 = (360 >> 8) & 0xFF;
      packet.dat0 = (360 & 0xFF);
      packet.dec = 2;
      break;
    case ACT_STOP:
    case ACT_ERROR_STOP:
    default:
      packet.dat1 = 0;
      packet.dat0 = 0;
      packet.dec = 0;
      break;
  }
  return packet;
}

void writePacket(const SCSPacket &p) {
  // Send raw packet data to Python via GPIO 16/17
  DATA_SERIAL.write(p.control);
  DATA_SERIAL.write(p.dat1);
  DATA_SERIAL.write(p.dat0);
  DATA_SERIAL.write(p.dec);
  
  // Send debug info to USB Serial Monitor
  DEBUG_SERIAL.printf("📤 TX SNC: ctrl=0x%02X dat1=%u dat0=%u dec=%u\n", p.control, p.dat1, p.dat0, p.dec);
}

bool readPacket(SCSPacket &p) {
  // Read from Python via GPIO 16/17
  while (DATA_SERIAL.available()) {
    int firstByte = DATA_SERIAL.peek();

    if (firstByte == 0x00) {
      DATA_SERIAL.read();
      continue;
    }

    if (firstByte == 0xAA) {
      if (DATA_SERIAL.available() < 6) return false;
      DATA_SERIAL.read();
      p.control = DATA_SERIAL.read();
      p.dat1 = DATA_SERIAL.read();
      p.dat0 = DATA_SERIAL.read();
      p.dec = DATA_SERIAL.read();
      int tail = DATA_SERIAL.read();
      if (tail != 0x55) {
        DEBUG_SERIAL.printf("Invalid frame tail: 0x%02X (expected 0x55)\n", tail);
        return false;
      }
      if (p.isEmpty()) return false;
      return true;
    }

    if (DATA_SERIAL.available() < 4) return false;
    p.control = DATA_SERIAL.read();
    p.dat1 = DATA_SERIAL.read();
    p.dat0 = DATA_SERIAL.read();
    p.dec = DATA_SERIAL.read();
    if (p.isEmpty()) return false;
    return true;
  }
  return false;
}

void ingestPacket(const SCSPacket &p) {
  SystemState sys = getSystemState(p.control);
  SubsystemID sub = getSubsystemID(p.control);
  uint8_t ist = getInternalState(p.control);

  // MDPS calibration regardless of system state
  if (sub == SUB_MDPS && ist == 0) {
    int16_t vL = p.dat0;
    int16_t vR = p.dat1;
    navcon.settings.updateCalibration(vL, vR);
    return;
  }

  // Only process MAZE state packets for main flow
  if (sys != SYS_MAZE) {
    DEBUG_SERIAL.printf("Ignoring non-MAZE packet: SYS=%d\n", sys);
    return;
  }

  // Track MAZE state entry
  if (!navcon.inMazeState) {
    navcon.inMazeState = true;
    DEBUG_SERIAL.println("🚀 ENTERED MAZE STATE");
  }

  // Send startup handshake if needed
  if (!navcon.startupSent) {
    SCSPacket pureTonePacket;
    pureTonePacket.control = makeControl(SYS_MAZE, SUB_SNC, 1);
    pureTonePacket.dat1 = 0;
    pureTonePacket.dat0 = 0;
    pureTonePacket.dec = 0;
    writePacket(pureTonePacket);

    SCSPacket touchPacket;
    touchPacket.control = makeControl(SYS_MAZE, SUB_SNC, 2);
    touchPacket.dat1 = 0;
    touchPacket.dat0 = 0;
    touchPacket.dec = 0;
    writePacket(touchPacket);

    navcon.startupSent = true;
    DEBUG_SERIAL.println("📡 Startup handshake sent: Pure Tone (0), Touch (0)");
  }

  // Process subsystem packets
  switch(sub) {
    case SUB_SS:
      if (ist == 1) {
        // Color detection
        uint16_t colorData = (uint16_t(p.dat1) << 8) | p.dat0;
        navcon.sensors.s1 = decodeColor((colorData >> 6) & 0x07);
        navcon.sensors.s2 = decodeColor((colorData >> 3) & 0x07);
        navcon.sensors.s3 = decodeColor(colorData & 0x07);
        navcon.sensors.haveColours = true;
        navcon.sensors.tColours = millis();
        DEBUG_SERIAL.printf("SS COLORS: S1=%s S2=%s S3=%s (raw=0x%04X)\n",
                            colorToString(navcon.sensors.s1),
                            colorToString(navcon.sensors.s2),
                            colorToString(navcon.sensors.s3),
                            colorData);
      } else if (ist == 2) {
        // Angle (signed in dat1)
        int16_t rawAngle = (int8_t)p.dat1;
        int16_t clampedAngle = constrain(rawAngle, -45, 45);
        if (rawAngle != clampedAngle) {
          DEBUG_SERIAL.printf("WARNING: Angle clamped from %d° to %d° (valid ±45°)\n", rawAngle, clampedAngle);
        }
        navcon.sensors.theta = clampedAngle;
        navcon.sensors.haveAngle = true;
        navcon.sensors.tAngle = millis();
        DEBUG_SERIAL.printf("SS ANGLE: %d° (valid ±45°)\n", navcon.sensors.theta);
      } else if (ist == 3) {
        DEBUG_SERIAL.println("SS END-OF-MAZE signal received");
      }
      break;

    case SUB_MDPS:
      if (ist == 1) {
        DEBUG_SERIAL.println("MDPS BATTERY packet received");
      } else if (ist == 2) {
        navcon.motors.lastRot = (uint16_t(p.dat1) << 8) | p.dat0;
        navcon.motors.rotDir = p.dec;
        if (navcon.motors.rotDir != 2 && navcon.motors.rotDir != 3) {
          DEBUG_SERIAL.printf("WARNING: Invalid MDPS rotDir %d, defaulting to LEFT(2)\n", navcon.motors.rotDir);
          navcon.motors.rotDir = 2;
        }
        navcon.motors.haveRot = true;
        navcon.motors.tRot = millis();
        String direction = (navcon.motors.rotDir == 2) ? "LEFT" : "RIGHT";
        DEBUG_SERIAL.printf("MDPS ROTATION: %d° %s\n", navcon.motors.lastRot, direction.c_str());
      } else if (ist == 3) {
        navcon.motors.vR = p.dat1;
        navcon.motors.vL = p.dat0;
        navcon.motors.haveSpeed = true;
        navcon.motors.tSpeed = millis();
        DEBUG_SERIAL.printf("MDPS SPEED: L=%d R=%d mm/s\n", navcon.motors.vL, navcon.motors.vR);
      } else if (ist == 4) {
        navcon.motors.dist = (uint16_t(p.dat1) << 8) | p.dat0;
        navcon.motors.haveDist = true;
        navcon.motors.tDist = millis();
        DEBUG_SERIAL.printf("MDPS DISTANCE: %d mm\n", navcon.motors.dist);
      }
      break;

    case SUB_SNC:
      DEBUG_SERIAL.printf("SNC NAVCON packet acknowledged: IST=%d\n", ist);
      break;

    default:
      DEBUG_SERIAL.printf("Unknown subsystem packet: SUB=%d IST=%d\n", sub, ist);
      break;
  }

  navcon.stateMachine.advanceAfter(p);
}

bool shouldExecuteNAVCON(const SCSPacket &lastPacket) {
  SystemState sys = getSystemState(lastPacket.control);
  SubsystemID sub = getSubsystemID(lastPacket.control);
  uint8_t ist = getInternalState(lastPacket.control);
  if (sys == SYS_MAZE && sub == SUB_SS && ist == 2) return true;
  if (navcon.wallFollow.isActive()) return true;
  return false;
}

// ==================== TEST FUNCTIONS ====================
void simulatePacket(SystemState sys, SubsystemID sub, uint8_t ist,
                   uint8_t dat1, uint8_t dat0, uint8_t dec) {
  SCSPacket p;
  p.control = makeControl(sys, sub, ist);
  p.dat1 = dat1;
  p.dat0 = dat0;
  p.dec = dec;
  
  DEBUG_SERIAL.printf("🧪 SIMULATE: [%s:%s:IST%d] dat1=%d dat0=%d dec=%d\n",
                      (sys == SYS_MAZE) ? "MAZE" : "OTHER",
                      (sub == SUB_SS) ? "SS" : (sub == SUB_MDPS) ? "MDPS" : "SNC",
                      ist, dat1, dat0, dec);
  
  if (!validatePacketSemantics(p)) {
    DEBUG_SERIAL.println("❌ Invalid packet semantics");
    return;
  }
  
  if (!navcon.stateMachine.isExpected(p)) {
    DEBUG_SERIAL.printf("⚠️ Unexpected packet: got %d:%d, expected %d:%d\n",
                        sub, ist, navcon.stateMachine.expectSub, navcon.stateMachine.expectIST);
  }
  
  ingestPacket(p);
  
  if (shouldExecuteNAVCON(p)) {
    NAVCONCommand decision = decideNavigationAction();
    if (decision.ready) {
      SCSPacket navPacket = generateNAVCONPacket(decision);
      writePacket(navPacket);
      navcon.decisionCycle++;
      DEBUG_SERIAL.printf("🧭 NAVCON #%lu: %s\n", navcon.decisionCycle, decision.why.c_str());
      DEBUG_SERIAL.printf("🧭 Specification: %s\n", decision.specification.c_str());
      DEBUG_SERIAL.printf("🧭 Action: %s\n", actionToString(decision.action));
      if (decision.action == ACT_FORWARD || decision.action == ACT_BACKWARD || decision.action == ACT_DIFFERENTIAL_STEER) {
        DEBUG_SERIAL.printf("🧭 Speeds: L=%d R=%d mm/s\n", decision.leftSpeed, decision.rightSpeed);
      } else if (decision.rotationDeg > 0) {
        String dir = (decision.rotationDir == 2) ? "LEFT" : "RIGHT";
        DEBUG_SERIAL.printf("🧭 Rotation: %d° %s\n", decision.rotationDeg, dir.c_str());
      }
    } else {
      DEBUG_SERIAL.println("❌ NAVCON decision not ready");
    }
  }
}

void runSteeringCorrectionTest() {
  DEBUG_SERIAL.println("\n🧪 ======= STEERING CORRECTION DIRECTION TEST =======");
  simulatePacket(SYS_CAL, SUB_MDPS, 0, 50, 50, 0);

  DEBUG_SERIAL.println("\n--- Positive Angle Tests ---");
  DEBUG_SERIAL.println("Red line +60° (RIGHT toward perpendicular):");
  simulatePacket(SYS_MAZE, SUB_SS, 1, 0x00, 0x08, 0);
  simulatePacket(SYS_MAZE, SUB_SS, 2, 60, 0, 0);

  DEBUG_SERIAL.println("Green line +50° (LEFT toward line):");
  simulatePacket(SYS_MAZE, SUB_SS, 1, 0x00, 0x10, 0);
  simulatePacket(SYS_MAZE, SUB_SS, 2, 50, 0, 0);

  DEBUG_SERIAL.println("Black wall +55° (LEFT away from wall):");
  simulatePacket(SYS_MAZE, SUB_SS, 1, 0x00, 0x20, 0);
  simulatePacket(SYS_MAZE, SUB_SS, 2, 55, 0, 0);

  DEBUG_SERIAL.println("\n--- Negative Angle Tests ---");
  DEBUG_SERIAL.println("Red line -60° (LEFT toward perpendicular):");
  simulatePacket(SYS_MAZE, SUB_SS, 1, 0x00, 0x08, 0);
  simulatePacket(SYS_MAZE, SUB_SS, 2, -60, 0, 0);

  DEBUG_SERIAL.println("Green line -50° (RIGHT toward line):");
  simulatePacket(SYS_MAZE, SUB_SS, 1, 0x00, 0x10, 0);
  simulatePacket(SYS_MAZE, SUB_SS, 2, -50, 0, 0);

  DEBUG_SERIAL.println("Black wall -55° (RIGHT away from wall):");
  simulatePacket(SYS_MAZE, SUB_SS, 1, 0x00, 0x20, 0);
  simulatePacket(SYS_MAZE, SUB_SS, 2, -55, 0, 0);

  DEBUG_SERIAL.println("\n🧪 ======= STEERING TEST COMPLETE =======");
  DEBUG_SERIAL.println("Expected DEC values: 2=LEFT, 3=RIGHT");
}

void runComprehensiveDemo() {
  DEBUG_SERIAL.println("\n🧪 ======= COMPREHENSIVE NAVCON DEMO =======");
  simulatePacket(SYS_CAL, SUB_MDPS, 0, 50, 50, 0);

  DEBUG_SERIAL.println("\n--- Test 1: Clear Path ---");
  simulatePacket(SYS_MAZE, SUB_SS, 1, 0x00, 0x00, 0);
  simulatePacket(SYS_MAZE, SUB_SS, 2, 0, 0, 0);

  DEBUG_SERIAL.println("\n--- Test 2: Black Wall - Wall Following ---");
  simulatePacket(SYS_MAZE, SUB_SS, 1, 0x00, 0x20, 0);
  simulatePacket(SYS_MAZE, SUB_SS, 2, 25, 0, 0);

  DEBUG_SERIAL.println("\n--- Test 3: Green Line - Crossing OK ---");
  simulatePacket(SYS_MAZE, SUB_SS, 1, 0x00, 0x10, 0);
  simulatePacket(SYS_MAZE, SUB_SS, 2, 3, 0, 0);

  DEBUG_SERIAL.println("\n--- Test 4: Red Line - High Angle ---");
  simulatePacket(SYS_MAZE, SUB_SS, 1, 0x00, 0x08, 0);
  simulatePacket(SYS_MAZE, SUB_SS, 2, 50, 0, 0);

  DEBUG_SERIAL.println("\n--- Test 5: Both Sides Blocked ---");
  simulatePacket(SYS_MAZE, SUB_SS, 1, 0x24, 0x20, 0);
  simulatePacket(SYS_MAZE, SUB_SS, 2, 15, 0, 0);

  DEBUG_SERIAL.println("\n--- Test 6: End of Maze ---");
  simulatePacket(SYS_MAZE, SUB_SS, 1, 0x00, 0x49, 0);
  simulatePacket(SYS_MAZE, SUB_SS, 2, 0, 0, 0);

  DEBUG_SERIAL.println("\n🧪 ======= DEMO COMPLETE =======\n");
}

void printDetailedStatus() {
  DEBUG_SERIAL.println("\n📊 ======= DETAILED NAVCON STATUS =======");
  DEBUG_SERIAL.printf("🔄 System State: inMaze=%s startupSent=%s\n",
                      navcon.inMazeState ? "YES" : "NO",
                      navcon.startupSent ? "YES" : "NO");
  DEBUG_SERIAL.printf("🔄 Decision Cycle: %lu\n", navcon.decisionCycle);
  DEBUG_SERIAL.printf("🔄 Error Count: %lu\n", navcon.errorCount);

  DEBUG_SERIAL.printf("🔧 Calibration: valid=%s vop=%d vcor=%d\n",
                      navcon.settings.haveCal ? "YES" : "NO",
                      navcon.settings.vop, navcon.settings.vcor);

  DEBUG_SERIAL.printf("📡 Sensor Colors: valid=%s age=%lums\n",
                      navcon.sensors.haveColours ? "YES" : "NO",
                      navcon.sensors.haveColours ? (millis() - navcon.sensors.tColours) : 0);
  if (navcon.sensors.haveColours) {
    DEBUG_SERIAL.printf("📡   S1=%s S2=%s S3=%s\n",
                        colorToString(navcon.sensors.s1),
                        colorToString(navcon.sensors.s2),
                        colorToString(navcon.sensors.s3));
  }

  DEBUG_SERIAL.printf("📡 Sensor Angle: valid=%s θ=%d° age=%lums\n",
                      navcon.sensors.haveAngle ? "YES" : "NO",
                      navcon.sensors.theta,
                      navcon.sensors.haveAngle ? (millis() - navcon.sensors.tAngle) : 0);

  if (navcon.wallFollow.isActive()) {
    DEBUG_SERIAL.printf("🔄 Wall Follow: state=%d angle=%d° turn=%d° side=%s\n",
                        navcon.wallFollow.state,
                        navcon.wallFollow.detectedAngle,
                        navcon.wallFollow.calculatedTurn,
                        navcon.wallFollow.obstacleOnLeft ? "LEFT" : "RIGHT");
  } else {
    DEBUG_SERIAL.println("🔄 Wall Follow: IDLE");
  }

  DEBUG_SERIAL.printf("✅ Input Validation: hasValid=%s dataFresh=%s angleValid=%s calValid=%s\n",
                      navcon.hasValidInputs() ? "YES" : "NO",
                      navcon.sensors.isDataFresh() ? "YES" : "NO",
                      navcon.sensors.isAngleValid() ? "YES" : "NO",
                      navcon.settings.haveCal ? "YES" : "NO");

  DEBUG_SERIAL.println("📊 =======================================\n");
}

// ==================== ARDUINO SETUP/LOOP ====================
void setup() {
  // Debug over USB Serial
  DEBUG_SERIAL.begin(DEBUG_BAUD);
  while (!DEBUG_SERIAL) { delay(1); }

  // Data communication via GPIO 16/17 for Python
  DATA_SERIAL.begin(DATA_BAUD, SERIAL_8N1, DATA_RX_PIN, DATA_TX_PIN);

  navcon.reset();

  DEBUG_SERIAL.println("🧭 ========================================");
  DEBUG_SERIAL.println("🧭   ENHANCED NAVCON MAZE STATE MACHINE");
  DEBUG_SERIAL.println("🧭   Dual Serial Port Configuration");
  DEBUG_SERIAL.println("🧭 ========================================");
  DEBUG_SERIAL.printf("📡 DEBUG Serial @ %lu baud (USB Serial Monitor)\n", (unsigned long)DEBUG_BAUD);
  DEBUG_SERIAL.printf("📡 DATA Serial @ %lu baud (GPIO %d/%d for Python)\n", (unsigned long)DATA_BAUD, DATA_RX_PIN, DATA_TX_PIN);
  DEBUG_SERIAL.println("📡 Python connects to DATA port via USB-Serial adapter");
  DEBUG_SERIAL.println("📡 Commands: C=steering, T=demo, S=status, R=reset, ?=help");
  DEBUG_SERIAL.println("🧭 ========================================\n");

  // Run initial demo on startup
  DEBUG_SERIAL.println("🚀 Running startup demo...");
  runComprehensiveDemo();
}

void loop() {
  // Process incoming packet from Python on DATA_SERIAL (GPIO 16/17)
  SCSPacket packet;
  if (readPacket(packet)) {
    DEBUG_SERIAL.printf("📥 RX from Python: ctrl=0x%02X dat1=%u dat0=%u dec=%u\n", 
                        packet.control, packet.dat1, packet.dat0, packet.dec);
    
    if (!validatePacketSemantics(packet)) {
      DEBUG_SERIAL.println("❌ Invalid packet semantics - ignored");
      navcon.incrementError();
    } else {
      if (!navcon.stateMachine.isExpected(packet)) {
        DEBUG_SERIAL.printf("⚠️ Unexpected packet: got %d:%d, expected %d:%d\n",
                            getSubsystemID(packet.control), getInternalState(packet.control),
                            navcon.stateMachine.expectSub, navcon.stateMachine.expectIST);
      }
      ingestPacket(packet);

      if (shouldExecuteNAVCON(packet)) {
        NAVCONCommand decision = decideNavigationAction();
        if (decision.ready) {
          SCSPacket navPacket = generateNAVCONPacket(decision);
          writePacket(navPacket);
          navcon.decisionCycle++;
          DEBUG_SERIAL.printf("🧭 NAVCON #%lu: %s (%s)\n",
                              navcon.decisionCycle,
                              actionToString(decision.action),
                              decision.specification.c_str());
        }
      }
    }
  }

  // ASCII test commands from DEBUG_SERIAL (USB Serial Monitor)
  if (DEBUG_SERIAL.available()) {
    char cmd = DEBUG_SERIAL.read();
    switch (cmd) {
      case 'C': case 'c': runSteeringCorrectionTest(); break;
      case 'T': case 't': runComprehensiveDemo(); break;
      case 'S': case 's': printDetailedStatus(); break;
      case 'R': case 'r':
        navcon.reset();
        DEBUG_SERIAL.println("🔄 NAVCON state reset complete");
        break;
      case '?':
        DEBUG_SERIAL.println("Commands: C=steering, T=demo, S=status, R=reset");
        break;
      default: break;
    }
  }

  // Periodic timeouts
  static uint32_t lastTimeoutCheck = 0;
  if (millis() - lastTimeoutCheck > 1000) {
    if (navcon.stateMachine.isStale()) {
      DEBUG_SERIAL.println("⚠️ WARNING: No packets received recently");
    }
    if (navcon.wallFollow.isTimedOut()) {
      DEBUG_SERIAL.println("⚠️ WARNING: Wall follow sequence timed out");
      navcon.wallFollow.reset();
    }
    lastTimeoutCheck = millis();
  }
}