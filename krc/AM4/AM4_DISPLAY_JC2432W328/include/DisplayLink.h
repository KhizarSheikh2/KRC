#pragma once

void initDisplayLink();
void serviceDisplayLink();

bool sendAM4HardwareCommand(const char* command);
bool sendAM4StartCommand();
bool sendAM4StopCommand();
bool sendAM4ResetCommand();
