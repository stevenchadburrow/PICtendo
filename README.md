# PICtendo
<b>Combining PICnes and PICboy emulators into one</b>

PICtendo combines both <a href="https://github.com/stevenchadburrow/PICnes">PICnes</a> and <a href="https://github.com/stevenchadburrow/PICboy">PICboy</a> into a single emulator with common functionality and memory spaces.  Most of the conventions and controls came from PICboy as that was the more recent emulator.

The desktop version of PICtendo uses OpenGL/GLFW and OpenAL for keyboard input and video/audio output.  This implementation with OpenAL is quite bad, but it sounds much better on a microcontroller with an R2R DAC.   The microcontroller targeted for PICtendo is the PIC32MZ running at 260 MHz with 512KB of RAM and 2MB of ROM.  This has been demonstrated multiple times on my past projects, particularly <a href="https://github.com/stevenchadburrow/AcolyteHandPICd32">here</a> and <a href="https://github.com/stevenchadburrow/AcolyteHandheld">here</a>.  
