   1 2 3 4 5 6 7 8
8  R N B Q K B N R
7  P P P P P P P P
6  .   .   .   .  
5    .   .   .   .
4  .   .   .   .  
3    .   .   .   .
2  P P P B P P P P
1  R N B  Q K B N R
YOUR MOVE         
FROM?  

The chess game is an adapted version of 
Clifford Ramshaw's "VIC Innovative Computing" (GB), Melbourne House Publishers, 1982.
http://www.vic20listings.freeolamail.com/books.html

You can access the game in the emulator by typing F3, then RUN.
It is  written entirely in BASIC, so you can LIST the code, and modify it if you want.

How to play:
Wait for the board to appear, then enter your move like this : FROM 1,2 TO 1,4
This will move your pawn from A2 to A4.
Of course the ELO rank is absolutely terrible. But it is fun to play anyway.

If you want to modify the game externally fron the emulator,
use Notepad++ to load the .bas code, be sure you did Edit/EOL conversion/Unix in the menu.
Then launch PowerShell and type :

Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

.\tokenize.ps1 -InFile chess.bas -OutFile chess.prg

python bin2c.py chess.prg chess.h chess_prg

Then recompile R4vic20.ino with Arduino IDE 2.x.x 
(Arduino UNO R4 Boards 1.5.0, board Arduino R4 minima)
