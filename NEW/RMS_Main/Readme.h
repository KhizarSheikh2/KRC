/*
iocon1(0) = Light 1
iocon1(1) = Light 2
iocon1(2) = TV
iocon1(3) = Shutter Down  ioCon1(3, LOW), ioCon1(4, HIGH);
iocon1(4) = Shutter Up  ioCon1(3, HIGH), ioCon1(4, LOW);
iocon1(5) = Curtain Reverse  ioCon1(5, LOW), ioCon1(6, HIGH);
iocon1(6) = Curtain Forward  ioCon1(5, HIGH), ioCon1(6, LOW);
iocon1(7) = Light 3


Pin 0: Light1 relay
Pin 1: Light2 relay
Pin 2: AC relay
Pin 7: Room Fan relay  
Pin 8: TV relay        ← NEW!
Pin 3/4: Shutter motor
Pin 5/6: Curtain motor



DX Coil Outputs
operateRS485Relay(0) = Mode 0
operateRS485Relay(1) = Mode 1
operateRS485Relay(2) = Mode 2 
operateRS485Relay(3) = (High If) Winter
operateRS485Relay(4) = (High If) Summer 





*/