CASTLE OF DOOM
==============
CASTLE GATE
EXITS : SE

YOU SEE: KEY

WHAT YOU WANNO DO
?
 
The 'castle of doom' game is an simple adventure, I couldnt identify the original author.

You can access the game in the emulator by typing F4, then RUN.
It is  written entirely in BASIC, so you can LIST the code, and modify it if you want.

How to play:
Text commands are
GO N/S/E/W,
LOOK, GET, DROP,
INVENTORY,USE, EXAMINE,
SCORE, QUIT, HELP

If you want to modify the game externally fron the emulator,
use Notepad++ to load the .bas code, be sure you did Edit/EOL conversion/Unix in the menu.
Then launch PowerShell and type :

Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

.\tokenize.ps1 -InFile castle.bas -OutFile castle.prg

python bin2c.py castle.prg castle.h castle_prg

Then recompile R4vic20.ino with Arduino IDE 2.x.x 
(Arduino UNO R4 Boards 1.5.0, board Arduino R4 minima)
