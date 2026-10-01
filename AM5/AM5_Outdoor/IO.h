#ifndef IO_H
#define IO_H

// =====================================================
// PCA9554 registers (relay output expander @ 0x20)
// =====================================================
#define PCA9554_INPUT_REG     0x00
#define PCA9554_OUTPUT_REG    0x01
#define PCA9554_POLARITY_REG  0x02
#define PCA9554_CONFIG_REG    0x03

inline void printByteBinary8(uint8_t value)
{
  for (int bit = 7; bit >= 0; --bit)
    Serial.print((value >> bit) & 0x01);
}

inline void printSwitchStatesLine()
{
  Serial.print("[SWITCH] ");
  for (uint8_t i = 0; i < SWITCH_COUNT; i++)
  {
    Serial.print("SW");
    Serial.print(i + 1);
    Serial.print("=");
    Serial.print(switchState[i] ? 1 : 0);
    if (i < SWITCH_COUNT - 1) Serial.print(" | ");
  }
  Serial.println();
}

inline void printRelayStatesLine(const char *prefix = "[RELAY]")
{
  Serial.print(prefix);
  Serial.print(" R1="); Serial.print(R1_State ? 1 : 0);
  Serial.print(" R2="); Serial.print(R2_State ? 1 : 0);
  Serial.print(" R3="); Serial.print(R3_State ? 1 : 0);
  Serial.print(" R4="); Serial.println(R4_State ? 1 : 0);
}

inline void scanI2CBusDebug()
{
  Serial.println("[I2C-SCAN] Scanning bus...");
  uint8_t found = 0;
  for (uint8_t address = 1; address < 127; address++)
  {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0)
    {
      Serial.print("[I2C-SCAN] Found 0x");
      if (address < 0x10) Serial.print("0");
      Serial.print(address, HEX);
      if (address == SWITCH_PCF_ADDRESS) Serial.print(" <- SWITCH PCF8574");
      if (address == RELAY_PCA_ADDRESS) Serial.print(" <- RELAY PCA9554");
      Serial.println();
      found++;
    }
  }
  Serial.print("[I2C-SCAN] Total devices = ");
  Serial.println(found);
}

inline bool i2cAddressResponds(uint8_t address)
{
  Wire.beginTransmission(address);
  return (Wire.endTransmission() == 0);
}

// =====================================================
// PCF8574 helpers - switch inputs only
// =====================================================
inline bool writePCF8574(uint8_t address, uint8_t value)
{
  Wire.beginTransmission(address);
  Wire.write(value);
  return (Wire.endTransmission() == 0);
}

inline bool readPCF8574(uint8_t address, uint8_t &data)
{
  const uint8_t received = Wire.requestFrom(address, (uint8_t)1);
  if (received != 1 || !Wire.available()) return false;
  data = Wire.read();
  return true;
}

