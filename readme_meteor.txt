   XXXX
   XXXX
   
   
   
   
			|
 
           XX   
 
The meteor game is an adapted version of 
VIC-20 User's Manual (Personal Computing on the VIC-20) 1982.
Allen Huffman
https://github.com/allenhuffman/VIC-20/tree/main/vic-20%20manual%20games/meteor

You can access the game in the emulator by typing F2, then RUN.
It is  written entirely in BASIC, so you can LIST the code, and modify it if you want.

How to play:
Use any key to lauch a missile from your Base,
in order to destroy the Meteor before it hit your Base.

If you want to modify the game externally fron the emulator,
use Notepad++ to load the .bas code, be sure you did Edit/EOL conversion/Unix in the menu.
Then launch PowerShell and type :

Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

.\tokenize.ps1 -InFile meteor.bas -OutFile meteor.prg

python bin2c.py meteor.prg meteor.h meteor_prg

Then recompile R4vic20.ino with Arduino IDE 2.x.x 
(Arduino UNO R4 Boards 1.5.0, board Arduino R4 minima)
