// ============================================================================
//  LOADER / UNLOADER CAPACITY CONTROL  (AquaForce Chiller scheme)
// ----------------------------------------------------------------------------
//  Applies only to fixed-speed compressors (driveSelection 0 = Star-Delta,
//  1 = DOL). VFD-driven compressors (driveSelection == 3) modulate capacity
//  via frequency instead and are left untouched here.
//
//  Valve characteristics (per spec):
//    - Loader solenoid   : Normally CLOSED (N.C.) - de-energized = no load step
//    - Unloader solenoid  : Normally OPEN  (N.O.) - de-energized = bypass open (unloaded)
//
//  LOADING      : Unloader held ON (closes bypass). Loader pulses
//                 ON loadPulseOnMs / OFF loadPulseOffMs, repeating, until
//                 load% reaches the target ceiling (RLA_Limit_Pct) or demand
//                 is satisfied.
//  MAINTAINING  : Loader held OFF, Unloader held ON - holds current capacity.
//  UNLOADING    : Loader held OFF. Unloader pulses OFF loadPulseOnMs /
//                 ON loadPulseOffMs (inverted duty - briefly opens the
//                 bypass each cycle), repeating, until load% falls to the
//                 floor (minLoadPct).
//
//  Capacity (load%) is estimated from motor current vs the operator-set
//  RLA_Amps (current at 100% rated load). RLA_Limit_Pct is shared with
//  MasterSlave.h - it's the same field that gets stepped to 50 during a
//  SLAVE assist hand-off, so this controller automatically respects that.
//
//  HARDWARE NOTE: setLoaderOutput()/setUnloaderOutput() below are placeholders
//  (Serial only) until the DC-25B Y-output GPIO map is wired into IO.h. Swap
//  the Serial.printf for the real Output_Write(...) call once that's known -
//  everything else in this file is hardware-agnostic.
// ============================================================================

void setLoaderOutput(Compressor &comp, bool state) {
  char id = (&comp == &CompA) ? 'A' : 'B';
  Serial.printf("[Loader %c] -> %s\n", id, state ? "ON" : "OFF");
  // TODO: Output_Write(LOADER_PIN[id], state);  // bind once DC-25B Y-output map is known
}

void setUnloaderOutput(Compressor &comp, bool state) {
  char id = (&comp == &CompA) ? 'A' : 'B';
  Serial.printf("[Unloader %c] -> %s\n", id, state ? "ON" : "OFF");
  // TODO: Output_Write(UNLOADER_PIN[id], state);  // bind once DC-25B Y-output map is known
}

// Drives one solenoid through a 2-phase ON/OFF cycle.
//   activeLevelDuringPhase1 = the output level held during the FIRST phase (phase1Ms long);
//   the SECOND phase (phase2Ms long) holds the opposite level.
void runPulseCycle(Compressor &comp, bool isLoaderChannel, bool activeLevelDuringPhase1,
                    unsigned long phase1Ms, unsigned long phase2Ms) {
  unsigned long now = millis();
  unsigned long phaseLen = comp.loadPulsePhaseOn ? phase1Ms : phase2Ms;

  if (now - comp.loadPulseTimer >= phaseLen) {
    comp.loadPulsePhaseOn = !comp.loadPulsePhaseOn;
    comp.loadPulseTimer = now;
  }

  bool desired = comp.loadPulsePhaseOn ? activeLevelDuringPhase1 : !activeLevelDuringPhase1;

  if (isLoaderChannel) {
    if (comp.loaderOutput != desired) {
      setLoaderOutput(comp, desired);
      comp.loaderOutput = desired;
    }
  } else {
    if (comp.unloaderOutput != desired) {
      setUnloaderOutput(comp, desired);
      comp.unloaderOutput = desired;
    }
  }
}

// Forces both solenoids to their de-energized rest state (matches valve rest
// position: Loader N.C. closed / Unloader N.O. open = fully unloaded).
void parkSolenoids(Compressor &comp) {
  if (comp.loaderOutput) {
    setLoaderOutput(comp, false);
    comp.loaderOutput = false;
  }
  if (comp.unloaderOutput) {
    setUnloaderOutput(comp, false);
    comp.unloaderOutput = false;
  }
}

