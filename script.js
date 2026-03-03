/*
 * script.js — Superbot TWLan NPC Controller
 * PHP 4.4.4 / old-browser compatible: var only, no const/let, no arrow functions.
 */

var bot_active      = false;
var tick_count      = 0;
var tick_timer      = null;
var countdown_end   = 0;
var countdown_timer = null;
var dash_timer      = null;

// -------------------------------------------------------
// AJAX helper
// -------------------------------------------------------
function ajax_post(url, params, callback) {
    var xhr;
    try {
        xhr = window.XMLHttpRequest
            ? new XMLHttpRequest()
            : new ActiveXObject("Microsoft.XMLHTTP");
    } catch(e) { alert('Browser does not support AJAX.'); return; }
    xhr.onreadystatechange = function() {
        if (xhr.readyState == 4 && xhr.status == 200 && callback) callback(xhr.responseText);
    };
    xhr.open('POST', url, true);
    xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
    xhr.send(params);
}

// -------------------------------------------------------
// Collect form settings into POST string
// -------------------------------------------------------
function collect_settings() {
    var ids   = [];
    var boxes = document.getElementsByName('bot_userids[]');
    for (var i = 0; i < boxes.length; i++) {
        if (boxes[i].checked) ids.push(boxes[i].value);
    }
    var p = 'bot_userids=' + encodeURIComponent(ids.join(','));
    p += '&profile='        + encodeURIComponent(document.getElementById('sel_profile').value);
    p += '&world_speed='    + encodeURIComponent(document.getElementById('inp_world_speed').value);
    p += '&tick_interval='  + encodeURIComponent(document.getElementById('inp_tick_interval').value);
    p += '&build_every='    + encodeURIComponent(document.getElementById('inp_build_every').value);
    p += '&research_every=' + encodeURIComponent(document.getElementById('inp_research_every').value);
    p += '&recruit_every='  + encodeURIComponent(document.getElementById('inp_recruit_every').value);
    p += '&attack_every='   + encodeURIComponent(document.getElementById('inp_attack_every').value);
    p += '&do_build='       + (document.getElementById('cb_build').checked    ? 1 : 0);
    p += '&do_research='    + (document.getElementById('cb_research').checked ? 1 : 0);
    p += '&do_recruit='     + (document.getElementById('cb_recruit').checked  ? 1 : 0);
    p += '&do_attack='      + (document.getElementById('cb_attack').checked   ? 1 : 0);
    return p;
}

function get_tick_ms() {
    var el = document.getElementById('inp_tick_interval');
    if (!el) return 10000;
    var v = parseInt(el.value, 10);
    if (isNaN(v) || v < 5) v = 5;
    return v * 1000;
}

// -------------------------------------------------------
// Save settings
// -------------------------------------------------------
function save_settings() {
    ajax_post('bot_action.php', 'action=save&' + collect_settings(), function(resp) {
        var el = document.getElementById('save_status');
        if (resp.indexOf('ok') == 0) {
            if (el) { el.innerHTML = '&#10003; Saved'; setTimeout(function(){ el.innerHTML = ''; }, 2000); }
            append_log('[' + now_str() + '] Settings saved.\n');
        } else {
            if (el) { el.innerHTML = '&#9888; Error!'; setTimeout(function(){ el.innerHTML = ''; }, 4000); }
            append_log('[' + now_str() + '] SAVE ERROR: ' + resp + '\n');
        }
    });
}

// -------------------------------------------------------
// Start bot
// -------------------------------------------------------
function start_bot() {
    ajax_post('bot_action.php', 'action=save&' + collect_settings(), function(resp) {
        if (resp.indexOf('ok') != 0) {
            append_log('[' + now_str() + '] SAVE ERROR: ' + resp + '\n');
            return;
        }
        ajax_post('bot_action.php', 'action=start', function(r2) {
            if (r2.indexOf('ok') != 0) {
                append_log('[' + now_str() + '] START ERROR: ' + r2 + '\n');
                return;
            }
            bot_active = true;
            set_bot_ui(true);
            schedule_next_tick();
            append_log('[' + now_str() + '] Bot started. Sessions will be established on first tick.\n');
        });
    });
}

// -------------------------------------------------------
// Stop bot
// -------------------------------------------------------
function stop_bot() {
    ajax_post('bot_action.php', 'action=stop', function(resp) {
        bot_active = false;
        if (tick_timer)      { clearTimeout(tick_timer);      tick_timer      = null; }
        if (countdown_timer) { clearInterval(countdown_timer); countdown_timer = null; }
        set_bot_ui(false);
        var nt = document.getElementById('next_tick');
        if (nt) nt.innerHTML = '';
        append_log('[' + now_str() + '] Bot stopped.\n');
    });
}

