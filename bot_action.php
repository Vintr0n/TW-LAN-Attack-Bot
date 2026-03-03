<?php
/*
 * bot_action.php — Superbot AJAX Action Handler
 *
 * Handles: start, stop, save, clear_sessions
 * Config stored as serialize() in superbot_config.dat (never opcode-cached).
 * PHP 4.4.4 compatible.
 */

session_start();

$config_file = dirname(__FILE__) . DIRECTORY_SEPARATOR . 'superbot_config.dat';

$defaults = array(
    'bot_active'     => 0,
    'bot_userids'    => '',
    'do_build'       => 1,
    'do_research'    => 1,
    'do_recruit'     => 1,
    'do_attack'      => 0,
    'profile'        => 'offdeff',
    'world_speed'    => 1000,
    'tick_interval'  => 10,
    'build_every'    => 1,
    'research_every' => 1,
    'recruit_every'  => 1,
    'attack_every'   => 6
);

function cfg_read($config_file, $defaults) {
    if (!file_exists($config_file)) return $defaults;
    $raw = file_get_contents($config_file);
    if ($raw === false || $raw == '') return $defaults;
    $data = @unserialize($raw);
    if (!is_array($data)) return $defaults;
    $keys = array_keys($defaults);
    for ($i = 0; $i < count($keys); $i++) {
        if (!isset($data[$keys[$i]])) $data[$keys[$i]] = $defaults[$keys[$i]];
    }
    return $data;
}

function cfg_write($cfg, $config_file) {
    $fp = fopen($config_file, 'w');
    if (!$fp) return false;
    fwrite($fp, serialize($cfg));
    fclose($fp);
    return true;
}

function cfg_verify($config_file, $key, $expected) {
    if (!file_exists($config_file)) return false;
    $raw = file_get_contents($config_file);
    $data = @unserialize($raw);
    return is_array($data) && isset($data[$key]) && $data[$key] == $expected;
}

$cfg    = cfg_read($config_file, $defaults);
$action = isset($_POST['action']) ? $_POST['action'] : '';

if ($action == 'start') {
    $cfg['bot_active'] = 1;
    if (!cfg_write($cfg, $config_file)) {
        echo 'error: cannot write config file - check folder permissions';
        exit;
    }
    if (cfg_verify($config_file, 'bot_active', 1)) {
        $_SESSION['bot_active'] = 1;
        echo 'ok';
    } else {
        echo 'error: wrote file but verify failed';
    }

} else if ($action == 'stop') {
    $cfg['bot_active'] = 0;
    cfg_write($cfg, $config_file);
    $_SESSION['bot_active'] = 0;
    echo 'ok';

} else if ($action == 'save') {
    $raw_ids = isset($_POST['bot_userids']) ? $_POST['bot_userids'] : '';
    $ids = array();
    $parts = explode(',', $raw_ids);
    for ($i = 0; $i < count($parts); $i++) {
        $id = intval(trim($parts[$i]));
        if ($id > 0) $ids[] = $id;
    }
    $cfg['bot_userids'] = implode(',', $ids);

    $p = isset($_POST['profile']) ? $_POST['profile'] : 'offdeff';
    $allowed = array('offdeff', 'off', 'deff', 'eco');
    $ok = false;
    for ($i = 0; $i < count($allowed); $i++) { if ($allowed[$i] == $p) { $ok = true; break; } }
    $cfg['profile']        = $ok ? $p : 'offdeff';
    $cfg['do_build']       = (isset($_POST['do_build'])    && intval($_POST['do_build'])    == 1) ? 1 : 0;
    $cfg['do_research']    = (isset($_POST['do_research']) && intval($_POST['do_research']) == 1) ? 1 : 0;
    $cfg['do_recruit']     = (isset($_POST['do_recruit'])  && intval($_POST['do_recruit'])  == 1) ? 1 : 0;
    $cfg['do_attack']      = (isset($_POST['do_attack'])   && intval($_POST['do_attack'])   == 1) ? 1 : 0;
    $cfg['world_speed']    = max(1,  intval($_POST['world_speed']));
    $cfg['tick_interval']  = max(5,  intval($_POST['tick_interval']));
    $cfg['build_every']    = max(1,  intval($_POST['build_every']));
    $cfg['research_every'] = max(1,  intval($_POST['research_every']));
    $cfg['recruit_every']  = max(1,  intval($_POST['recruit_every']));
    $cfg['attack_every']   = max(1,  intval($_POST['attack_every']));

    $ckeys = array_keys($cfg);
    for ($i = 0; $i < count($ckeys); $i++) { $_SESSION[$ckeys[$i]] = $cfg[$ckeys[$i]]; }

    if (cfg_write($cfg, $config_file)) {
        echo 'ok';
    } else {
        echo 'error: cannot write config - check folder write permissions';
    }

} else if ($action == 'clear_sessions') {
    // Delete all stored session cookie files
    require_once('bot_session.php');
    $count = superbot_clear_sessions();
    echo 'ok:' . $count;

} else if ($action == 'save_profile') {
    // Save per-user army profile to bot_villages table
    $uid = intval(isset($_POST['uid']) ? $_POST['uid'] : 0);
    $p   = isset($_POST['profile']) ? $_POST['profile'] : 'offdeff';
    $allowed = array('offdeff', 'off', 'deff');
    $ok = false;
    for ($i = 0; $i < count($allowed); $i++) { if ($allowed[$i] == $p) { $ok = true; break; } }
    if (!$ok) $p = 'offdeff';

    if ($uid <= 0) { echo 'error: invalid uid'; exit; }

    require_once('bot_core.inc');
    $conn = @mysql_connect($DB_HOST, $DB_USER, $DB_PASS);
    if (!$conn || !@mysql_select_db($DB_NAME, $conn)) { echo 'error: db'; exit; }

    // Get all village IDs for this user and upsert bot_villages rows.
    // DELETE + INSERT pattern avoids silent failures from duplicate key or
    // partial rows left behind after a world reset.
    $vq = mysql_query("SELECT id FROM villages WHERE userid='" . $uid . "'", $conn);
    $saved = 0;
    if ($vq) {
        while ($vr = mysql_fetch_assoc($vq)) {
            $vid = intval($vr['id']);
            // Remove any existing row first, then insert fresh — avoids UPDATE silently
            // matching 0 rows if the row was dropped during a world reset.
            mysql_query("DELETE FROM bot_villages WHERE uid='" . $uid . "' AND vid='" . $vid . "'", $conn);
            $ins = mysql_query(
                "INSERT INTO bot_villages (uid, vid, type) VALUES ('" . $uid . "', '" . $vid . "', '" . $p . "')",
                $conn
            );
            if ($ins) $saved++;
        }
    }
    mysql_close($conn);
    if ($saved > 0) {
        echo 'ok:' . $saved;
    } else {
        echo 'error: no villages found for uid=' . $uid . ' (village may not exist yet — bot will auto-create on first tick)';
    }

} else {
    echo 'error: unknown action';
}
?>