// =====================================================
// PCA9554 helpers - relay outputs only
// PCA9554 is register-based; unlike PCF8574 we MUST write register address.
// =====================================================
inline bool writePCA9554Register(uint8_t reg, uint8_t value)
{
  Wire.beginTransmission(RELAY_PCA_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return (Wire.endTransmission() == 0);
}

inline bool readPCA9554Register(uint8_t reg, uint8_t &value)
{
  Wire.beginTransmission(RELAY_PCA_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;

  const uint8_t received = Wire.requestFrom(RELAY_PCA_ADDRESS, (uint8_t)1);
  if (received != 1 || !Wire.available()) return false;
  value = Wire.read();
  return true;
}

inline uint8_t buildRelayPcaConfigMask()
{
  // PCA9554 config bit: 1=input, 0=output.
  // Start with every pin as input, then make only the four relay pins outputs.
  uint8_t mask = 0xFF;
  bitClear(mask, R1_PCA_PIN);
  bitClear(mask, R2_PCA_PIN);
  bitClear(mask, R3_PCA_PIN);
  bitClear(mask, R4_PCA_PIN);
  return mask;
}

inline uint8_t relayOnLevel()
{
  return (RELAY_ACTIVE_LEVEL == HIGH) ? HIGH : LOW;
}

inline uint8_t relayOffLevel()
{
  return (RELAY_ACTIVE_LEVEL == HIGH) ? LOW : HIGH;
}

inline void setRelayBitInShadow(uint8_t pin, bool relayOn)
{
  const uint8_t outputLevel = relayOn ? relayOnLevel() : relayOffLevel();
  bitWrite(relayPcaShadow, pin, outputLevel == HIGH ? 1 : 0);
}

inline bool initRelayPCA9554()
{
  // Safe startup for either relay polarity:
  // 1) preload the four relay output latches to their OFF electrical level
  // 2) no polarity inversion
  // 3) only then configure those four PCA9554 pins as outputs
  // This prevents an active-HIGH relay pulse during PCA9554 initialization.
  relayPcaShadow = 0xFF; // unused PCA pins stay HIGH
  setRelayBitInShadow(R1_PCA_PIN, false);
  setRelayBitInShadow(R2_PCA_PIN, false);
  setRelayBitInShadow(R3_PCA_PIN, false);
  setRelayBitInShadow(R4_PCA_PIN, false);

  if (!writePCA9554Register(PCA9554_OUTPUT_REG, relayPcaShadow)) return false;
  if (!writePCA9554Register(PCA9554_POLARITY_REG, 0x00)) return false;

  const uint8_t configMask = buildRelayPcaConfigMask();
  if (!writePCA9554Register(PCA9554_CONFIG_REG, configMask)) return false;

  uint8_t outputReadBack = 0;
  uint8_t configReadBack = 0;
  const bool outputReadOk = readPCA9554Register(PCA9554_OUTPUT_REG, outputReadBack);
  const bool configReadOk = readPCA9554Register(PCA9554_CONFIG_REG, configReadBack);

  Serial.print("[PCA9554-RELAY] OUTPUT readback=");
  if (outputReadOk)
  {
    Serial.print("0x");
    if (outputReadBack < 0x10) Serial.print("0");
    Serial.println(outputReadBack, HEX);
  }
  else
  {
    Serial.println("FAIL");
  }

  Serial.print("[PCA9554-RELAY] CONFIG readback=");
  if (configReadOk)
  {
    Serial.print("0x");
    if (configReadBack < 0x10) Serial.print("0");
    Serial.println(configReadBack, HEX);
  }
  else
  {
    Serial.println("FAIL");
  }

  if (!outputReadOk || !configReadOk || outputReadBack != relayPcaShadow || configReadBack != configMask)
  {
    Serial.println("[PCA9554-RELAY] ERROR: register verification failed");
    return false;
  }

  return true;
}

inline void buildRelayShadow(bool r1, bool r2, bool r3, bool r4)
{
  relayPcaShadow = 0xFF; // all unused pins stay HIGH
  setRelayBitInShadow(R1_PCA_PIN, r1);
  setRelayBitInShadow(R2_PCA_PIN, r2);
  setRelayBitInShadow(R3_PCA_PIN, r3);
  setRelayBitInShadow(R4_PCA_PIN, r4);
}

inline bool writeRelayStates(bool r1, bool r2, bool r3, bool r4)
{
  buildRelayShadow(r1, r2, r3, r4);

  // If PCA9554 was never configured or a previous transaction failed,
  // reinitialize its registers before attempting relay control.
  if (!relayPcaConfigured)
  {
    relayPcaHealthy = i2cAddressResponds(RELAY_PCA_ADDRESS) && initRelayPCA9554();
    relayPcaConfigured = relayPcaHealthy;
    if (!relayPcaHealthy) return false;
  }

  const bool wasHealthy = relayPcaHealthy;
  relayPcaHealthy = writePCA9554Register(PCA9554_OUTPUT_REG, relayPcaShadow);

  if (relayPcaHealthy)
  {
    uint8_t readBack = 0;
    relayPcaHealthy = readPCA9554Register(PCA9554_OUTPUT_REG, readBack) &&
                      (readBack == relayPcaShadow);
    if (!relayPcaHealthy)
    {
      Serial.print("[PCA9554-RELAY] ERROR: OUTPUT readback mismatch, wanted=0x");
      if (relayPcaShadow < 0x10) Serial.print("0");
      Serial.print(relayPcaShadow, HEX);
      Serial.print(" read=0x");
      if (readBack < 0x10) Serial.print("0");
      Serial.println(readBack, HEX);
    }
  }

  if (!relayPcaHealthy)
  {
    relayPcaConfigured = false;
    if (wasHealthy)
    {
      Serial.print("[PCA9554-RELAY] ERROR: OUTPUT transaction failed at 0x");
      Serial.println(RELAY_PCA_ADDRESS, HEX);
    }
  }
  else if (!wasHealthy)
  {
    Serial.print("[PCA9554-RELAY] RECOVERED at 0x");
    Serial.println(RELAY_PCA_ADDRESS, HEX);
  }

  return relayPcaHealthy;
}

void forceAllRelaysOff()
{
  const bool hadAnyRelayOn = R1_State || R2_State || R3_State || R4_State;

  R1_State = false;
  R2_State = false;
  R3_State = false;
  R4_State = false;

  writeRelayStates(false, false, false, false);

  if (hadAnyRelayOn)
  {
    Serial.println("[RELAY] FORCE ALL OFF");
    printRelayStatesLine();
  }
}

void initIO()
{
  Serial.println("[I2C] --------------------------------------------------");
  Serial.println("[I2C] Initializing Switch PCF8574 + Relay PCA9554");
  Serial.print("[I2C] SDA GPIO="); Serial.print(I2C_SDA_PIN);
  Serial.print(" | SCL GPIO="); Serial.println(I2C_SCL_PIN);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  scanI2CBusDebug();

  Serial.print("[I2C] Switch PCF8574 address = 0x");
  Serial.println(SWITCH_PCF_ADDRESS, HEX);
  Serial.print("[I2C] Relay PCA9554 address  = 0x");
  Serial.println(RELAY_PCA_ADDRESS, HEX);

  if (SWITCH_PCF_ADDRESS == RELAY_PCA_ADDRESS)
  {
    Serial.println("[I2C] FATAL: switch and relay expanders have identical addresses!");
    switchPcfHealthy = false;
    relayPcaHealthy = false;
    return;
  }

  switchPcfHealthy = i2cAddressResponds(SWITCH_PCF_ADDRESS);
  relayPcaHealthy = i2cAddressResponds(RELAY_PCA_ADDRESS);

  Serial.print("[PCF8574-SWITCH] 0x");
  Serial.print(SWITCH_PCF_ADDRESS, HEX);
  Serial.println(switchPcfHealthy ? " FOUND" : " NOT FOUND");

  Serial.print("[PCA9554-RELAY]  0x");
  Serial.print(RELAY_PCA_ADDRESS, HEX);
  Serial.println(relayPcaHealthy ? " FOUND" : " NOT FOUND");

  Serial.print("[SWITCH] Active level = ");
  Serial.println(SWITCH_ACTIVE_LEVEL == HIGH ? "HIGH (1)" : "LOW (0)");
  Serial.print("[RELAY] Active level = ");
  Serial.println(RELAY_ACTIVE_LEVEL == HIGH ? "HIGH (1)" : "LOW (0)");
  Serial.print("[PCA9554-RELAY] Mapping: R1=P"); Serial.print(R1_PCA_PIN);
  Serial.print(" R2=P"); Serial.print(R2_PCA_PIN);
  Serial.print(" R3=P"); Serial.print(R3_PCA_PIN);
  Serial.print(" R4=P"); Serial.println(R4_PCA_PIN);

  // PCF8574 quasi-bidirectional inputs: HIGH releases pins for input use.
  if (switchPcfHealthy)
  {
    switchPcfHealthy = writePCF8574(SWITCH_PCF_ADDRESS, 0xFF);
    Serial.println(switchPcfHealthy
      ? "[PCF8574-SWITCH] P0..P7 released HIGH for input mode"
      : "[PCF8574-SWITCH] ERROR configuring input mode");
  }

  // PCA9554 must be explicitly configured through registers.
  if (relayPcaHealthy)
  {
    relayPcaHealthy = initRelayPCA9554();
    relayPcaConfigured = relayPcaHealthy;
    Serial.println(relayPcaHealthy
      ? "[PCA9554-RELAY] Initialized + verified OK"
      : "[PCA9554-RELAY] Initialization/verification FAILED");
  }

  // Ensure relays start OFF using the configured electrical polarity.
  forceAllRelaysOff();
  Serial.print("[PCA9554-RELAY] Initial OFF shadow = 0x");
  if (relayPcaShadow < 0x10) Serial.print("0");
  Serial.println(relayPcaShadow, HEX);
  printRelayStatesLine("[RELAY-INIT]");

  if (!switchPcfHealthy)
    Serial.println("[SAFETY] Switch PCF8574 unavailable -> relays locked OFF");

  if (!relayPcaHealthy)
    Serial.println("[SAFETY] Relay PCA9554 unavailable -> outputs cannot be driven");

  Serial.println("[I2C] --------------------------------------------------");
}

void readSwitches()
{
  static bool firstRead = true;
  static uint8_t previousData = 0x00;
  static bool previousHealthy = false;

  uint8_t newData = 0xFF;
  const bool readOk = readPCF8574(SWITCH_PCF_ADDRESS, newData);
  switchPcfHealthy = readOk;

  if (!readOk)
  {
    for (int i = 0; i < SWITCH_COUNT; i++) switchState[i] = false;

    if (firstRead || previousHealthy)
    {
      Serial.print("[PCF8574-SWITCH] ERROR: read failed at 0x");
      Serial.println(SWITCH_PCF_ADDRESS, HEX);
      Serial.println("[SAFETY] All switch states forced to 0");
    }

    previousHealthy = false;
    firstRead = false;
    return;
  }

  if (firstRead || !previousHealthy)
  {
    Serial.print("[PCF8574-SWITCH] Read OK / RECOVERED at 0x");
    Serial.println(SWITCH_PCF_ADDRESS, HEX);
  }

  switchPcfData = newData;

  switchState[0] = (bitRead(switchPcfData, SW1) == SWITCH_ACTIVE_LEVEL);
  switchState[1] = (bitRead(switchPcfData, SW2) == SWITCH_ACTIVE_LEVEL);
  switchState[2] = (bitRead(switchPcfData, SW3) == SWITCH_ACTIVE_LEVEL);
  switchState[3] = (bitRead(switchPcfData, SW4) == SWITCH_ACTIVE_LEVEL);
  switchState[4] = (bitRead(switchPcfData, SW5) == SWITCH_ACTIVE_LEVEL);
  switchState[5] = (bitRead(switchPcfData, SW6) == SWITCH_ACTIVE_LEVEL);
  switchState[6] = (bitRead(switchPcfData, SW7) == SWITCH_ACTIVE_LEVEL);
  switchState[7] = (bitRead(switchPcfData, SW8) == SWITCH_ACTIVE_LEVEL);

  if (firstRead || !previousHealthy || newData != previousData)
  {
    Serial.print("[PCF8574-SWITCH] RAW=0x");
    if (newData < 0x10) Serial.print("0");
    Serial.print(newData, HEX);
    Serial.print(" BIN=");
    printByteBinary8(newData);
    Serial.println();
    printSwitchStatesLine();
  }

  previousData = newData;
  previousHealthy = true;
  firstRead = false;
}

void controlRelays()
{
  static int previousInhibitReason = -1;
  static bool previousGroup1 = false;
  static bool previousGroup2 = false;
  static bool firstNormalEvaluation = true;

  // Master safety gates come first. The existing SW1..SW8 logic is used only
  // while the complete AM5 system AND the Outdoor section are enabled.
  // This guarantees a Display/App powersw=0 command turns Outdoor R1..R4 OFF.
  int inhibitReason = 0;

  // Defense in depth: controlRelays() itself verifies the Indoor authorization
  // lease. Even if a caller forgets to run the timeout service first, stale or
  // missing master communication can never energize Outdoor relays.
  if (!validIndoorCommandSeen) inhibitReason = 5;
  else if (millis() - lastValidIndoorCommandMs > INDOOR_COMMAND_TIMEOUT_MS) inhibitReason = 6;
  else if (systemPower != 1) inhibitReason = 3;
  else if (outdoorEnable != 1) inhibitReason = 4;
  else if (!switchPcfHealthy) inhibitReason = 1;
  else if (!relayPcaHealthy) inhibitReason = 2;

  if (inhibitReason != 0)
  {
    if (inhibitReason != previousInhibitReason)
    {
      if (inhibitReason == 1)
        Serial.println("[RELAY] INHIBIT: Switch PCF8574 fault -> all relays OFF");
      else if (inhibitReason == 2)
        Serial.println("[RELAY] INHIBIT: Relay PCA9554 fault -> retrying all OFF");
      else if (inhibitReason == 3)
        Serial.println("[RELAY] INHIBIT: systemPower=0 -> all Outdoor relays OFF");
      else if (inhibitReason == 4)
        Serial.println("[RELAY] INHIBIT: outdoorEnable=0 -> all Outdoor relays OFF");
      else if (inhibitReason == 5)
        Serial.println("[RELAY] INHIBIT: no fresh Indoor B0 authorization -> all relays OFF");
      else
        Serial.println("[RELAY] INHIBIT: Indoor B0 authorization stale -> all relays OFF");
    }

    forceAllRelaysOff();
    previousInhibitReason = inhibitReason;
    firstNormalEvaluation = true;
    return;
  }

  if (previousInhibitReason != 0)
    Serial.println("[RELAY] Outdoor control enabled -> applying SW1..SW8 relay logic");

  previousInhibitReason = 0;

  // Exact required logic:
  // SW1+SW2+SW3+SW4 active -> R1 + R3 ON
  // SW5+SW6+SW7+SW8 active -> R2 + R4 ON
  const bool group1 = switchState[0] && switchState[1] &&
                      switchState[2] && switchState[3];

  const bool group2 = switchState[4] && switchState[5] &&
                      switchState[6] && switchState[7];

  if (firstNormalEvaluation || group1 != previousGroup1)
  {
    Serial.print("[LOGIC] Group1 SW1-4 = ");
    Serial.print(group1 ? "TRUE" : "FALSE");
    Serial.println(group1 ? " -> R1,R3 ON" : " -> R1,R3 OFF");
  }

  if (firstNormalEvaluation || group2 != previousGroup2)
  {
    Serial.print("[LOGIC] Group2 SW5-8 = ");
    Serial.print(group2 ? "TRUE" : "FALSE");
    Serial.println(group2 ? " -> R2,R4 ON" : " -> R2,R4 OFF");
  }

  const bool oldR1 = R1_State;
  const bool oldR2 = R2_State;
  const bool oldR3 = R3_State;
  const bool oldR4 = R4_State;

  const bool targetChanged = firstNormalEvaluation ||
                             oldR1 != group1 || oldR2 != group2 ||
                             oldR3 != group1 || oldR4 != group2;

  if (targetChanged || !relayPcaHealthy || !relayPcaConfigured)
  {
    if (!writeRelayStates(group1, group2, group1, group2))
    {
      R1_State = false;
      R2_State = false;
      R3_State = false;
      R4_State = false;
      Serial.println("[SAFETY] Relay PCA9554 write failed -> logical relay states forced OFF");
      previousGroup1 = group1;
      previousGroup2 = group2;
      firstNormalEvaluation = false;
      return;
    }
  }

  R1_State = group1;
  R3_State = group1;
  R2_State = group2;
  R4_State = group2;

  if (targetChanged)
  {
    Serial.print("[PCA9554-RELAY] OUTPUT shadow=0x");
    if (relayPcaShadow < 0x10) Serial.print("0");
    Serial.println(relayPcaShadow, HEX);
    printRelayStatesLine();
  }

  previousGroup1 = group1;
  previousGroup2 = group2;
  firstNormalEvaluation = false;
}

#endif