void updateCapacityControl(Compressor &comp) {
  // Only fixed-speed compressors use Loader/Unloader; VFD modulates via frequency.
  if (comp.driveSelection == 3) return;

  if (comp.state != COMP_RUNNING) {
    if (comp.loadState != LOAD_IDLE) {
      parkSolenoids(comp);
      comp.loadState = LOAD_IDLE;
      comp.fullyLoaded = false;
    }
    return;
  }

  if (comp.RLA_Amps <= 0) return;  // not commissioned yet (HMI setpoint missing) - do nothing safely
  if (AMP1 == SENSOR_DISCONNECTED) return;  // CT module fault - hold last solenoid state rather than act on bad data

  int targetPct = comp.RLA_Limit_Pct;             // ceiling - 100% normally, 50% during assist hand-off
  int loadPct = (AMP1 * 100) / comp.RLA_Amps;      // estimated current load %

  comp.fullyLoaded = (loadPct >= targetPct);

  bool needsMoreCooling = (ReturnTemp > ReturnSp + LOAD_DEADBAND) && (loadPct < targetPct);
  bool needsLessCooling = (ReturnTemp < ReturnSp - LOAD_DEADBAND) && (loadPct > comp.minLoadPct);

  // ---- State transitions ----
  switch (comp.loadState) {
    case LOAD_IDLE:
      comp.loadState = needsMoreCooling ? LOAD_LOADING : LOAD_MAINTAINING;
      comp.loadPulsePhaseOn = true;
      comp.loadPulseTimer = millis();
      break;

    case LOAD_LOADING:
      if (!needsMoreCooling) {
        Serial.println("[Capacity] Target reached / fully loaded -> MAINTAINING");
        comp.loadState = LOAD_MAINTAINING;
      }
      break;

    case LOAD_UNLOADING:
      if (!needsLessCooling) {
        Serial.println("[Capacity] Floor reached / no longer overshooting -> MAINTAINING");
        comp.loadState = LOAD_MAINTAINING;
      }
      break;

    case LOAD_MAINTAINING:
    default:
      if (needsMoreCooling) {
        Serial.println("[Capacity] Demand rising -> LOADING");
        comp.loadState = LOAD_LOADING;
        comp.loadPulsePhaseOn = true;  // restart the pulse cycle cleanly
        comp.loadPulseTimer = millis();
      } else if (needsLessCooling) {
        Serial.println("[Capacity] Demand falling -> UNLOADING");
        comp.loadState = LOAD_UNLOADING;
        comp.loadPulsePhaseOn = true;
        comp.loadPulseTimer = millis();
      }
      break;
  }

  // ---- Drive solenoids for the current state ----
  switch (comp.loadState) {
    case LOAD_LOADING:
      // Unloader held ON; Loader pulses ON loadPulseOnMs / OFF loadPulseOffMs
      if (!comp.unloaderOutput) {
        setUnloaderOutput(comp, true);
        comp.unloaderOutput = true;
      }
      runPulseCycle(comp, true /*loader channel*/, true /*phase1 = energized*/,
                    comp.loadPulseOnMs, comp.loadPulseOffMs);
      break;

    case LOAD_UNLOADING:
      // Loader held OFF; Unloader pulses OFF loadPulseOnMs / ON loadPulseOffMs (inverted duty)
      if (comp.loaderOutput) {
        setLoaderOutput(comp, false);
        comp.loaderOutput = false;
      }
      runPulseCycle(comp, false /*unloader channel*/, false /*phase1 = de-energized*/,
                    comp.loadPulseOnMs, comp.loadPulseOffMs);
      break;

    case LOAD_MAINTAINING:
      if (comp.loaderOutput) {
        setLoaderOutput(comp, false);
        comp.loaderOutput = false;
      }
      if (!comp.unloaderOutput) {
        setUnloaderOutput(comp, true);
        comp.unloaderOutput = true;
      }
      break;

    default:
      break;
  }
}
