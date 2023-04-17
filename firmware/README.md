# Super Serial Card Firmware  
  
This folder contains the SSC firmware.  To assemble, use the command:  
cl65 SSC.s -C SSC.cfg -o SSC.bin  
  
## Notes
1. I had to copy cc65/asminc/apple2.mac in here as APPLE2.mac otherwise cl65 complains about a file not found  
2. I modified the original sources supplied to me just a little so it would compile  
