<?php
/*
 * bot_session.php — Superbot Persistent Session Manager
 *
 * Maintains real HTTP sessions for each bot user by:
 *   1. Logging in once per user and storing the session cookie to a file
 *   2. On every tick, GETting game.php?screen=overview with that cookie
 *      This triggers the game's own run_events.php / include_inc.php which
 *      processes build completions, recruit completions, resource production
 *      exactly as if the real player opened their browser.
 *   3. Re-logging in automatically if a session expires (HTTP 302 to login page)
 *
 * Cookie files are stored in the same directory as this script.
 * Format: superbot_cookie_<userid>.txt
 *
 * PHP 4.4.4 compatible. Requires curl extension (php_curl.dll in php.ini).
 */

define('GAME_BASE',     'http://localhost');
define('GAME_PASSWORD', '122407');
define('GAME_WORLD',    'welt1');
define('SESSION_DIR',   dirname(__FILE__));

/*
 * Get the path to the cookie file for a given user ID.
 */
function superbot_cookie_file($uid) {
    return SESSION_DIR . DIRECTORY_SEPARATOR . 'superbot_cookie_' . intval($uid) . '.txt';
}

/*
 * Perform a login for the given username and save the session cookie.
 * Returns true on success, false on failure.
 */
function superbot_do_login($username, $cookie_file) {
    if (!function_exists('curl_init')) return false;

    $login_url = GAME_BASE . '/index.php?action=login';
    $post_data = 'user='     . urlencode($username) .
                 '&password=' . urlencode(GAME_PASSWORD) .
                 '&server='   . urlencode(GAME_WORLD) .
                 '&clear=true';

    $ch = curl_init();
    curl_setopt($ch, CURLOPT_URL,            $login_url);
    curl_setopt($ch, CURLOPT_POST,           1);
    curl_setopt($ch, CURLOPT_POSTFIELDS,     $post_data);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
    curl_setopt($ch, CURLOPT_FOLLOWLOCATION, 1);
    curl_setopt($ch, CURLOPT_COOKIEJAR,      $cookie_file);
    curl_setopt($ch, CURLOPT_COOKIEFILE,     $cookie_file);
    curl_setopt($ch, CURLOPT_TIMEOUT,        15);
    curl_setopt($ch, CURLOPT_USERAGENT,      'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Superbot/2.0');

    $response  = curl_exec($ch);
    $http_code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    $final_url = curl_getinfo($ch, CURLINFO_EFFECTIVE_URL);
    curl_close($ch);

    if ($response === false || $http_code >= 500) return false;

    // Check we actually got into the game (not redirected back to login page)
    if (strpos($final_url, 'action=login') !== false) return false;
    if (strpos($response, 'game.php') === false && strpos($response, 'screen=overview') === false &&
        strpos($response, 'village=') === false && $http_code != 200) return false;

    return true;
}

/*
 * Hit the game overview page for a village using stored cookie.
 * This triggers game event processing (builds, recruits, production).
 * Returns: 'ok', 'expired' (need re-login), or 'error'
 */
function superbot_game_ping($vid, $cookie_file) {
    if (!function_exists('curl_init')) return 'no_curl';
    if (!file_exists($cookie_file))   return 'no_cookie';

    $url = GAME_BASE . '/game.php?village=' . intval($vid) . '&screen=overview';

    $ch = curl_init();
    curl_setopt($ch, CURLOPT_URL,            $url);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
    curl_setopt($ch, CURLOPT_FOLLOWLOCATION, 0); // Don't follow — we detect redirect manually
    curl_setopt($ch, CURLOPT_COOKIEJAR,      $cookie_file);
    curl_setopt($ch, CURLOPT_COOKIEFILE,     $cookie_file);
    curl_setopt($ch, CURLOPT_TIMEOUT,        15);
    curl_setopt($ch, CURLOPT_USERAGENT,      'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Superbot/2.0');
    curl_setopt($ch, CURLOPT_REFERER,        GAME_BASE . '/game.php?village=' . intval($vid) . '&screen=main');

    $response  = curl_exec($ch);
    $http_code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    $redirect  = curl_getinfo($ch, CURLINFO_REDIRECT_URL);
    curl_close($ch);

    if ($response === false) return 'error';

    // 302 to login page = session expired
    if ($http_code == 302 && strpos($redirect, 'login') !== false) return 'expired';
    if ($http_code == 302) return 'expired'; // Any redirect = not logged in
    if ($http_code == 200) return 'ok';

    return 'error';
}

