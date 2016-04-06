As things are completed, they will be listed here and categorized by week.

## 4/29/10 - 5/5/10 ##
  * Hours and hours of debugging. Problems don't even make sense anymore; everything looks correct except for the behavior when it is not in debug mode.
  * Finished building second board, mounted speakers, can transmit from one unit to the other but the error rate is far too high.
  * Found and fixed the error from above. Type conversion problem.
  * TWO FREQUENCIES! Simultaneously transmit and receive on two frequencies with independent state machines, etc. Need to add more frequencies (should be easy after setting parameters).
  * Also, implemented some channel equalization in the matlab script. Values chosen by experiment, but it helps deal with our filter not having a flat pass band.
  * Fixed the second unit's output amplifier. The current mirror was not producing enough current, leading to clipping. Differences in discrete transistors are crazy.
  * Some final updates:
  * Finalized microcontroller code, it works in both directions and is relatively error free for ASCII characters. For some reason anything with bit 8 set has a much higher error rate, but no time to investigate/fix.
  * Wrote report.
  * Implemented transport layer protocol handler for use in windows C++ programs to communicate over acoustic modem.

## 4/22/10 - 4/28/10 ##
  * Managed to get real data dumps off the AVR with some sketchy serial code.
  * Revamped the matlab script to correctly trigger on aforementioned real data.
  * Successful transmission with a bit-error rate of less than 5%, up to 5 inches.
  * Some documentation for website/report.
  * Fixed problems, system works really well with new triggering code and fixed buffer traversal (low to no errors, depending on ambiance).
  * Finished building more complete transmit/receive unit. Some problems with transistors not doing as well but otherwise it's basically the same as the breadboarded one.
  * Started converting the original breadboard into another soldered one since the first one worked.

## 4/15/10 - 4/21/10 ##
  * Completed full schematic (in LTSpice's Schematic Editor)
  * Built microphone test circuit--results are poor.
  * Improved microphone circuit with an op-amp. Result is pretty good, even with some distance. Gain is ~70x and somehow it does not amplify noise, despite having no reason to reject it as common mode (the inputs are not differential and are not even from the same parts).
  * Wrote matlab detection code for semi-arbitrary starting position for a sinewave burst. Looks pretty good.
  * Ported to C. Almost works, but there are some mysterious problems relating to memory location and potentially sketchy code.
  * Built second Atmega carrier for receiver unit.

## 4/8/10 - 4/14/10 ##
  * Made flash memory tables for all possible combinations of waveform generation.
  * Full output circuit completed and tested with speaker load.
  * Able to synthesize output waveforms for serial data. Possibly one byte at a time at the moment, but maybe not.
  * Began documenting the schematics that are actually building, which are largely different from the originally proposed schematics.

## 3/31/10 - 4/7/10 ##
  * Built Atmega Carrier (and tested)
  * Built amplifier circuit
  * Built DAC circuit