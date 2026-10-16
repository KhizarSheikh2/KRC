#pragma once

void initDisplayLink();
void serviceDisplayLink();
bool sendAM3HardwareCommand(const char* command);
bool sendAM3StartCommand();
bool sendAM3StopCommand();
bool sendAM3ResetCommand();
