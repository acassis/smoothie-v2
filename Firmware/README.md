To build the test cases do ```rake testing=1```
then flash to Bambino, the results print out to the uart/serial

To make the Firmware do ```rake```

Firmware currently runs on UART at 115200 baud and on USB serial at ttyACM0.
On USB Serial you need to hit Enter to get it to start.

The config file will be config.ini an example is shown here... 
https://gist.github.com/8996097730ba1f17413e3aad1c98eaaf

Currently the config.ini is builtin and defined in string-config.h

Currently Enough is ported to be able to run a 3D Printer using a Bambino board and external drivers.
XYZ and an extruder and temperture controls should work, but needs extensive testing.