/*
 * Main entry point: ensure all bot users have active sessions and ping overview.
 *
 * $uid_list  = array of integer user IDs
 * $conn      = active mysql connection
 *
 * Returns array: uid => array('status' => 'ok'|'login_ok'|'login_fail'|'no_curl', 'msg' => '...')
 */
function superbot_ping_all_users($uid_list, $conn) {
    $results = array();

    if (!function_exists('curl_init')) {
        for ($i = 0; $i < count($uid_list); $i++) {
            $results[$uid_list[$i]] = array('status' => 'no_curl', 'msg' => 'curl not available');
        }
        return $results;
    }

    for ($i = 0; $i < count($uid_list); $i++) {
        $uid = intval($uid_list[$i]);

        // Get username from DB
        $uq = mysql_query("SELECT username FROM users WHERE id='" . $uid . "' LIMIT 1", $conn);
        if (!$uq || mysql_num_rows($uq) == 0) {
            $results[$uid] = array('status' => 'error', 'msg' => 'user not found');
            continue;
        }
        $urow = mysql_fetch_assoc($uq);
        $username = $urow['username'];

        $cookie_file = superbot_cookie_file($uid);

        // Get this user's first village for the ping URL
        $vq = mysql_query("SELECT id FROM villages WHERE userid='" . $uid . "' ORDER BY id ASC LIMIT 1", $conn);
        $first_vid = 1;
        if ($vq && mysql_num_rows($vq) > 0) {
            $vrow = mysql_fetch_assoc($vq);
            $first_vid = intval($vrow['id']);
        }

        // Try to ping with existing cookie
        $ping = superbot_game_ping($first_vid, $cookie_file);

        if ($ping == 'ok') {
            $results[$uid] = array('status' => 'ok', 'msg' => $username . ': session alive');
            continue;
        }

        // Cookie missing or expired — log in fresh
        $ok = superbot_do_login($username, $cookie_file);
        if (!$ok) {
            $results[$uid] = array('status' => 'login_fail', 'msg' => $username . ': login FAILED');
            continue;
        }

        // Ping again after fresh login
        $ping2 = superbot_game_ping($first_vid, $cookie_file);
        if ($ping2 == 'ok') {
            $results[$uid] = array('status' => 'login_ok', 'msg' => $username . ': logged in OK');
        } else {
            $results[$uid] = array('status' => 'login_ok', 'msg' => $username . ': logged in (ping=' . $ping2 . ')');
        }
    }

    return $results;
}

/*
 * Force re-login for a specific user (call if you want to reset their session).
 */
function superbot_force_login($uid, $conn) {
    $uid = intval($uid);
    $uq  = mysql_query("SELECT username FROM users WHERE id='" . $uid . "' LIMIT 1", $conn);
    if (!$uq || mysql_num_rows($uq) == 0) return false;
    $urow = mysql_fetch_assoc($uq);
    $cookie_file = superbot_cookie_file($uid);
    return superbot_do_login($urow['username'], $cookie_file);
}

/*
 * Delete all stored session cookies (useful for "reset sessions" button in UI).
 */
function superbot_clear_sessions() {
    $files = glob(SESSION_DIR . DIRECTORY_SEPARATOR . 'superbot_cookie_*.txt');
    if (!$files) return 0;
    $count = 0;
    for ($i = 0; $i < count($files); $i++) {
        @unlink($files[$i]);
        $count++;
    }
    return $count;
}
?>
