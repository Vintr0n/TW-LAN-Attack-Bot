<?php
/*
 * bot.php — Superbot AJAX Tick Worker
 *
 * Called by the browser every N seconds (via script.js AJAX).
 * Processes ALL villages for ALL selected bot users on every tick.
 *
 * Session strategy:
 *   - Uses bot_session.php to maintain real HTTP sessions for each bot user
 *   - Each tick pings game.php?screen=overview which triggers the game's own
 *     event processor (builds finish, recruits finish, resources accumulate)
 *   - Falls back to direct DB event processing if curl is unavailable
 *
 * PHP 4.4.4 compatible.
 */

$config_file  = dirname(__FILE__) . DIRECTORY_SEPARATOR . 'superbot_config.dat';
$counter_file = dirname(__FILE__) . DIRECTORY_SEPARATOR . 'superbot_counters.dat';

$defaults = array(
    'bot_active'     => 0,
    'bot_userids'    => '',
    'do_build'       => 1,
    'do_research'    => 1,
    'do_recruit'     => 1,
    'do_attack'      => 0,
    'world_speed'    => 1000,
    'tick_interval'  => 10,
    'build_every'    => 1,
    'research_every' => 1,
    'recruit_every'  => 1,
    'attack_every'   => 6
);

// Fresh read — bypass any opcode cache
$cfg = $defaults;
if (file_exists($config_file)) {
    $raw  = file_get_contents($config_file);
    $data = @unserialize($raw);
    if (is_array($data)) {
        $keys = array_keys($defaults);
        for ($i = 0; $i < count($keys); $i++) {
            if (isset($data[$keys[$i]])) $cfg[$keys[$i]] = $data[$keys[$i]];
        }
    }
}

if (empty($cfg['bot_active'])) {
    echo '[bot] Bot is stopped.' . "\n";
    exit;
}

require_once('bot_core.inc');
require_once('bot_session.php');

$raw_ids        = $cfg['bot_userids'];
$do_build       = !empty($cfg['do_build']);
$do_research    = !empty($cfg['do_research']);
$do_recruit     = !empty($cfg['do_recruit']);
$do_attack      = !empty($cfg['do_attack']);
$world_speed    = intval($cfg['world_speed']);
$build_every    = max(1, intval($cfg['build_every']));
$research_every = max(1, intval($cfg['research_every']));
$recruit_every  = max(1, intval($cfg['recruit_every']));
$attack_every   = max(1, intval($cfg['attack_every']));

// Per-action tick counters
$cdefs = array('build_tick' => 0, 'research_tick' => 0, 'recruit_tick' => 0, 'attack_tick' => 0);
$counters = $cdefs;
if (file_exists($counter_file)) {
    $raw  = file_get_contents($counter_file);
    $data = @unserialize($raw);
    if (is_array($data)) {
        $ckeys = array_keys($cdefs);
        for ($i = 0; $i < count($ckeys); $i++) {
            if (isset($data[$ckeys[$i]])) $counters[$ckeys[$i]] = $data[$ckeys[$i]];
        }
    }
}

$counters['build_tick']++;
$counters['research_tick']++;
$counters['recruit_tick']++;
$counters['attack_tick']++;

$fire_build    = $do_build    && ($counters['build_tick']    >= $build_every);
$fire_research = $do_research && ($counters['research_tick'] >= $research_every);
$fire_recruit  = $do_recruit  && ($counters['recruit_tick']  >= $recruit_every);
$fire_attack   = $do_attack   && ($counters['attack_tick']   >= $attack_every);

if ($fire_build)    $counters['build_tick']    = 0;
if ($fire_research) $counters['research_tick'] = 0;
if ($fire_recruit)  $counters['recruit_tick']  = 0;
if ($fire_attack)   $counters['attack_tick']   = 0;

$fp = fopen($counter_file, 'w');
if ($fp) { fwrite($fp, serialize($counters)); fclose($fp); }

// Parse user IDs
$uid_list = array();
if ($raw_ids != '') {
    $parts = explode(',', $raw_ids);
    for ($i = 0; $i < count($parts); $i++) {
        $id = intval(trim($parts[$i]));
        if ($id > 0) $uid_list[] = $id;
    }
}
if (count($uid_list) == 0) {
    echo '[bot] No bot users selected.' . "\n";
    exit;
}

$conn = @mysql_connect($DB_HOST, $DB_USER, $DB_PASS);
if (!$conn) { echo '[bot] DB connect failed: ' . mysql_error() . "\n"; exit; }
if (!@mysql_select_db($DB_NAME, $conn)) { echo '[bot] DB failed: ' . mysql_error($conn) . "\n"; exit; }

// --- STEP 1: Ping game for each bot user via real HTTP session ---
// This is the crucial step that triggers the game's own event processor.
// superbot_ping_all_users() will log in if the session has expired.
$ping_results = superbot_ping_all_users($uid_list, $conn);

$ping_log = '';
$pkeys = array_keys($ping_results);
for ($pi = 0; $pi < count($pkeys); $pi++) {
    $puid = $pkeys[$pi];
    $pres = $ping_results[$puid];
    if ($pres['status'] != 'ok') {
        // Only log non-routine events (logins, failures)
        $ping_log .= '[' . date('H:i:s') . '] SESSION: ' . $pres['msg'] . "\n";
    }
}

// --- STEP 2: Also process events directly in DB (fallback + belt-and-suspenders) ---
// Handles the case where curl isn't available, and also catches any events
// the game didn't process on its own.
$all_vids = array();
for ($i = 0; $i < count($uid_list); $i++) {
    $uid = $uid_list[$i];
    $vq = mysql_query("SELECT id FROM villages WHERE userid='" . $uid . "' ORDER BY id ASC", $conn);
    if ($vq) {
        while ($vr = mysql_fetch_assoc($vq)) {
            $all_vids[] = array('vid' => intval($vr['id']), 'uid' => $uid);
        }
    }
}

// Ensure every village has a bot_villages row — creates with default 'offdeff'
// profile if the UI missed inserting it (e.g. village created after profile was set).
for ($i = 0; $i < count($all_vids); $i++) {
    $bvid = $all_vids[$i]['vid'];
    $buid = $all_vids[$i]['uid'];
    $ex = mysql_query(
        "SELECT vid FROM bot_villages WHERE uid='" . $buid . "' AND vid='" . $bvid . "'", $conn
    );
    if (!$ex || mysql_num_rows($ex) == 0) {
        mysql_query(
            "INSERT INTO bot_villages (uid, vid, type) VALUES ('" . $buid . "', '" . $bvid . "', 'offdeff')",
            $conn
        );
    }
}

$total = count($all_vids);
if ($total == 0) {
    echo '[bot] No villages found for selected users.' . "\n";
    mysql_close($conn);
    exit;
}

// NOTE: superbot_apply_production and superbot_process_events are called
// inside superbot_tick_village() for each village. Do NOT call them here
// as well — that causes trickle to fire twice per tick, duplicating troops.

// --- STEP 3: Run bot actions on all villages ---
$output = $ping_log;
for ($i = 0; $i < $total; $i++) {
    $entry = $all_vids[$i];
    $output .= superbot_tick_village(
        $entry['vid'], $entry['uid'], $total, $i,
        $COL, $BUILDINGS, $UNITS, $ATTACK_CONFIG,
        $fire_build, $fire_research, $fire_recruit, $fire_attack,
        $world_speed, $conn
    );
}

mysql_close($conn);
echo $output;
?>
