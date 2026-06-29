// ============================================================================
//  MASTER / SLAVE  (LEAD-LAG)  CONTROLLER
// ----------------------------------------------------------------------------
//  Implements the dual-compressor priority logic from the AquaForce spec:
//
//   AUTO  (LeadSW == 0): the compressor with LESS running hours leads
//         (MASTER) and starts first. The other is SLAVE.
//   MANUAL (LeadSW == 1 / 2): operator forces Comp A / Comp B to be MASTER.
//
//   - If MASTER fails/trips while running, the SLAVE is promoted to MASTER.
//   - If MASTER is fully loaded and the target still isn't met, MASTER is
//     stepped down to 50% FIRST (to limit power-source loading), then the
//     SLAVE is started to assist.
//   - SLAVE is released once the target is met or assistance is no longer
//     required, and MASTER is restored to full range.
//   - Any compressor that stops is locked out from restarting for 600s
//     (10 minutes) - enforced via Compressor::STOP_TIME, stamped inside
//     StopCompressor() in Config.h.
//
//  NOTE: Compressor::fullyLoaded is currently a PLACEHOLDER heuristic
//  (VFD frequency at max, or motor current at the high setpoint for
//  fixed-speed drives). Once the Loader/Unloader solenoid sequencing is
//  built, that module should set comp.fullyLoaded directly and
//  updateLoadStatus() below can be removed.
// ============================================================================

bool restartAllowed(Compressor &comp) {
  return (comp.STOP_TIME == 0) || (millis() - comp.STOP_TIME >= RESTART_LOCKOUT_MS);
}

bool compAvailable(Compressor &comp) {
  return comp.COMP_ENABLE == 1
      && comp.state != COMP_TRIPPED
      && !comp.alarmFlag
      && !comp.Machine_Shutdown
      && !comp.Switches_Alarm
      && restartAllowed(comp);
}

// "fullyLoaded" detection:
//  - VFD driven: frequency heuristic (no Loader/Unloader hardware on this path).
//  - Fixed speed (SD/DOL): set directly by updateCapacityControl() in LoaderUnloader.h,
//    which has the real current-based load% estimate - left untouched here.
void updateLoadStatus(Compressor &comp) {
  if (comp.state != COMP_RUNNING) {
    comp.fullyLoaded = false;
    return;
  }
  if (comp.driveSelection == 3) {  // VFD driven
    comp.fullyLoaded = (comp.out_hz >= comp.max_vfd_freq - 1);
  }
}

void demoteAndStopAssist(Compressor &master, Compressor &slave) {
  if (slave.state == COMP_RUNNING || slave.startup_flag) {
    Serial.println("[LeadLag] Releasing SLAVE assist.");
    StopCompressor(slave);
    slave.startup_flag = false;
  }
  master.assistActive = false;
  master.RLA_Limit_Pct = 100;
}

void manageAssist(Compressor &master, Compressor &slave) {
  // ---- Bring SLAVE in to assist (spec 2b/2c) ----
  bool needsAssist = (master.state == COMP_RUNNING) && master.fullyLoaded && (ReturnTemp > ReturnSp);

  if (needsAssist && !master.assistActive && slave.state == COMP_STOPPED && compAvailable(slave)) {
    // Step MASTER down to 50% FIRST to minimize loading of the power source (spec 2c)
    Serial.println("[LeadLag] MASTER fully loaded, target not met -> staging SLAVE assist.");
    master.RLA_Limit_Pct = 50;
    master.assistActive = true;
    master.ASSIST_STEP_TIME = millis();
  }

  if (master.assistActive && slave.state == COMP_STOPPED && !slave.startup_flag
      && millis() - master.ASSIST_STEP_TIME > ASSIST_SETTLE_MS) {
    Serial.println("[LeadLag] Starting SLAVE for assist.");
    Init_Compressor(slave);
  }

  // ---- Release SLAVE once target is met or assist is no longer needed ----
  if (master.assistActive && (slave.state == COMP_RUNNING || slave.startup_flag)
      && (ReturnTemp <= ReturnSp || !master.fullyLoaded)) {
    Serial.println("[LeadLag] Target met / assist no longer needed -> stopping SLAVE.");
    demoteAndStopAssist(master, slave);
  }
}

void manageLeadLag() {
  updateLoadStatus(CompA);
  updateLoadStatus(CompB);

  bool aFitted = CompA.COMP_ENABLE == 1;
  bool bFitted = CompB.COMP_ENABLE == 1;

  // ---- Single-compressor systems: no lead-lag needed ----
  if (aFitted && !bFitted) { CompA.isMaster = true;  CompB.isMaster = false; return; }
  if (bFitted && !aFitted) { CompB.isMaster = true;  CompA.isMaster = false; return; }
  if (!aFitted && !bFitted) return;

  bool neitherRunning = (CompA.state != COMP_RUNNING) && (CompB.state != COMP_RUNNING);

  if (LeadSW == 1) {
    // MANUAL: Comp A forced MASTER (spec 1a)
    CompA.isMaster = true;
    CompB.isMaster = false;
  } else if (LeadSW == 2) {
    // MANUAL: Comp B forced MASTER (spec 1a)
    CompB.isMaster = true;
    CompA.isMaster = false;
  } else if (neitherRunning) {
    // AUTO: lower running hours leads (spec 2a). Decided only while both are
    // idle so roles never flip mid-operation.
    bool aAvail = compAvailable(CompA);
    bool bAvail = compAvailable(CompB);
    if (aAvail && bAvail) {
      CompA.isMaster = (CompA.Run_hrs <= CompB.Run_hrs);
      CompB.isMaster = !CompA.isMaster;
    } else if (aAvail) {
      CompA.isMaster = true;  CompB.isMaster = false;
    } else if (bAvail) {
      CompB.isMaster = true;  CompA.isMaster = false;
    }
  }

  Compressor &master = CompA.isMaster ? CompA : CompB;
  Compressor &slave  = CompA.isMaster ? CompB : CompA;

  // ---- AUTO only: MASTER failure -> promote SLAVE (spec 2b) ----
  if (LeadSW == 0 && master.state == COMP_TRIPPED && compAvailable(slave)) {
    Serial.println("[LeadLag] MASTER tripped -> promoting SLAVE to MASTER.");
    master.isMaster = false;
    slave.isMaster = true;
    manageAssist(slave, master);  // slave is now MASTER, master is now SLAVE
    return;
  }

  manageAssist(master, slave);
}