// -------------------------------------------------------
// Update all UI elements for bot state
// -------------------------------------------------------
function set_bot_ui(running) {
    var dot  = document.getElementById('dot_bot');
    var txt  = document.getElementById('bot_status_text');
    var act  = document.getElementById('botact');
    var bbtn = document.getElementById('bottom_bot_btn');

    if (running) {
        if (dot)  dot.className  = 'sdot dg';
        if (txt)  txt.innerHTML  = 'Bot <strong>RUNNING</strong>';
        if (act)  act.innerHTML  = '<a href="javascript:stop_bot()">\u258a\u258a Stop Bot</a>';
        if (bbtn) { bbtn.value = '\u258a\u258a Stop Bot'; bbtn.onclick = function(){ stop_bot(); }; bbtn.className = 'btn btn-stop'; }
    } else {
        if (dot)  dot.className  = 'sdot dy';
        if (txt)  txt.innerHTML  = 'Bot Stopped';
        if (act)  act.innerHTML  = '<a href="javascript:start_bot()">\u25b6 Start Bot</a>';
        if (bbtn) { bbtn.value = '\u25b6 Start Bot'; bbtn.onclick = function(){ start_bot(); }; bbtn.className = 'btn btn-start'; }
    }
}

// -------------------------------------------------------
// Bot tick
// -------------------------------------------------------
function do_tick() {
    if (!bot_active) return;
    ajax_post('bot.php', '', function(resp) {
        if (resp && resp.length > 0) append_log(resp);
        schedule_next_tick();
    });
}

function schedule_next_tick() {
    if (!bot_active) return;
    var ms = get_tick_ms();
    tick_timer = setTimeout("do_tick();", ms);
    update_countdown(ms);
}

// -------------------------------------------------------
// Countdown display
// -------------------------------------------------------
function update_countdown(ms) {
    countdown_end = new Date().getTime() + ms;
    if (countdown_timer) clearInterval(countdown_timer);
    countdown_timer = setInterval("tick_countdown();", 500);
}

function tick_countdown() {
    var el = document.getElementById('next_tick');
    if (!el) return;
    var remaining = Math.max(0, Math.round((countdown_end - new Date().getTime()) / 1000));
    el.innerHTML = remaining <= 0 ? 'ticking...' : 'next tick in ' + remaining + 's';
    if (remaining <= 0) { clearInterval(countdown_timer); countdown_timer = null; }
}

// -------------------------------------------------------
// Log helpers
// -------------------------------------------------------
function append_log(text) {
    var log = document.logform.log;
    tick_count++;
    if (tick_count >= 300) { log.value = ''; tick_count = 0; }
    log.value += text;
    log.scrollTop = log.scrollHeight;
}

function clean_log() {
    document.logform.log.value = '';
    tick_count = 0;
}

function now_str() {
    var d = new Date();
    var h = d.getHours();   if (h < 10) h = '0' + h;
    var m = d.getMinutes(); if (m < 10) m = '0' + m;
    var s = d.getSeconds(); if (s < 10) s = '0' + s;
    return h + ':' + m + ':' + s;
}

// -------------------------------------------------------
// Clear sessions button
// -------------------------------------------------------
function clear_sessions() {
    if (!confirm('Delete all stored session cookies? Bot will re-login on next tick.')) return;
    ajax_post('bot_action.php', 'action=clear_sessions', function(resp) {
        if (resp.indexOf('ok') == 0) {
            var n = resp.replace('ok:', '');
            append_log('[' + now_str() + '] Cleared ' + n + ' session cookie(s). Will re-login next tick.\n');
        }
    });
}

// -------------------------------------------------------
// Tab management
// -------------------------------------------------------
function show_tab(name) {
    var tabs = ['dashboard', 'browser'];
    for (var i = 0; i < tabs.length; i++) {
        var pane = document.getElementById('pane_' + tabs[i]);
        var tab  = document.getElementById('tab_' + tabs[i]);
        if (tabs[i] == name) {
            if (pane) pane.style.display = 'block';
            if (tab)  { tab.className = 'tab tab-active'; }
        } else {
            if (pane) pane.style.display = 'none';
            if (tab)  { tab.className = 'tab tab-inactive'; }
        }
    }
    if (name == 'dashboard') {
        reload_dashboard();
        if (!dash_timer) dash_timer = setInterval("reload_dashboard();", 15000);
    } else {
        if (dash_timer) { clearInterval(dash_timer); dash_timer = null; }
    }
}

function reload_dashboard() {
    var tbody = document.getElementById('dash_body');
    if (!tbody) return;
    ajax_post('bot_dashboard.php', '', function(resp) {
        tbody.innerHTML = resp;
        var el = document.getElementById('dash_updated');
        if (el) el.innerHTML = 'Updated ' + now_str();
    });
}
