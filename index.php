<?php
/*
 * index.php — Superbot TWLan NPC Controller
 * PHP 4.4.4 compatible.
 * Consolidated: removed run_bot.bat / headless runner tab (not needed).
 * Session management handled by bot_session.php via real HTTP curl requests.
 */
session_start();
require_once('bot_core.inc');

// Load config from disk (never include — opcode cache)
$config_file = dirname(__FILE__) . DIRECTORY_SEPARATOR . 'superbot_config.dat';
$SUPERBOT_CFG = array();
if (file_exists($config_file)) {
    $raw = file_get_contents($config_file);
    $parsed = @unserialize($raw);
    if (is_array($parsed)) $SUPERBOT_CFG = $parsed;
}

function cfg($key, $default, $SUPERBOT_CFG) {
    if (isset($SUPERBOT_CFG[$key])) return $SUPERBOT_CFG[$key];
    if (isset($_SESSION[$key]))     return $_SESSION[$key];
    return $default;
}

$bot_active     = (bool) cfg('bot_active',     0,         $SUPERBOT_CFG);
$bot_userids    =        cfg('bot_userids',    '',        $SUPERBOT_CFG);
$do_build       = (bool) cfg('do_build',       1,         $SUPERBOT_CFG);
$do_research    = (bool) cfg('do_research',    1,         $SUPERBOT_CFG);
$do_recruit     = (bool) cfg('do_recruit',     1,         $SUPERBOT_CFG);
$do_attack      = (bool) cfg('do_attack',      0,         $SUPERBOT_CFG);
$profile        =        cfg('profile',        'offdeff', $SUPERBOT_CFG);
$world_speed    =  intval(cfg('world_speed',   1000,      $SUPERBOT_CFG));
$tick_interval  =  intval(cfg('tick_interval', 10,        $SUPERBOT_CFG));
$build_every    =  intval(cfg('build_every',   1,         $SUPERBOT_CFG));
$research_every =  intval(cfg('research_every',1,         $SUPERBOT_CFG));
$recruit_every  =  intval(cfg('recruit_every', 1,         $SUPERBOT_CFG));
$attack_every   =  intval(cfg('attack_every',  6,         $SUPERBOT_CFG));

$selected_ids = array();
if ($bot_userids != '') {
    $parts = explode(',', $bot_userids);
    for ($i = 0; $i < count($parts); $i++) {
        $v = intval(trim($parts[$i]));
        if ($v > 0) $selected_ids[] = $v;
    }
}

$conn  = @mysql_connect($DB_HOST, $DB_USER, $DB_PASS);
$db_ok = ($conn && @mysql_select_db($DB_NAME, $conn));

$users = array();
if ($db_ok) {
    $uq = mysql_query(
        "SELECT u.id, u.username," .
        " (SELECT COUNT(*) FROM villages v2 WHERE v2.userid=u.id) AS vcount" .
        " FROM users u WHERE u.id>0 ORDER BY u.username ASC",
        $conn
    );
    if ($uq) while ($r = mysql_fetch_assoc($uq)) $users[] = $r;
}

// Check curl availability
$curl_ok = function_exists('curl_init');

function js_str($s) {
    $s = str_replace('\\','\\\\', $s);
    $s = str_replace("'", "\\'",  $s);
    $s = str_replace("\r",'',     $s);
    $s = str_replace("\n",'\\n',  $s);
    return $s;
}
function is_sel($id, $arr) {
    for ($i=0;$i<count($arr);$i++) { if ($arr[$i]==$id) return true; }
    return false;
}
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01//EN">
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Superbot &mdash; TWLan NPC Controller</title>
<script type="text/javascript" src="script.js"></script>
<style type="text/css">
@import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Exo+2:wght@300;400;600;700&display=swap');

* { box-sizing: border-box; }

body {
    font-family: 'Exo 2', Verdana, Arial, sans-serif;
    font-size: 12px;
    background: #0d0f14;
    color: #b8bcc8;
    margin: 0;
    padding: 0;
    min-height: 100vh;
}

