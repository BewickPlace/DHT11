DHT11 wiringPi Driver
====================
By John Chandler, created using numerouse sources

What is it
----------
This program is an example of how to read Pressure and Temperature data from the common DHT11 sensor.

The program includes support for an illuminated switch.

Pin Usage
---------

All pins can be changed in the .h header definitions, however the base setup is as follows:

                   ---------
                   | 1 | 2 |
                   | 3 | 4 | +5v
  Switch Write Pin | 5 | 6 | GRND
   Switch Read Pin | 7 | 8 | DHT11 Read/Write Data
             GRND  | 9 |   |
                   |   |   |

The program uses wiringPi to access the GPIO pins, and utilises Physical pin numbering mode.


