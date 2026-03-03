<?php
/*
 * bot_dashboard.php — Superbot Live Dashboard
 * Called via AJAX from the dashboard tab. Returns HTML rows for all bot villages.
 * PHP 4.4.4 compatible.
 */

require_once('bot_core.inc');

$config_file = dirname(__FILE__) . DIRECTORY_SEPARATOR . 'superbot_config.dat';
$cfg = array('bot_userids' => '');
if (file_exists($config_file)) {
    $raw = file_get_contents($config_file);
    $data = @unserialize($raw);
    if (is_array($data)) $cfg = $data;
}

$uid_list = array();
$raw_ids = isset($cfg['bot_userids']) ? $cfg['bot_userids'] : '';
if ($raw_ids != '') {
    $parts = explode(',', $raw_ids);
    for ($i = 0; $i < count($parts); $i++) {
        $id = intval(trim($parts[$i]));
        if ($id > 0) $uid_list[] = $id;
    }
}

if (count($uid_list) == 0) {
    echo '<tr><td colspan="12" style="color:#5a6878;padding:12px">No bot users selected.</td></tr>';
    exit;
}

$conn = @mysql_connect($DB_HOST, $DB_USER, $DB_PASS);
if (!$conn || !@mysql_select_db($DB_NAME, $conn)) {
    echo '<tr><td colspan="12" style="color:#e08080;padding:12px">DB error: ' . htmlspecialchars(mysql_error()) . '</td></tr>';
    exit;
}

$now = time();
$id_str = implode(',', $uid_list);

$vq = mysql_query(
    "SELECT v.id, v.name, v.x, v.y, v.userid, u.username," .
    " v.r_wood, v.r_stone, v.r_iron, v.r_bh AS pop," .
    " v.`" . $COL['b_wood']    . "` AS wood_lv," .
    " v.`" . $COL['b_stone']   . "` AS stone_lv," .
    " v.`" . $COL['b_iron']    . "` AS iron_lv," .
    " v.`" . $COL['b_farm']    . "` AS farm_lv," .
    " v.`" . $COL['b_storage'] . "` AS stor_lv," .
    " v.`" . $COL['b_barracks']. "` AS brk_lv," .
    " v.`" . $COL['b_smith']   . "` AS smi_lv," .
    " v.`" . $COL['b_stable']  . "` AS stb_lv," .
    " v.`" . $COL['b_wall']    . "` AS wall_lv," .
    " v.`" . $COL['b_main']    . "` AS main_lv," .
    " v.`" . $COL['points']    . "` AS pts," .
    " v.last_prod_aktu AS last_ts," .
    " bv.type AS profile," .
    " (SELECT COUNT(*) FROM build b WHERE b.villageid=v.id AND b.end_time > " . $now . ") AS building," .
    " (SELECT COUNT(*) FROM recruit r WHERE r.villageid=v.id AND r.time_finished > " . $now . ") AS recruiting" .
    " FROM villages v" .
    " JOIN users u ON u.id = v.userid" .
    " LEFT JOIN bot_villages bv ON bv.vid = v.id AND bv.uid = v.userid" .
    " WHERE v.userid IN (" . $id_str . ")" .
    " ORDER BY u.username ASC, pts DESC",
    $conn
);

if (!$vq || mysql_num_rows($vq) == 0) {
    echo '<tr><td colspan="12" style="color:#5a6878;padding:12px">No villages found.</td></tr>';
    mysql_close($conn);
    exit;
}

// Also get home troops for each village
$troop_map = array();
$all_vids_q = mysql_query("SELECT villages_from_id AS vid, unit_spear+unit_sword+unit_axe+unit_spy+unit_light+unit_heavy+unit_ram+unit_catapult+unit_archer+unit_marcher AS total FROM unit_place WHERE villages_from_id=villages_to_id", $conn);
if ($all_vids_q) {
    while ($tr = mysql_fetch_assoc($all_vids_q)) {
        $troop_map[intval($tr['vid'])] = intval($tr['total']);
    }
}

