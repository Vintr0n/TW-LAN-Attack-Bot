# TW-LAN-Attack-Bot / Superbot NPC Controller

After all these years I fancied playing Tribal Wars in an offline capacity and relooked at the attack bot I created some years ago. If you are still playing Tribal Wars offline using the TW LAN files and want AI-controlled NPC villages that actually behave like real players, this is the bot for you.

Tribal Wars LAN NPC Controller — simulates fully autonomous AI villages that build, research, recruit, attack, and support each other.

---

> **Disclaimer:** I did not create TW LAN. The original project can be found at https://twlan.github.io/  
> This should only be used in an **offline, local** capacity. Do not attempt to host this publicly.  
> The official game belongs to InnoGames™️ and can be accessed at http://tribalwars.net/  
> I assume no responsibility or liability for any errors or omissions in the content of TW LAN or this bot.  
> All scripts are provided on an "as is" basis with no guarantees of completeness, accuracy, or timeliness.

---

## What does this do?

Each NPC village will:

- **Build** — follows a min-max build plan tailored to its profile (offensive, defensive, or mixed)
- **Research** — unlocks units automatically in priority order (axe → spy → light cav → sword → ram etc.)
- **Recruit** — trains troops across barracks, stable, and workshop simultaneously with farm cap awareness
- **Attack** — sends attacks against the nearest player village with cooldown, strength-check, and noble support
- **Support** — when one of their own villages comes under attack, sibling villages send defensive reinforcements
- **Profiles** — each village can be set to `offdeff` (mixed), `off` (nuke), `deff` (wall)

The bot is controlled via a browser UI at `127.0.0.1/superbot/index.php` and runs as a background AJAX tick worker.

---

## Files

| File | Purpose |
|---|---|
| `index.php` | Browser UI — start/stop bot, configure settings, live dashboard |
| `bot.php` | AJAX tick worker — called every N seconds by the browser |
| `bot_core.inc` | All game logic — build plans, recruit, attack, support, research |
| `bot_action.php` | Handles start/stop/save/clear_sessions AJAX actions |
| `bot_dashboard.php` | Live village dashboard — resources, army %, buildings, status badges |
| `bot_session.php` | Maintains real HTTP sessions for each bot user via cURL |
| `script.js` | Browser-side tick timer, AJAX, UI controls |

---

## Prerequisites

### 1. TW LAN installed and running

Download and install TW LAN from https://twlan.github.io/  
Confirm you can access:
- `http://127.0.0.1/` — the game
- `http://127.0.0.1/admin` — the admin panel

### 2. PHP with cURL enabled

The session manager (`bot_session.php`) logs each bot user into the game via cURL so the game engine processes their events natively. Without cURL the bot falls back to direct DB processing only.

In your `php.ini` ensure this line is uncommented:
```
extension=php_curl.dll       ; Windows (XAMPP/WAMP)
extension=curl.so             ; Linux
```

Then restart Apache.

### 3. MySQL user with write access to the `lan` database

The default config in `bot_core.inc` connects as:
```php
$DB_HOST = 'localhost';
$DB_USER = 'root';
$DB_PASS = '';
$DB_NAME = 'lan';
```
Change these if your setup differs.

### 4. The `bot_villages` table

This is the **most important prerequisite** — the bot will not work without it. Run this SQL in phpMyAdmin or the MySQL CLI:

```sql
CREATE TABLE IF NOT EXISTS `bot_villages` (
    `uid`                INT          NOT NULL DEFAULT 0,
    `vid`                INT          NOT NULL DEFAULT 0,
    `type`               VARCHAR(10)  NOT NULL DEFAULT 'offdeff',
    `finish_builds`      TINYINT(1)   NOT NULL DEFAULT 0,
    `finish_techs`       TINYINT(1)   NOT NULL DEFAULT 0,
    `save_ticks`         INT          NOT NULL DEFAULT 0,
    `save_target`        VARCHAR(50)  NOT NULL DEFAULT '',
    `last_attack_time`   INT          NOT NULL DEFAULT 0,
    `attack_losses`      INT          NOT NULL DEFAULT 0,
    `last_attack_ratio`  FLOAT        NOT NULL DEFAULT 0,
    `target_vid`         INT          NOT NULL DEFAULT 0,
    `last_support_time`  INT          NOT NULL DEFAULT 0,
    `support_target_vid` INT          NOT NULL DEFAULT 0,
    PRIMARY KEY (`uid`, `vid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
