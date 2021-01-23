# TW-LAN-Attack-Bot
Tribal Wars LAN attack bot
____________________________________________________________________________________________________

Disclaimer: I did not create TW LAN, that can be found here: https://twlan.github.io/
            This should only be played in an offline capacity do not attempt to 
            host this and play this with 
            
            I assume no responsibility or liability for any errors or omissions 
            in the content of TW LAN or this bot. The information contained within 
            this/these script(s) is provided on an "as is" basis with no guarantees 
            of completeness, accuracy, usefulness or timeliness.
            
            Official game belongs to InnoGames™️ and can be accessed here: http://tribalwars.net/
            
____________________________________________________________________________________________________

If you are currently playing this fantasic browser based game offline using the TW LAN files and the 
LAN based MySQL tables but find that the other bots, such as Superbot, do not simulate enemies
attacking one another then this is the bot for you.

This bots aim is to simulate each enemy sending attacks based on village points. 

NB: This bot works well with the auto build bot - SuperBot. Typically I run them together.

--------------
Installation - prerequisite
--------------
1. Download and install https://twlan.github.io/
2. Ensure you are able to connect to the MySQL database 
3. Ensure you are able to access the admin pages 127.0.0.1/admin
4. Ensure you are able to use Superbot 127.0.0.1/superbot/index.php (or equivalent build bot)
5. Create your player & AI accounts

--------------
Installation and use of TW LAN attack bot
--------------
1. Place TWLANattackbot.php in the htcdocs folder (or the hosting server web folder)
2. Navigate to the web page 127.0.0.1/TWLANattackbot.php
3. Change refresh speed as you see fit currently set to 80 seconds (would NOT recommends less than 60
   as you could attempt to send attacks before the preivous one has landed and returned).
4. Change settings as you see fit