while ($v = mysql_fetch_assoc($vq)) {
    $vid    = intval($v['id']);
    $stor   = superbot_storage_cap(intval($v['stor_lv']));
    $farm_c = superbot_farm_cap(intval($v['farm_lv']));
    $pop    = intval($v['pop']);
    $pts    = intval($v['pts']);
    $prof   = (isset($v['profile']) && $v['profile'] != '') ? $v['profile'] : 'offdeff';
    $troops = isset($troop_map[$vid]) ? $troop_map[$vid] : 0;

    // Resource percentages for bar display
    $wood  = (int) round($v['r_wood']);
    $stone = (int) round($v['r_stone']);
    $iron  = (int) round($v['r_iron']);
    $wood_pct  = $stor > 0 ? min(100, (int)(($wood  / $stor) * 100)) : 0;
    $stone_pct = $stor > 0 ? min(100, (int)(($stone / $stor) * 100)) : 0;
    $iron_pct  = $stor > 0 ? min(100, (int)(($iron  / $stor) * 100)) : 0;
    $pop_pct   = $farm_c > 0 ? min(100, (int)(($pop / $farm_c) * 100)) : 0;

    // Prod rates
    $w_rate = superbot_prod_rate(intval($v['wood_lv']));
    $s_rate = superbot_prod_rate(intval($v['stone_lv']));
    $i_rate = superbot_prod_rate(intval($v['iron_lv']));

    // Status badges — build, train, and outgoing attacks
    $badges = '';
    if ($v['building'] > 0)   $badges .= '<span class="badge bdg-build">BUILD</span>';
    if ($v['recruiting'] > 0) $badges .= '<span class="badge bdg-rec">TRAIN</span>';

    // Outgoing attacks: query movements for this village
    $aq = mysql_query(
        "SELECT m.to_village, m.end_time, mv.name AS tname, mv.x AS tx, mv.y AS ty" .
        " FROM movements m" .
        " LEFT JOIN villages mv ON mv.id = m.to_village" .
        " WHERE m.from_village='" . $vid . "' AND m.type='attack' AND m.end_time > " . $now .
        " ORDER BY m.end_time ASC LIMIT 3",
        $conn
    );
    if ($aq && mysql_num_rows($aq) > 0) {
        while ($ar = mysql_fetch_assoc($aq)) {
            $eta_s   = max(0, intval($ar['end_time']) - $now);
            $eta_str = $eta_s >= 60 ? ((int)floor($eta_s/60)) . 'm' : $eta_s . 's';
            $tname   = $ar['tname'] ? htmlspecialchars(substr($ar['tname'],0,12)) : $ar['to_village'];
            $badges .= '<span class="badge bdg-atk" title="ETA ' . $eta_str . '">' .
                       '&#x2192;' . $tname . ' ' . $eta_str . '</span>';
        }
    }

    // Profile colour and label
    $prof_info = array(
        'off'     => array('col' => '#d06060', 'bg' => '#2a0808', 'lbl' => 'NUKE'),
        'deff'    => array('col' => '#6090d0', 'bg' => '#08102a', 'lbl' => 'WALL'),
        'offdeff' => array('col' => '#70c080', 'bg' => '#081a10', 'lbl' => 'MIX'),
        'eco'     => array('col' => '#c0a030', 'bg' => '#1a1400', 'lbl' => 'ECO')
    );
    $pinfo = isset($prof_info[$prof]) ? $prof_info[$prof] : array('col'=>'#888','bg'=>'#111','lbl'=>strtoupper($prof));
    $pcol = $pinfo['col']; $pbg = $pinfo['bg']; $plbl = $pinfo['lbl'];

    // Row colour alternates by user
    echo '<tr class="vrow" id="vrow_' . $vid . '">';

    // User / village name
    echo '<td class="dc">';
    echo '<div class="vname">' . htmlspecialchars($v['name']) . '</div>';
    echo '<div class="vuser">' . htmlspecialchars($v['username']) . ' &bull; ' . $v['x'] . '|' . $v['y'] . '</div>';
    echo '</td>';

    // Points
    echo '<td class="dc dn">' . number_format($pts) . '</td>';

    // Profile
    echo '<td class="dc"><span style="color:' . $pcol . ';background:' . $pbg . ';font-size:9px;font-weight:700;padding:2px 6px;border-radius:3px;border:1px solid ' . $pcol . ';letter-spacing:1px">' . $plbl . '</span></td>';

    // Resources with mini bars
    echo '<td class="dc res-cell">';
    echo '<div class="res-row"><span class="ri ri-w">W</span><div class="rbar"><div class="rbfill rbw" style="width:' . $wood_pct . '%"></div></div><span class="rv">' . number_format($wood) . '</span></div>';
    echo '<div class="res-row"><span class="ri ri-s">S</span><div class="rbar"><div class="rbfill rbs" style="width:' . $stone_pct . '%"></div></div><span class="rv">' . number_format($stone) . '</span></div>';
    echo '<div class="res-row"><span class="ri ri-i">I</span><div class="rbar"><div class="rbfill rbi" style="width:' . $iron_pct . '%"></div></div><span class="rv">' . number_format($iron) . '</span></div>';
    echo '<div style="font-size:10px;color:#4a6080;margin-top:2px">cap: ' . number_format($stor) . '</div>';
    echo '</td>';

    // Production rates
    echo '<td class="dc dn" style="font-size:11px;color:#6888a8">';
    echo '<div>&#127795;' . $w_rate . '/h</div>';
    echo '<div>&#127762;' . $s_rate . '/h</div>';
    echo '<div>&#9875;' . $i_rate . '/h</div>';
    echo '</td>';

    // Population
    echo '<td class="dc dn">';
    echo '<div class="rbar" style="width:60px;display:inline-block;vertical-align:middle"><div class="rbfill rbp" style="width:' . $pop_pct . '%"></div></div>';
    echo ' <span style="font-size:11px">' . $pop . '/' . $farm_c . '</span>';
    echo '</td>';

    // Key buildings
    echo '<td class="dc dn" style="font-size:11px">';
    echo '<table class="blt"><tr>';
    echo '<td class="bl">HQ</td><td class="blv">' . $v['main_lv'] . '</td>';
    echo '<td class="bl">BRK</td><td class="blv">' . $v['brk_lv'] . '</td>';
    echo '<td class="bl">SMI</td><td class="blv">' . $v['smi_lv'] . '</td>';
    echo '</tr><tr>';
    echo '<td class="bl">STB</td><td class="blv">' . $v['stb_lv'] . '</td>';
    echo '<td class="bl">WAL</td><td class="blv">' . $v['wall_lv'] . '</td>';
    echo '<td class="bl">STO</td><td class="blv">' . $v['stor_lv'] . '</td>';
    echo '</tr></table>';
    echo '</td>';

    // Army progress vs target
    $army_targets = array(
        'off'     => array('unit_axe'=>6500,'unit_light'=>2800,'unit_ram'=>300,'unit_spy'=>100),
        'deff'    => array('unit_spear'=>8000,'unit_sword'=>8000,'unit_archer'=>1500,'unit_spy'=>500),
        'offdeff' => array('unit_spear'=>4000,'unit_sword'=>4000,'unit_axe'=>3000,'unit_light'=>1000,'unit_ram'=>150,'unit_spy'=>100)
    );
    $tgt = isset($army_targets[$prof]) ? $army_targets[$prof] : $army_targets['offdeff'];
    $tgt_total = 0;
    $atkeys = array_keys($tgt);
    for ($ai=0;$ai<count($atkeys);$ai++) $tgt_total += $tgt[$atkeys[$ai]];

    // Read per-unit counts for this village
    $uq2 = mysql_query(
        "SELECT unit_spear,unit_sword,unit_axe,unit_spy,unit_light,unit_ram,unit_archer,unit_marcher" .
        " FROM unit_place WHERE villages_from_id='" . $vid . "' AND villages_to_id='" . $vid . "' LIMIT 1",
        $conn
    );
    $army_pct = 0; $have_total = 0;
    if ($uq2 && mysql_num_rows($uq2) > 0) {
        $ur2 = mysql_fetch_assoc($uq2);
        // Sum units against target keys
        for ($ai=0;$ai<count($atkeys);$ai++) {
            $ak = $atkeys[$ai];
            $have_total += isset($ur2[$ak]) ? intval($ur2[$ak]) : 0;
        }
        $army_pct = $tgt_total > 0 ? min(100,(int)(($have_total / (float)$tgt_total) * 100)) : 0;
    }
    $army_col = $army_pct >= 100 ? '#40c060' : ($army_pct >= 50 ? '#c0a030' : '#6888a8');

    echo '<td class="dc dn" style="font-size:11px;text-align:center">';
    echo '<div style="color:' . $army_col . ';font-weight:700;font-size:13px">' . $army_pct . '%</div>';
    echo '<div class="rbar" style="width:55px;margin:2px auto"><div class="rbfill" style="width:' . $army_pct . '%;background:' . $army_col . '"></div></div>';
    echo '<div style="font-size:9px;color:#3a5060;font-family:monospace">' . $have_total . '/' . $tgt_total . '</div>';
    echo '</td>';

    // Status
    echo '<td class="dc">' . ($badges != '' ? $badges : '<span style="color:#3a4050;font-size:10px">idle</span>') . '</td>';

    echo '</tr>' . "\n";
}

mysql_close($conn);
?>
