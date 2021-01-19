	<?php
$page = $_SERVER['PHP_SELF'];
$sec = "85";//60 is one minute, 60000 just to stop it
?>
<html>
    <head>
    <meta http-equiv="refresh" content="<?php echo $sec?>;URL='<?php echo $page?>'">
	
	<script>
		
	var timeleft = 85;
	var downloadTimer = setInterval(function(){
	if(timeleft <= 0){
		clearInterval(downloadTimer);
		document.getElementById("countdown").innerHTML = "Finished";
	} else {
    document.getElementById("countdown").innerHTML = timeleft + " seconds remaining";
  }
  timeleft -= 1;
}, 1000);

	</script>
    </head>
    <body>
	
<div id="countdown"></div>
<br>

<?php

$dbname = 'lan';
$dbuser = 'root';
$dbpass = '';
$dbhost = 'localhost';

$connect = mysql_connect($dbhost, $dbuser, $dbpass) or die("Unable to Connect to '$dbhost'");
mysql_select_db($dbname) or die("Could not open the db '$dbname'");

$users = mysql_query("SELECT id, points, username FROM users");

$maxquery= mysql_query("SELECT MAX(id) AS maximum FROM users");
$maxrow = mysql_fetch_assoc($maxquery); 
$maximumID = $maxrow['maximum'];


$maxVillagequery= mysql_query("SELECT MAX(id) AS maximum FROM villages");
$maxVillagerow = mysql_fetch_assoc($maxquery); 
$maximumVillageID = $maxrow['maximum'];

//echo ("This is the maximum player ID: $maximumID <br>");


while ($row = mysql_fetch_array($users, MYSQL_BOTH)) {
	
	//echo $row['id'].' '.$row["points"].'<br>';
	
	$id = $row['id'];
	$username = $row['username'];
	$points = $row['points'];

	

	// THIS removes players that have 0 points
	if ($points < 79){
		echo 
		$deleteuser = mysql_query("DELETE FROM `users` WHERE points < 79 ");
		$deletevillage = mysql_query("DELETE FROM `villages` WHERE points < 79 ");
		echo '______________________________________________________<br>';
		echo '<b><p style="color:red">'.$username.' HAS BEEN DEFEATED</p></b>';
		echo '______________________________________________________<br>';
	}

	
	
	$village = mysql_query("SELECT * FROM villages where userid = $id");
	
	while ($row = mysql_fetch_array($village, MYSQL_BOTH)) {
		
		$villageid = $row['id'];
		$userid = $row['userid'];
		$villagename = $row['name'];
		$villagepoints = $row['points'];
		$spears = $row['all_unit_spear'];
		$swords = $row['all_unit_sword'];
		$axes = $row['all_unit_axe'];
		$archers = $row['all_unit_archer'];
		$spy = $row['all_unit_spy'];
		$lightcav = $row['all_unit_light'];
		$mountarch  = $row['all_unit_marcher'];
		$heavycav = $row['all_unit_heavy'];
		$ram = $row['all_unit_ram'];
		$cata = $row['all_unit_catapult'];
		$knight = $row['all_unit_knight'];
		$nobles = $row['all_unit_snob'];
		
		$troopcount = $spears + $axes + $swords + $archers +   $spy + $lightcav + $heavycav;
		
		
		// Chance of attacking here: *0.0075 6000pts+ = 45% chance (1000pts = 8% chance, rounded).
		$chance = $villagepoints*0.0075;
		

	//echo '<br>'.$id.' '.$username.' | Points: '.$points.' | Chance of attack: '.$chance.' | Axes: '.$axes.' | Spears: '.$spears.' | Swords: '.$swords.' | Spies: '.$spy.' | LC: '.$lightcav.' | Troops total: '.$troopcount.'<br>'  ;
	
	$randomchance = rand(1, 100);
	echo 'Village name: '.$villagename.'<br>';
	echo 'Chance check: '.$chance.'<br>';
	echo 'Rolled      : '.$randomchance.'<br>';
	echo 'Unit count  : '.$troopcount.'<br>';
	
	
	//increased this to 1000000 from 100
	$rand_movementid = rand(1, 1000000);
	$rand_eventid = rand(1, 1000000);
	
	
	//THIS SOLVES THE ATTACK SELF ISSUE
	//IT CHECKS FOR villages that are not the users/attackers villages
	//$rand_enemy = rand(1,$maximumVillageID);
	//$rand_enemy = rand(1,$maximumID);
	$rand_enemy_query = mysql_query("SELECT * FROM villages WHERE userid <> '$id' ORDER BY RAND() LIMIT 1");
	while ($row = mysql_fetch_array($rand_enemy_query, MYSQL_BOTH)) {
	$rand_enemy = $row['id'];
	echo 'Random enemy village: '.$rand_enemy.'<br>';
	echo '______________________________________________________<br>';
}



	//Check each player for chance to attack, attack if true ($id > 1 stops player1's account sending attacks)
	if($randomchance <= $chance AND $troopcount > 300 AND $id > 0 AND $villagepoints > 300 AND empty($rand_enemy)==0){
		
		echo '<b><p style="color:red">'.$username.' attacks village ID: '.$rand_enemy.'</p></b>';
		echo '______________________________________________________<br>';
		
		//Random movement time
		$randomMovementTime = rand(21, 41);
		
		//Send the movement - TO DO - need to change the to village to the enemy or target village variable (need to create) 
		$movement = mysql_query("INSERT INTO  `movements` (  `id` ,  `from_village` ,  `to_village` ,  `units` ,  `type` ,  `start_time` ,  `end_time` ,  `building` ,                                       `from_userid` ,  `to_userid` ,  `to_hidden` ,  `wood` ,  `stone` ,  `iron` ,  `send_from_village` ,  `send_from_user` ,  `send_to_user` ,  `send_to_village` ,  `die` ) 
VALUES (
$rand_movementid,  $villageid,  '$rand_enemy',  '$spears;$swords;$axes;$archers;$spy;$lightcav;$mountarch;$heavycav;$ram;$cata;$knight;$nobles',  'attack',  UNIX_TIMESTAMP(NOW()),  UNIX_TIMESTAMP(NOW())+$randomMovementTime, NULL ,  '$id',           $rand_enemy,  '0',  '0',  '0',  '0',  $villageid,  '$id',       $rand_enemy,     '$rand_enemy',      '0'
);");
		
		
		//Record the event
		$event = mysql_query("INSERT INTO  `events` (  `id` ,  `event_time` ,  `event_type` ,  `event_id` ,  `user_id` ,  `villageid` ,  `knot_event` ,  `cid` ,  `can_knot` ,  `is_locked` ) 
VALUES (
$rand_eventid,  UNIX_TIMESTAMP(NOW())+$randomMovementTime,  'movement',  $rand_movementid,  $id,  $villageid,  '',  '0',  '0',  ''
);
");

		//Remove the troops from the unit_place table
		$event = mysql_query("UPDATE unit_place SET 
unit_spear = 0,
unit_sword = 0,
unit_axe = 0,
unit_archer = 0,
unit_spy = 0,
unit_light = 0,
unit_marcher = 0,
unit_heavy = 0,
unit_ram = 0,
unit_catapult = 0,
unit_knight = 0,
unit_snob  = 0 WHERE villages_from_id = $villageid");
	}
	}
}
?>
   </body>
</html>