```

> **Note:** The bot will attempt to `ALTER TABLE` to add missing columns automatically on its first tick, but creating the table upfront avoids errors.

The `type` column controls each village's behaviour:

| Value | Behaviour |
|---|---|
| `offdeff` | Balanced mixed village — LC rush, moderate wall, noble |
| `off` | Pure offensive nuke — axe + LC + rams, minimal wall |
| `deff` | Defensive wall village — spear + sword + high wall, noble |
| `eco` | Economy focus — max pits/storage, slower military build |

### 5. Player and AI accounts created in TW LAN

- Create one or more accounts that will be the **bot-controlled NPC players**
- Note their user IDs from the `users` table (or from the admin panel)
- These are the IDs you select in the bot UI

### 6. Cookie file write permission

`bot_session.php` stores session cookies as files named `superbot_cookie_<userid>.txt` in the same folder as the bot files. Ensure your web server user has write permission to that folder:

```bash
chmod 755 /path/to/superbot/
```

---

## Installation

1. Copy all bot files into a subfolder of your web root, e.g. `htdocs/superbot/`
2. Run the `bot_villages` SQL above in phpMyAdmin
3. Navigate to `http://127.0.0.1/superbot/index.php`
4. Tick the checkboxes next to the NPC user accounts you want the bot to control
5. Set the profile for each village using the dashboard dropdown (`offdeff` is a safe default)
6. Click **▶ Start Bot**

---

## Configuration

All settings are saved automatically when you click Start or Save Settings.

| Setting | Description | Default |
|---|---|---|
| Tick interval | Seconds between bot ticks | 10 |
| World speed | Match your TW LAN world speed setting | 1000 |
| Build every N ticks | How often the build action fires | 1 |
| Research every N ticks | How often research fires | 1 |
| Recruit every N ticks | How often the recruit action fires | 1 |
| Attack every N ticks | How often attacks are evaluated | 6 |

---

## Resetting troops (useful after testing)

If you need to wipe all troops and start the armies fresh while keeping buildings intact:

```sql
-- Zero all unit counts
UPDATE unit_place SET
    unit_spear=0, unit_sword=0, unit_axe=0, unit_archer=0,
    unit_spy=0, unit_light=0, unit_marcher=0, unit_heavy=0,
    unit_ram=0, unit_catapult=0, unit_snob=0, unit_knight=0, unit_miliz=0;

-- Clear training queues
DELETE FROM recruit;

-- Clear all movements
DELETE FROM movements;
DELETE FROM events WHERE event_type = 'movement';

-- Reset population counter (bot recalculates on next tick)
UPDATE villages SET r_bh = 0;

-- Reset bot state (cooldowns, targets, stall counters)
UPDATE bot_villages SET
    last_attack_time=0, attack_losses=0, last_attack_ratio=0,
    target_vid=0, last_support_time=0, support_target_vid=0,
    save_ticks=0, save_target='';
```

---

## Village profiles

Set via the bot dashboard. Each village's profile is stored in `bot_villages.type`.

**`offdeff` (default — balanced):** Follows the min-max LC rush. Builds economy alongside a stable + barracks, trains light cavalry as the primary unit, moderate wall, builds a noble at end-game.

**`off` (nuke):** Rushes light cav and axemen as fast as possible. Minimal wall. Sends the most aggressive attacks. Builds a noble at end-game.

**`deff` (wall):** Prioritises wall levels and spear/sword recruitment. Still follows the LC rush path for economy but invests more in wall. Trains a noble at end-game to allow village conquest.

**`eco`:** Maximises resource income first. Storage and pits are upgraded before military buildings. Good for a village that will feed resources or support another village.

---

## Troubleshooting

**Bot starts but nothing happens / "Bot is stopped" in the log**  
→ Check that `superbot_config.dat` was created and is writable. The folder needs write permission.

**"DB connect failed"**  
→ Check `$DB_HOST`, `$DB_USER`, `$DB_PASS`, `$DB_NAME` at the top of `bot_core.inc`.

**Sessions keep expiring every tick**  
→ cURL is working but the login is failing. Check that `GAME_PASSWORD` and `GAME_WORLD` in `bot_session.php` match your TW LAN install. The default password is `122407` and world is `welt1`.

**Troops exceed farm capacity**  
→ Ensure you are running the latest `bot_core.inc`. Earlier versions did not account for troops stationed at sibling villages via support movements, causing the farm cap check to under-count population.

**Villages building more than 1 nobleman**  
→ Ensure you are running the latest `bot_core.inc`. Earlier versions did not read `unit_snob` from `unit_place`, causing the "max 1 noble" guard to always pass.

**Farm shows a huge number like 59000/14904**  
→ This is a separate village whose troops were previously stationed here and the farm level hasn't caught up. The bot will upgrade the farm automatically. You can also run `UPDATE villages SET r_bh = 0;` to force a recalculation on next tick.

---

## Compatibility

- TW LAN (any recent version from https://twlan.github.io/)
- PHP 4.4.4+ (no modern syntax used — `var`, `mysql_*`, no arrow functions, no `const`/`let`)
- MySQL 5.x / MariaDB
- Apache (XAMPP / WAMP on Windows, or standard LAMP on Linux)
- Any browser with JavaScript enabled (IE8+ compatible)