/* Top header bar */
.topbar {
    background: linear-gradient(90deg, #0d1420 0%, #121828 50%, #0d1420 100%);
    border-bottom: 1px solid #1e3050;
    padding: 10px 18px;
    display: flex;
    align-items: center;
    justify-content: space-between;
}
.topbar-title {
    font-family: 'Exo 2', sans-serif;
    font-size: 16px;
    font-weight: 700;
    color: #e8c060;
    letter-spacing: 1px;
}
.topbar-title span { color: #6888b0; font-weight: 300; }
.topbar-right { display: flex; align-items: center; gap: 12px; }

/* Layout */
.layout-wrap { display: table; width: 100%; border-collapse: collapse; padding: 14px; }
.sidebar { display: table-cell; width: 230px; vertical-align: top; padding-right: 14px; }
.maincol { display: table-cell; vertical-align: top; }

/* Panels */
.panel {
    background: #141720;
    border: 1px solid #1e2535;
    border-radius: 6px;
    padding: 10px 12px;
    margin-bottom: 10px;
}
.panel-title {
    font-family: 'Exo 2', sans-serif;
    font-size: 10px;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 2px;
    color: #3a5880;
    margin: 0 0 8px 0;
    padding-bottom: 6px;
    border-bottom: 1px solid #1a2535;
}

/* Status indicators */
.status-row { display: flex; align-items: center; margin: 5px 0; }
.sdot {
    display: inline-block;
    width: 8px; height: 8px;
    border-radius: 50%;
    margin-right: 8px;
    flex-shrink: 0;
}
.dg { background: #40d060; box-shadow: 0 0 6px #40d060; }
.dr { background: #d04040; box-shadow: 0 0 6px #d04040; }
.dy { background: #d0a030; box-shadow: 0 0 6px #d0a030; }
#bot_status_text { color: #8898b0; font-size: 12px; }
#bot_status_text strong { color: #40d060; }
#next_tick { color: #3a5060; font-size: 10px; margin-top: 3px; margin-left: 16px; font-family: 'Share Tech Mono', monospace; }

/* Bot action link */
#botact a {
    color: #e8c060;
    font-weight: 700;
    text-decoration: none;
    font-size: 13px;
    letter-spacing: 0.5px;
}
#botact a:hover { color: #fff; }
#save_status { color: #40c060; font-size: 10px; margin-left: 6px; }

/* User list */
.user-list {
    max-height: 120px;
    overflow-y: auto;
    background: #0d0f14;
    border: 1px solid #1e2535;
    border-radius: 4px;
    padding: 3px;
    margin: 4px 0;
}
.user-item { padding: 3px 5px; border-radius: 3px; cursor: pointer; display: flex; align-items: center; }
.user-item:hover { background: #181c28; }
.user-item input { margin-right: 7px; cursor: pointer; }
.user-item label { cursor: pointer; color: #c8cad0; flex: 1; }
.user-item .vcnt { color: #3a5060; font-size: 10px; }

/* Form elements */
.panel label.lbl {
    display: block;
    margin: 7px 0 2px 0;
    color: #4a6070;
    font-size: 10px;
    text-transform: uppercase;
    letter-spacing: 1px;
}
.panel select, .panel input[type="text"] {
    width: 100%;
    background: #0d0f14;
    color: #b8bcc8;
    border: 1px solid #1e2a40;
    border-radius: 4px;
    padding: 4px 7px;
    font-size: 12px;
    font-family: 'Exo 2', Verdana, sans-serif;
}
.panel select:focus, .panel input[type="text"]:focus {
    outline: none;
    border-color: #3a5880;
}

/* Checkboxes */
.cbrow { margin: 5px 0; display: flex; align-items: center; }
.cbrow input { margin-right: 7px; cursor: pointer; }
.cbrow label { cursor: pointer; color: #9098a8; font-size: 12px; }

/* Timing grid */
.timing-grid { width: 100%; border-collapse: collapse; margin-top: 2px; }
.timing-grid td { padding: 2px 2px; font-size: 11px; color: #4a6070; vertical-align: middle; }
.timing-grid input[type="text"] {
    width: 52px;
    background: #0d0f14;
    color: #b8bcc8;
    border: 1px solid #1e2a40;
    border-radius: 3px;
    padding: 3px 5px;
    font-size: 12px;
    font-family: 'Share Tech Mono', monospace;
}

/* Buttons */
.btn {
    padding: 6px 14px;
    border-radius: 4px;
    border: none;
    cursor: pointer;
    font-size: 11px;
    font-weight: 600;
    font-family: 'Exo 2', Verdana, sans-serif;
    letter-spacing: 0.5px;
    margin: 3px 2px 3px 0;
    transition: opacity 0.15s;
}
.btn:hover { opacity: 0.85; }
.btn-save  { background: #1e3a60; color: #80b0e0; border: 1px solid #2a5080; }
.btn-start { background: #1a4a28; color: #60d080; border: 1px solid #2a6038; }
.btn-stop  { background: #4a1a1a; color: #e06060; border: 1px solid #6a2828; }
.btn-clear { background: #1e2a1e; color: #6a8870; border: 1px solid #2a4030; font-size: 10px; padding: 4px 10px; }

/* Warning / info banners */
.warn-bar {
    background: #2a1a00;
    border: 1px solid #6a4a00;
    border-radius: 5px;
    padding: 8px 12px;
    color: #d0a040;
    font-size: 11px;
    margin-bottom: 10px;
}
.info-bar {
    background: #001a2a;
    border: 1px solid #004060;
    border-radius: 5px;
    padding: 8px 12px;
    color: #4090c0;
    font-size: 11px;
    margin-bottom: 10px;
}
.err-bar {
    background: #2a0808;
    border: 1px solid #6a1818;
    border-radius: 5px;
    padding: 8px 12px;
    color: #e08080;
    margin-bottom: 10px;
}
.hint { color: #3a5060; font-size: 10px; line-height: 1.6; margin-top: 3px; }

/* Tabs */
.tab-bar { display: flex; gap: 3px; margin-bottom: 0; }
.tab {
    display: inline-block;
    padding: 5px 14px;
    border-radius: 5px 5px 0 0;
    text-decoration: none;
    font-size: 11px;
    font-weight: 600;
    letter-spacing: 0.5px;
    transition: background 0.15s;
}
.tab-active   { background: #141720; color: #c8d0e0; border: 1px solid #1e2535; border-bottom: 1px solid #141720; }
.tab-inactive { background: #0d0f14; color: #3a5060; border: 1px solid #111520; }
.tab-inactive:hover { color: #6888a8; }

/* Tab content */
.tab-pane {
    background: #141720;
    border: 1px solid #1e2535;
    border-radius: 0 6px 6px 6px;
    padding: 12px;
}

/* Log textarea */
.log-area {
    width: 100%;
    height: 460px;
    background: #080a0e;
    color: #60c878;
    border: 1px solid #1a2535;
    border-radius: 4px;
    font-family: 'Share Tech Mono', 'Courier New', monospace;
    font-size: 11px;
    padding: 8px;
    resize: vertical;
    line-height: 1.5;
}
.logctl { margin-top: 6px; display: flex; align-items: center; gap: 12px; }
.logctl a { color: #3a5878; text-decoration: none; font-size: 11px; }
.logctl a:hover { color: #6090b0; }
.logctl span { color: #1e3040; font-size: 10px; }

/* Dashboard table */
.dash-wrap { overflow-x: auto; }
.dash-table { width: 100%; border-collapse: collapse; font-size: 11px; }
.dash-table th {
    background: #0d1018;
    color: #3a5878;
    font-weight: 600;
    padding: 6px 8px;
    border-bottom: 1px solid #1a2535;
    text-align: left;
    white-space: nowrap;
    letter-spacing: 0.5px;
    font-size: 10px;
    text-transform: uppercase;
}
.dash-table .dc { padding: 5px 7px; border-bottom: 1px solid #0f1520; vertical-align: middle; }
.dash-table .dn { white-space: nowrap; }
.vrow:hover td { background: #111828 !important; }
.vname { color: #c0c8d8; font-weight: 600; font-size: 12px; }
.vuser { color: #2a4060; font-size: 10px; margin-top: 1px; font-family: 'Share Tech Mono', monospace; }
.res-row { display: flex; align-items: center; margin: 1px 0; }
.ri { font-size: 9px; font-weight: 700; width: 10px; margin-right: 4px; text-align: center; }
.ri-w { color: #b8903a; } .ri-s { color: #908070; } .ri-i { color: #6080a8; }
.rbar { flex: 1; height: 4px; background: #0a0c10; border-radius: 2px; overflow: hidden; margin-right: 5px; min-width: 50px; }
.rbfill { height: 100%; border-radius: 2px; }
.rbw { background: #c89030; } .rbs { background: #806050; } .rbi { background: #4870a0; } .rbp { background: #406040; }
.rv { font-size: 10px; color: #4a6080; min-width: 40px; text-align: right; font-family: 'Share Tech Mono', monospace; }
.blt { border-collapse: collapse; }
.bl  { color: #2a3848; font-size: 9px; padding: 0 2px 1px 0; text-transform: uppercase; }
.blv { color: #9098a8; font-size: 11px; padding: 0 6px 1px 0; text-align: right; font-family: 'Share Tech Mono', monospace; }
.badge { display: inline-block; font-size: 9px; font-weight: 700; padding: 1px 6px; border-radius: 3px; margin: 1px; letter-spacing: 0.5px; }
.bdg-build  { background: #1a3010; color: #60b040; border: 1px solid #2a5020; }
.bdg-rec    { background: #180d30; color: #7050c0; border: 1px solid #2a1850; }
.bdg-atk    { background: #2a1000; color: #e08020; border: 1px solid #6a3000; }
.dash-refresh { color: #2a4060; font-size: 10px; font-family: 'Share Tech Mono', monospace; }

/* Profile badges */
.prof-off     { color: #c06060; }
.prof-deff    { color: #6080c0; }
.prof-offdeff { color: #70a070; }
.prof-eco     { color: #c0a030; }

/* Scrollbar styling (webkit) */
::-webkit-scrollbar { width: 6px; height: 6px; }
::-webkit-scrollbar-track { background: #0d0f14; }
::-webkit-scrollbar-thumb { background: #1e2535; border-radius: 3px; }
::-webkit-scrollbar-thumb:hover { background: #2a3548; }
</style>
</head>
<body>

<!-- Top Bar -->
<div class="topbar">
    <div class="topbar-title">&#9670; Superbot <span>/ TWLan NPC Controller</span></div>
    <div class="topbar-right">
        <?php if (!$curl_ok): ?>
        <span style="color:#c07020;font-size:10px;background:#1a1000;border:1px solid #4a3000;padding:3px 8px;border-radius:3px">
            &#9888; Enable php_curl.dll in php.ini for game session support
        </span>
        <?php endif; ?>
        <span style="color:<?php echo $db_ok?'#40d060':'#d04040'; ?>;font-size:10px;background:<?php echo $db_ok?'#001a08':'#1a0000'; ?>;border:1px solid <?php echo $db_ok?'#005020':'#500000'; ?>;padding:3px 8px;border-radius:3px">
            <?php echo $db_ok ? '&#9679; DB Connected' : '&#9679; DB Error'; ?>
        </span>
    </div>
</div>

<div class="layout-wrap">

<!-- SIDEBAR -->
<div class="sidebar">

    <!-- Status + Control -->
    <div class="panel">
        <div class="panel-title">Bot Control</div>
        <div class="status-row">
            <span class="sdot <?php echo $bot_active?'dg':'dy'; ?>" id="dot_bot"></span>
            <span id="bot_status_text">Bot <?php echo $bot_active?'<strong>RUNNING</strong>':'Stopped'; ?></span>
        </div>
        <div id="next_tick"></div>
        <div style="margin-top:10px" id="botact">
            <?php if ($bot_active): ?>
            <a href="javascript:stop_bot()">&#9646;&#9646; Stop Bot</a>
            <?php else: ?>
            <a href="javascript:start_bot()">&#9654; Start Bot</a>
            <?php endif; ?>
        </div>
    </div>

    <?php if (!$curl_ok): ?>
    <div class="warn-bar">
        <strong>Session mode limited</strong><br>
        Enable <code>php_curl.dll</code> in php.ini for real HTTP session support.<br>
        The bot will still work via direct DB processing but game events may be delayed.
    </div>
    <?php else: ?>
    <div class="info-bar" style="font-size:10px;padding:6px 10px">
        &#9679; curl active &mdash; bot will maintain real game sessions automatically
    </div>
    <?php endif; ?>

    <!-- NPC Accounts -->
    <div class="panel">
        <div class="panel-title">NPC Accounts</div>
        <div class="hint" style="margin-bottom:6px">Check a user to include them. Choose their army profile.</div>
        <div class="user-list" style="max-height:200px">
            <?php
            $prof_opts = array('offdeff'=>'Balanced','off'=>'Offensive','deff'=>'Defensive');
            for ($i=0;$i<count($users);$i++):
                $u=$users[$i];
                $chk = is_sel($u['id'],$selected_ids);
                // Read per-user saved profile from bot_villages or fall back to global
                $u_profile = $profile; // default
                if ($db_ok) {
                    $pq = mysql_query("SELECT type FROM bot_villages WHERE uid='" . $u['id'] . "' LIMIT 1", $conn);
                    if ($pq && mysql_num_rows($pq) > 0) {
                        $pr = mysql_fetch_assoc($pq);
                        if ($pr['type'] != '') $u_profile = $pr['type'];
                    }
                }
            ?>
            <div class="user-item" style="flex-wrap:wrap;gap:4px;padding:5px 4px;border-bottom:1px solid #0f1520">
                <input type="checkbox" name="bot_userids[]" id="uid_<?php echo $u['id']; ?>"
                       value="<?php echo $u['id']; ?>"<?php if ($chk) echo ' checked="checked"'; ?>>
                <label for="uid_<?php echo $u['id']; ?>" style="flex:1;min-width:60px">
                    <strong style="color:<?php echo $chk?'#c0c8d8':'#5a6878'; ?>"><?php echo htmlspecialchars($u['username']); ?></strong>
                    <span class="vcnt"><?php echo $u['vcount']; ?>v</span>
                </label>
                <select name="user_profile[<?php echo $u['id']; ?>]"
                        id="prof_<?php echo $u['id']; ?>"
                        onchange="save_user_profile(<?php echo $u['id']; ?>, this.value)"
                        style="width:90px;font-size:10px;padding:2px 3px;background:#0d0f14;color:#8898b0;border:1px solid #1e2a40;border-radius:3px">
                    <?php
                    $pk2 = array_keys($prof_opts);
                    for ($pi=0;$pi<count($pk2);$pi++) {
                        $pv=$pk2[$pi];
                        $ps=($pv==$u_profile)?' selected="selected"':'';
                        echo '<option value="'.$pv.'"'.$ps.'>'.$prof_opts[$pv].'</option>';
                    }
                    ?>
                </select>
            </div>
            <?php endfor; ?>
        </div>
        <div class="hint" style="margin-top:5px">Profile sets build order <em>and</em> target army composition.</div>

        <label class="lbl" style="margin-top:8px">World speed</label>
        <input type="text" id="inp_world_speed" value="<?php echo $world_speed; ?>">
        <input type="hidden" id="sel_profile" value="offdeff">
    </div>

    <!-- Actions -->
    <div class="panel">
        <div class="panel-title">Actions</div>
        <div class="cbrow"><input type="checkbox" id="cb_build"    value="1"<?php if ($do_build)    echo ' checked="checked"'; ?>><label for="cb_build">&#127959; Build</label></div>
        <div class="cbrow"><input type="checkbox" id="cb_research" value="1"<?php if ($do_research) echo ' checked="checked"'; ?>><label for="cb_research">&#128300; Research units</label></div>
        <div class="cbrow"><input type="checkbox" id="cb_recruit"  value="1"<?php if ($do_recruit)  echo ' checked="checked"'; ?>><label for="cb_recruit">&#9876; Recruit troops</label></div>
        <div class="cbrow"><input type="checkbox" id="cb_attack"   value="1"<?php if ($do_attack)   echo ' checked="checked"'; ?>><label for="cb_attack">&#9876; Attack players</label></div>
        <div class="hint" style="margin-top:6px;border-top:1px solid #1a2030;padding-top:6px">
            Attack: &gt;=<?php echo $ATTACK_CONFIG['min_points']; ?>pts &bull;
            brk&gt;=<?php echo $ATTACK_CONFIG['min_barracks']; ?> &bull;
            &gt;=<?php echo $ATTACK_CONFIG['min_troops_home']; ?> home &bull;
            <?php echo (int)($ATTACK_CONFIG['send_fraction']*100); ?>% sent &bull;
            <?php echo $ATTACK_CONFIG['cooldown_seconds']; ?>s cooldown
        </div>
    </div>

    <!-- Timing -->
    <div class="panel">
        <div class="panel-title">Timing</div>
        <label class="lbl">Tick every (seconds, min 5)</label>
        <input type="text" id="inp_tick_interval" value="<?php echo $tick_interval; ?>">
        <div class="hint">How often browser calls bot.php</div>

        <label class="lbl" style="margin-top:8px">Action fires every N ticks</label>
        <table class="timing-grid">
            <tr>
                <td>Build</td>
                <td><input type="text" id="inp_build_every" value="<?php echo $build_every; ?>"></td>
                <td>Research</td>
                <td><input type="text" id="inp_research_every" value="<?php echo $research_every; ?>"></td>
            </tr>
            <tr>
                <td>Recruit</td>
                <td><input type="text" id="inp_recruit_every" value="<?php echo $recruit_every; ?>"></td>
                <td>Attack</td>
                <td><input type="text" id="inp_attack_every" value="<?php echo $attack_every; ?>"></td>
            </tr>
        </table>
        <div class="hint" style="margin-top:4px">E.g. tick=10s, attack_every=6 &rarr; attack attempt every 60s</div>
    </div>

    <!-- Save + Bottom button -->
    <div style="margin-bottom:10px">
        <button class="btn btn-save" onclick="save_settings()">&#128190; Save Settings</button>
        <span id="save_status"></span>
        <br>
        <input type="button" id="bottom_bot_btn"
               class="btn <?php echo $bot_active?'btn-stop':'btn-start'; ?>"
               value="<?php echo $bot_active?'Stop Bot':'\u25b6 Start Bot'; ?>"
               onclick="<?php echo $bot_active?'stop_bot()':'start_bot()'; ?>">
        <button class="btn btn-clear" onclick="clear_sessions()" title="Delete stored session cookies — bot will re-login on next tick">&#8635; Reset Sessions</button>
    </div>

</div><!-- /sidebar -->

<!-- MAIN COLUMN -->
<div class="maincol">

<?php if (!$db_ok): ?>
<div class="err-bar">
    <strong>&#9888; Cannot connect to database.</strong>
    Check <code>$DB_HOST</code>, <code>$DB_USER</code>, <code>$DB_PASS</code>, <code>$DB_NAME</code> in <code>bot_core.inc</code>.
    Error: <?php echo htmlspecialchars(mysql_error()); ?>
</div>
<?php endif; ?>

    <!-- Tab bar -->
    <div class="tab-bar">
        <a href="javascript:show_tab('dashboard')" id="tab_dashboard" class="tab tab-inactive">&#9776; Dashboard</a>
        <a href="javascript:show_tab('browser')"   id="tab_browser"   class="tab tab-active">&#9654; Bot Log</a>
    </div>

    <!-- Dashboard pane -->
    <div id="pane_dashboard" class="tab-pane" style="display:none">
        <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:10px">
            <span class="hint">Auto-refreshes every 15s &bull; <a href="javascript:reload_dashboard()" style="color:#3a6090;text-decoration:none">&#8635; Refresh now</a></span>
            <span id="dash_updated" class="dash-refresh"></span>
        </div>
        <div class="dash-wrap">
            <table class="dash-table" id="dash_table">
                <thead>
                    <tr>
                        <th>Village / Owner</th>
                        <th>Pts</th>
                        <th>Profile</th>
                        <th>Resources</th>
                        <th>Prod/h</th>
                        <th>Pop</th>
                        <th>Buildings</th>
                        <th>Army %</th>
                        <th>Status</th>
                    </tr>
                </thead>
                <tbody id="dash_body">
                    <tr><td colspan="9" style="color:#2a3848;padding:18px;text-align:center;font-style:italic">Click Dashboard tab to load...</td></tr>
                </tbody>
            </table>
        </div>
    </div>

    <!-- Bot log pane -->
    <div id="pane_browser" class="tab-pane">
        <form name="logform">
            <textarea name="log" readonly="readonly" class="log-area"></textarea>
        </form>
        <div class="logctl">
            <a href="javascript:clean_log()">&#9003; Clear log</a>
            <span>Browser AJAX log &mdash; auto-clears after 300 lines</span>
        </div>
    </div>

</div><!-- /maincol -->
</div><!-- /layout-wrap -->

<script type="text/javascript">
// Save per-user profile directly to bot_villages table via AJAX
function save_user_profile(uid, profile) {
    ajax_post('bot_action.php', 'action=save_profile&uid=' + uid + '&profile=' + encodeURIComponent(profile), function(resp) {
        var el = document.getElementById('save_status');
        if (resp.indexOf('ok') == 0) {
            if (el) { el.innerHTML = '&#10003; Profile saved'; setTimeout(function(){ el.innerHTML = ''; }, 1500); }
        }
    });
}

// Init tab state
document.getElementById('tab_browser').className   = 'tab tab-active';
document.getElementById('tab_dashboard').className = 'tab tab-inactive';

<?php if ($bot_active): ?>
bot_active = true;
schedule_next_tick();
<?php endif; ?>
</script>

</body></html>
<?php if ($db_ok) mysql_close($conn); ?>
