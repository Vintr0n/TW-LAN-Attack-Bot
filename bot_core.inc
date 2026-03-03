<?php
/*
 * bot_inc.php
 * Superbot for TWLan - Core Bot Logic Library
 *
 * PHP 4.4.4 compatible (no json_encode, no [], no type hints, mysql_* only)
 *
 * Schema confirmed from live DB:
 *   villages:   id, userid, x, y, name, r_wood, r_stone, r_iron, r_bh,
 *               last_prod_aktu, points, main, barracks, stable, garage,
 *               smith, market, wood, stone, iron, storage, farm, hide, wall, snob,
 *               unit_spear_tec_level, unit_sword_tec_level, unit_axe_tec_level,
 *               unit_spy_tec_level, unit_light_tec_level, unit_heavy_tec_level,
 *               unit_ram_tec_level, unit_catapult_tec_level
 *               (tech IS in villages table — no separate tech table)
 *   unit_place: unit_spear, unit_sword, unit_axe, unit_spy, unit_light,
 *               unit_heavy, unit_ram, unit_catapult, unit_knight, unit_snob,
 *               unit_archer, unit_marcher, unit_miliz,
 *               villages_from_id, villages_to_id
 *   build:      id, building, villageid, end_time, build_time
 *   recruit:    id, unit, num_unit, num_finished, last_reload, time_finished,
 *               time_start, time_per_unit, building, villageid
 *   events:     id, event_time, event_type, event_id, user_id, villageid
 *   movements:  id, from_village, to_village, units, type, start_time, end_time,
 *               from_userid, to_userid, to_hidden, wood, stone, iron,
 *               send_from_village, send_from_user, send_to_user, send_to_village, die
 *               building
 *   bot_players:  username
 *   bot_villages: uid, vid, finish_builds, finish_techs, type (offdeff/off/deff)
 *   bot_builds:   vid, building
 *   bot_techs:    vid, tech
 */

/* =========================================================
   SECTION 1: DATABASE CONFIGURATION
   ========================================================= */

$DB_HOST = 'localhost';
$DB_USER = 'root';
$DB_PASS = '';
$DB_NAME = 'lan';

/* =========================================================
   SECTION 2: COLUMN MAP
   Maps internal keys to actual DB column names.
   Only change the VALUES if your install differs.
   ========================================================= */

$COL = array(
    // Resources (in villages table)
    'r_wood'     => 'r_wood',
    'r_stone'    => 'r_stone',
    'r_iron'     => 'r_iron',
    'r_pop'      => 'r_bh',              // population currently used
    'last_prod'  => 'last_prod_aktu',    // unix timestamp of last resource tick

    // Score
    'points'     => 'points',

    // Buildings (bare column names — no prefix in this install)
    'b_main'     => 'main',
    'b_barracks' => 'barracks',
    'b_smith'    => 'smith',
    'b_stable'   => 'stable',
    'b_garage'   => 'garage',
    'b_market'   => 'market',
    'b_wall'     => 'wall',
    'b_wood'     => 'wood',
    'b_stone'    => 'stone',
    'b_iron'     => 'iron',
    'b_farm'     => 'farm',
    'b_storage'  => 'storage',
    'b_hide'     => 'hide',
    'b_snob'     => 'snob',

    // Tech levels — stored IN villages table as unit_X_tec_level
    // Value is 1 when researched, 0 when not
    'tec_spear'    => 'unit_spear_tec_level',
    'tec_sword'    => 'unit_sword_tec_level',
    'tec_axe'      => 'unit_axe_tec_level',
    'tec_spy'      => 'unit_spy_tec_level',
    'tec_light'    => 'unit_light_tec_level',
    'tec_heavy'    => 'unit_heavy_tec_level',
    'tec_ram'      => 'unit_ram_tec_level',
    'tec_catapult' => 'unit_catapult_tec_level',
    'tec_archer'   => 'unit_archer_tec_level',
    'tec_marcher'  => 'unit_marcher_tec_level',

    // Unit columns in unit_place table
    'u_spear'    => 'unit_spear',
    'u_sword'    => 'unit_sword',
    'u_axe'      => 'unit_axe',
    'u_spy'      => 'unit_spy',
    'u_light'    => 'unit_light',
    'u_heavy'    => 'unit_heavy',
    'u_ram'      => 'unit_ram',
    'u_catapult' => 'unit_catapult',
    'u_knight'   => 'unit_knight',
    'u_archer'   => 'unit_archer',
    'u_marcher'  => 'unit_marcher',
    'u_snob'     => 'unit_snob',
    'u_archer'   => 'unit_archer',
    'u_marcher'  => 'unit_marcher',

    // All-time recruitment totals — game decrements on death; we increment on graduation
    'au_spear'    => 'all_unit_spear',
    'au_sword'    => 'all_unit_sword',
    'au_axe'      => 'all_unit_axe',
    'au_archer'   => 'all_unit_archer',
    'au_spy'      => 'all_unit_spy',
    'au_light'    => 'all_unit_light',
    'au_marcher'  => 'all_unit_marcher',
    'au_heavy'    => 'all_unit_heavy',
    'au_ram'      => 'all_unit_ram',
    'au_catapult' => 'all_unit_catapult'
);

/* =========================================================
   SECTION 3: BUILDING DEFINITIONS

   cost  = array(base_wood, base_stone, base_iron, growth_factor)
   Actual cost at level N = floor(base * growth_factor ^ (N-1))
   This is the formula the real game uses. Never use flat costs.

   max  = maximum building level
   req  = prerequisites: array(internal_building_key => min_level_required)
   ========================================================= */

$BUILDINGS = array(
    'b_main'     => array('cost' => array(90,    80,    70,    1.26),  'max' => 30, 'req' => array()),
    'b_barracks' => array('cost' => array(200,   170,   90,    1.26),  'max' => 25, 'req' => array('b_main' => 3)),
    'b_smith'    => array('cost' => array(220,   180,   240,   1.26),  'max' => 20, 'req' => array('b_main' => 5, 'b_barracks' => 1)),
    'b_stable'   => array('cost' => array(275,   240,   225,   1.26),  'max' => 20, 'req' => array('b_main' => 10, 'b_barracks' => 5, 'b_smith' => 5)),
    'b_garage'   => array('cost' => array(300,   240,   260,   1.26),  'max' => 15, 'req' => array('b_main' => 10, 'b_smith' => 10)),
    'b_market'   => array('cost' => array(100,   100,   100,   1.26),  'max' => 25, 'req' => array('b_main' => 3, 'b_storage' => 2)),
    'b_wall'     => array('cost' => array(50,    100,   20,    1.26),  'max' => 20, 'req' => array('b_barracks' => 1)),
    'b_wood'     => array('cost' => array(50,    60,    40,    1.25),  'max' => 30, 'req' => array()),
    'b_stone'    => array('cost' => array(65,    50,    40,    1.27),  'max' => 30, 'req' => array()),
    'b_iron'     => array('cost' => array(75,    65,    50,    1.28),  'max' => 30, 'req' => array()),
    'b_farm'     => array('cost' => array(45,    40,    30,    1.30),  'max' => 30, 'req' => array()),
    'b_storage'  => array('cost' => array(60,    50,    40,    1.265), 'max' => 30, 'req' => array()),
    'b_hide'     => array('cost' => array(50,    60,    50,    1.25),  'max' => 10, 'req' => array()),
    'b_snob'     => array('cost' => array(15000, 25000, 10000, 2.0),   'max' => 1,  'req' => array('b_main' => 20, 'b_smith' => 20, 'b_market' => 10))
);

/* =========================================================
   SECTION 4: UNIT DEFINITIONS

   cost     = array(wood, stone, iron) per single unit
   pop      = population slots consumed per unit
   bi       = internal building key that must be >= bmin
   bmin     = minimum level of that building
   smith    = minimum smithy level before research is possible
   tec_col  = key in $COL pointing to the unit_X_tec_level column in villages
   always   = true = no smith/research needed (spear fighter only)

   Tech tree:
     Spear    | Barracks 1 | Smith  0 | Always available (tec_level starts at 1)
     Sword    | Barracks 1 | Smith  1 | unit_sword_tec_level must = 1
     Axe      | Barracks 1 | Smith  2 | unit_axe_tec_level must = 1
     Scout    | Stable   1 | Smith  2 | unit_spy_tec_level must = 1
     Light    | Stable   1 | Smith  5 | unit_light_tec_level must = 1
     Heavy    | Stable  10 | Smith 15 | unit_heavy_tec_level must = 1
     Ram      | Garage   1 | Smith  3 | unit_ram_tec_level must = 1
     Catapult | Garage   1 | Smith  8 | unit_catapult_tec_level must = 1
   ========================================================= */

$UNITS = array(
    'u_spear' => array(
        'cost'    => array(50,  30,  10),
        'pop'     => 1,
        'bi'      => 'b_barracks', 'bmin' => 1,
        'smith'   => 0,
        'tec_col' => 'tec_spear',
        'always'  => true
    ),
    'u_sword' => array(
        'cost'    => array(30,  30,  70),
        'pop'     => 1,
        'bi'      => 'b_barracks', 'bmin' => 1,
        'smith'   => 1,
        'tec_col' => 'tec_sword',
        'always'  => false
    ),
    'u_axe' => array(
        'cost'    => array(60,  30,  40),
        'pop'     => 1,
        'bi'      => 'b_barracks', 'bmin' => 1,
        'smith'   => 2,
        'tec_col' => 'tec_axe',
        'always'  => false
    ),
    'u_spy' => array(
        'cost'    => array(50,  50,  20),
        'pop'     => 2,
        'bi'      => 'b_stable',   'bmin' => 1,
        'smith'   => 2,
        'tec_col' => 'tec_spy',
        'always'  => false
    ),
    'u_light' => array(
        'cost'    => array(125, 100, 250),
        'pop'     => 4,
        'bi'      => 'b_stable',   'bmin' => 1,
        'smith'   => 5,
        'tec_col' => 'tec_light',
        'always'  => false
    ),
    'u_heavy' => array(
        'cost'    => array(200, 150, 600),
        'pop'     => 6,
        'bi'      => 'b_stable',   'bmin' => 10,
        'smith'   => 15,
        'tec_col' => 'tec_heavy',
        'always'  => false
    ),
    'u_ram' => array(
        'cost'    => array(300, 200, 200),
        'pop'     => 5,
        'bi'      => 'b_garage',   'bmin' => 1,
        'smith'   => 3,
        'tec_col' => 'tec_ram',
        'always'  => false
    ),
    'u_catapult' => array(
        'cost'    => array(320, 400, 100),
        'pop'     => 8,
        'bi'      => 'b_garage',   'bmin' => 1,
        'smith'   => 8,
        'tec_col' => 'tec_catapult',
        'always'  => false
    ),
    'u_archer' => array(
        'cost'    => array(100, 30,  60),
        'pop'     => 1,
        'bi'      => 'b_barracks', 'bmin' => 1,
        'smith'   => 1,
        'tec_col' => 'tec_archer',
        'always'  => false
    ),
    'u_marcher' => array(
        'cost'    => array(250, 100, 150),
        'pop'     => 4,
        'bi'      => 'b_stable',   'bmin' => 1,
        'smith'   => 5,
        'tec_col' => 'tec_marcher',
        'always'  => false
    ),
    'u_snob' => array(
        'cost'    => array(28000, 30000, 25000),
        'pop'     => 100,
        'bi'      => 'b_snob',     'bmin' => 1,
        'smith'   => 20,
        'tec_col' => null,
        'always'  => false
    )
);

/* =========================================================
   SECTION 5: ATTACK CONFIGURATION
   All gates must pass before a village sends an attack.
   Tune these to control NPC aggressiveness.
   ========================================================= */

$ATTACK_CONFIG = array(
    'min_points'       => 500,   // Village must have at least this many points
    'min_barracks'     => 5,     // Barracks must be at least this level
    'min_farm_free'    => 50,    // Free pop slots that must remain after sending
    'min_troops_home'  => 100,   // Troops at home before any attack is sent
    'min_send'         => 50,    // Minimum total troops in the attack wave
    // One attack in-flight at a time is enforced in superbot_action_attack().
    'cooldown_seconds' => 3600,  // Minimum seconds between attacks from same village
    // Attack chance per tick per profile (0.0 - 1.0). Adds natural variation so
    // not every village attacks every time it qualifies. Mimics human behaviour.
    'chance_off'       => 0.70,  // 70% chance per qualifying tick for off villages
    'chance_offdeff'   => 0.50,  // 50% for mixed villages
    'chance_deff'      => 0.30,  // 30% for defensive villages
    // Travel time jitter in seconds — randomises arrival so all attacks don't
    // land simultaneously. Each attack gets +0 to jitter_max seconds added.
    'travel_jitter'    => 120,   // Up to 2 extra minutes of random travel time
    // Minimum ratio of (our wave's offensive power) / (target's defensive power)
    // required before attacking. 0.60 = need 60% of target's def strength.
    // Raise to 0.80 for more cautious bots; lower to 0.40 for aggressive.
    'min_strength_ratio' => 0.60
);

/* =========================================================
   SECTION 6: BUILD PLANS
   Ordered list of array(building_key, target_level).
   Bot walks from top, finds first unmet step whose prerequisites
   are satisfied, and builds that.
   ========================================================= */

function superbot_get_build_plan($profile) {

    // =========================================================
    // MIN-MAX LC RUSH — shared Phase 1 & 2 foundation
    // Phase 1: Basic economy + Barracks for early spears
    // Phase 2: HQ5 -> Smith1 -> Smith5 -> Stable1 -> Stable3
    //          -> research LC -> never stop recruiting LC
    // Phase 3: Scale economy, Barracks25, Stable20, HQ20
    // Phase 4: Smith20, Market10, Workshop1, Academy1 (noble)
    // Final:   All pits/farm/storage/wall to max (9800 pts target)
    // =========================================================

    // ECO: Maximise resource income first, LC rush slightly later.
    // Good for a village that will feed resources to an off village.
    // KEY RULE: storage/farm always 1-2 levels AHEAD of pits so production never overflows.
    if ($profile == 'eco') {
        return array(
            // --- Phase 1: Early economy — storage first, then pits ---
            array('b_main',2),
            array('b_storage',2), array('b_farm',2),
            array('b_wood',2),    array('b_stone',2),   array('b_iron',2),
            array('b_storage',3), array('b_farm',3),
            array('b_main',3),
            array('b_storage',4), array('b_farm',4),
            array('b_wood',4),    array('b_stone',4),   array('b_iron',4),
            array('b_storage',5), array('b_farm',5),
            array('b_wood',5),    array('b_stone',5),   array('b_iron',5),
            array('b_barracks',1),
            array('b_storage',6), array('b_farm',6),
            array('b_wood',7),    array('b_stone',7),   array('b_iron',7),

            // --- Phase 2: LC Rush path ---
            // Stable prereqs: HQ10 + Barracks5 + Smith5
            array('b_storage',7), array('b_farm',7),
            array('b_main',5),
            array('b_smith',1),   array('b_smith',5),
            array('b_main',10),   array('b_barracks',5),
            array('b_stable',1),  array('b_stable',3),
            // LC now researchable — bot research action handles it

            // --- Phase 3: Economy scaling + troop infrastructure ---
            array('b_storage',8), array('b_farm',8),
            array('b_wood',9),    array('b_stone',9),   array('b_iron',9),
            array('b_storage',9), array('b_farm',9),
            array('b_wood',10),   array('b_stone',10),  array('b_iron',10),
            array('b_smith',8),   array('b_stable',5),
            array('b_storage',11),array('b_farm',11),
            array('b_wood',12),   array('b_stone',12),  array('b_iron',12),
            array('b_smith',10),  array('b_stable',8),
            array('b_storage',13),array('b_farm',13),
            array('b_wood',14),   array('b_stone',14),  array('b_iron',14),
            array('b_barracks',10),array('b_stable',10),
            array('b_storage',15),array('b_farm',15),
            array('b_wood',16),   array('b_stone',16),  array('b_iron',16),
            array('b_barracks',15),array('b_stable',15),array('b_smith',15),
            array('b_storage',17),array('b_farm',17),
            array('b_wood',18),   array('b_stone',18),  array('b_iron',18),
            array('b_barracks',20),array('b_stable',20),
            array('b_storage',19),array('b_farm',19),
            array('b_wood',20),   array('b_stone',20),  array('b_iron',20),

            // --- Phase 4: Push to Noble ---
            array('b_storage',21),array('b_farm',21),
            array('b_main',20),
            array('b_smith',20),
            array('b_market',5),  array('b_market',10),
            array('b_garage',1),
            array('b_barracks',25),
            array('b_market',20),

            // --- Final: Max everything for 9800 pts ---
            array('b_storage',25),array('b_farm',25),
            array('b_wood',25),   array('b_stone',25),  array('b_iron',25),
            array('b_storage',28),array('b_farm',28),
            array('b_wood',28),   array('b_stone',28),  array('b_iron',28),
            array('b_storage',30),array('b_farm',30),
            array('b_wood',30),   array('b_stone',30),  array('b_iron',30),
            array('b_wall',10),   array('b_wall',20),
            array('b_snob',1)
        );
    }

    // DEFF: Prioritises wall and defensive troop infrastructure.
    // Still does the LC rush but invests more in wall + sword/spear research.
    // KEY RULE: storage/farm lead pits by 1-2 levels to prevent overflow waste.
    if ($profile == 'deff') {
        return array(
            // --- Phase 1: Storage first so early production is never wasted ---
            array('b_main',2),
            array('b_storage',2), array('b_farm',2),
            array('b_wood',2),    array('b_stone',2),   array('b_iron',2),
            array('b_main',3),
            array('b_barracks',1),array('b_wall',1),
            array('b_storage',3), array('b_farm',3),
            array('b_wood',4),    array('b_stone',4),   array('b_iron',4),
            array('b_storage',4), array('b_farm',4),
            array('b_wood',5),    array('b_stone',5),   array('b_iron',5),
            array('b_wall',3),    array('b_barracks',3),

            // --- Phase 2: LC Rush ---
            // Stable prereqs: HQ10 + Barracks5 + Smith5
            array('b_storage',5), array('b_farm',5),
            array('b_main',5),
            array('b_smith',1),   array('b_smith',5),
            array('b_main',10),   array('b_barracks',5),
            array('b_stable',1),  array('b_stable',3),

            // --- Phase 3: Scale with wall focus, storage always 2 ahead ---
            array('b_storage',8), array('b_farm',8),
            array('b_wood',7),    array('b_stone',7),   array('b_iron',7),
            array('b_wall',6),    array('b_barracks',6),
            array('b_storage',9), array('b_farm',9),
            array('b_wood',9),    array('b_stone',9),   array('b_iron',9),
            array('b_wall',8),    array('b_barracks',8),
            array('b_smith',8),
            array('b_storage',11),array('b_farm',11),
            array('b_wood',11),   array('b_stone',11),  array('b_iron',11),
            array('b_stable',8),  array('b_wall',10),
            array('b_storage',13),array('b_farm',13),
            array('b_wood',13),   array('b_stone',13),  array('b_iron',13),
            array('b_barracks',12),array('b_smith',10),
            array('b_storage',15),array('b_farm',15),
            array('b_wood',15),   array('b_stone',15),  array('b_iron',15),
            array('b_barracks',15),array('b_wall',12),
            array('b_stable',10), array('b_smith',12),
            array('b_storage',17),array('b_farm',17),
            array('b_wood',17),   array('b_stone',17),  array('b_iron',17),
            array('b_smith',15),  array('b_stable',15),
            array('b_storage',19),array('b_farm',19),
            array('b_wood',19),   array('b_stone',19),  array('b_iron',19),
            array('b_barracks',20),array('b_wall',15),
            array('b_stable',18),

            // --- Phase 4: Noble path ---
            array('b_storage',21),array('b_farm',21),
            array('b_wood',20),   array('b_stone',20),  array('b_iron',20),
            array('b_main',20),
            array('b_smith',20),
            array('b_market',5),  array('b_market',10),
            array('b_garage',1),
            array('b_barracks',25),
            array('b_market',20),

            // --- Final: Max for 9800 pts ---
            array('b_storage',25),array('b_farm',25),
            array('b_wood',24),   array('b_stone',24),  array('b_iron',24),
            array('b_storage',28),array('b_farm',28),
            array('b_wood',27),   array('b_stone',27),  array('b_iron',27),
            array('b_storage',30),array('b_farm',30),
            array('b_wood',30),   array('b_stone',30),  array('b_iron',30),
            array('b_wall',18),   array('b_wall',20),
            array('b_snob',1)
        );
    }

    // OFF: Pure offensive — rush LC as fast as possible, axemen second,
    // wall kept low, everything oriented around raiding output.
    // KEY RULE: storage/farm lead pits so production is never wasted on overflow.
    if ($profile == 'off') {
        return array(
            // --- Phase 1: Minimum to unlock Barracks, but storage first ---
            array('b_main',2),
            array('b_storage',2), array('b_farm',2),
            array('b_wood',2),    array('b_stone',2),   array('b_iron',2),
            array('b_main',3),
            array('b_barracks',1),
            array('b_storage',3), array('b_farm',3),
            array('b_wood',3),    array('b_stone',3),   array('b_iron',3),
            array('b_storage',4), array('b_farm',4),
            array('b_wood',4),    array('b_stone',4),   array('b_iron',4),

            // --- Phase 2: Fastest possible LC unlock ---
            // Prerequisites for Stable: HQ10 + Barracks5 + Smith5
            array('b_storage',5), array('b_farm',5),
            array('b_main',5),
            array('b_smith',1),   array('b_smith',5),
            array('b_main',10),   array('b_barracks',5),
            array('b_stable',1),  array('b_stable',3),
            // LC unlocked — research fires automatically

            // --- Phase 3: Axemen + scaling offence, storage stays ahead ---
            array('b_storage',7), array('b_farm',7),
            array('b_wood',6),    array('b_stone',6),   array('b_iron',6),
            array('b_smith',8),   array('b_stable',5),
            array('b_storage',9), array('b_farm',9),
            array('b_wood',8),    array('b_stone',8),   array('b_iron',8),
            array('b_barracks',8),array('b_stable',8),
            array('b_storage',11),array('b_farm',11),
            array('b_wood',10),   array('b_stone',10),  array('b_iron',10),
            array('b_smith',10),  array('b_stable',10),
            array('b_storage',13),array('b_farm',13),
            array('b_wood',12),   array('b_stone',12),  array('b_iron',12),
            array('b_barracks',12),array('b_smith',12),
            array('b_storage',15),array('b_farm',15),
            array('b_wood',14),   array('b_stone',14),  array('b_iron',14),
            array('b_barracks',15),array('b_stable',15),
            array('b_smith',15),
            array('b_storage',17),array('b_farm',17),
            array('b_wood',16),   array('b_stone',16),  array('b_iron',16),
            array('b_barracks',18),array('b_stable',18),
            array('b_storage',19),array('b_farm',19),
            array('b_wood',18),   array('b_stone',18),  array('b_iron',18),
            array('b_barracks',20),array('b_stable',20),

            // --- Phase 4: Noble path (wall minimal - off style) ---
            array('b_storage',21),array('b_farm',21),
            array('b_wood',20),   array('b_stone',20),  array('b_iron',20),
            array('b_main',20),
            array('b_smith',20),
            array('b_market',5),  array('b_market',10),
            array('b_garage',1),
            array('b_barracks',25),
            array('b_market',20),
            array('b_wall',5),   // minimal wall for off village

            // --- Final: Max pits + farm + storage for 9800 pts ---
            array('b_storage',25),array('b_farm',25),
            array('b_wood',24),   array('b_stone',24),  array('b_iron',24),
            array('b_storage',28),array('b_farm',28),
            array('b_wood',27),   array('b_stone',27),  array('b_iron',27),
            array('b_storage',30),array('b_farm',30),
            array('b_wood',30),   array('b_stone',30),  array('b_iron',30),
            array('b_snob',1)
        );
    }

    // DEFAULT — offdeff (Balanced Min-Max LC Rush)
    // This is the recommended profile for most NPC villages.
    // KEY RULE: storage/farm always built BEFORE reaching matching pit levels.
    // This ensures produced resources are never lost to overflow, and building
    // costs (especially for Smithy/Stable which are stone/iron heavy) can always
    // be banked. Wood stays most abundant so LC training never stalls.
    return array(
        // -------------------------------------------------------
        // PHASE 1: Scavenge & Farm Start
        // Storage/farm FIRST — guarantee no overflow from tick 1.
        // -------------------------------------------------------
        array('b_main',2),
        array('b_storage',2), array('b_farm',2),         // cap before first pit upgrade
        array('b_wood',2),    array('b_stone',2),   array('b_iron',2),
        array('b_main',3),
        array('b_storage',3), array('b_farm',3),
        array('b_barracks',1),
        array('b_storage',4), array('b_farm',4),
        array('b_wood',3),    array('b_stone',3),   array('b_iron',3),
        array('b_storage',5), array('b_farm',5),
        array('b_wood',4),    array('b_stone',4),   array('b_iron',4),

        // -------------------------------------------------------
        // PHASE 2: The LC Rush (Critical Path)
        // Prerequisites for Stable: HQ10 + Barracks5 + Smith5
        // Storage kept ahead so smith costs don't stall in an empty warehouse.
        // -------------------------------------------------------
        array('b_storage',6), array('b_farm',6),
        array('b_main',5),
        array('b_smith',1),
        array('b_smith',2),   array('b_smith',3),
        array('b_smith',4),   array('b_smith',5),
        array('b_main',10),           // HQ10 — required for Stable
        array('b_barracks',5),        // Barracks5 — required for Stable + unlocks axe
        array('b_stable',1),
        array('b_stable',2),  array('b_stable',3),
        // <<< LC is now researchable. Bot research action fires automatically. >>>
        // Bump storage/farm so LC iron costs (250I each) never overflow iron pit
        array('b_storage',7), array('b_farm',7),
        array('b_wood',5),    array('b_stone',5),   array('b_iron',5),

        // -------------------------------------------------------
        // PHASE 3: Scaling the Engine
        // Pattern: storage/farm → pits → military. Never let pits outrun storage.
        // Iron mine intentionally 1 level behind wood/stone until LC is flowing
        // (LC costs 125W/100S/250I — iron is the actual bottleneck, not wood).
        // -------------------------------------------------------
        array('b_storage',9), array('b_farm',9),
        array('b_wood',7),    array('b_stone',7),   array('b_iron',6),
        array('b_main',8),
        array('b_smith',7),   array('b_stable',5),
        array('b_storage',11),array('b_farm',11),
        array('b_wood',9),    array('b_stone',9),   array('b_iron',8),
        array('b_barracks',8), array('b_smith',9),  array('b_stable',7),
        array('b_storage',13),array('b_farm',13),
        array('b_wood',11),   array('b_stone',11),  array('b_iron',10),
        array('b_main',10),
        array('b_barracks',10),array('b_smith',10), array('b_stable',9),
        array('b_storage',15),array('b_farm',15),
        array('b_wood',13),   array('b_stone',13),  array('b_iron',12),
        array('b_stable',10),
        array('b_barracks',12),array('b_smith',12),
        array('b_storage',17),array('b_farm',17),
        array('b_wood',15),   array('b_stone',15),  array('b_iron',14),
        array('b_smith',14),  array('b_stable',12),
        array('b_barracks',15),
        array('b_storage',19),array('b_farm',19),
        array('b_wood',17),   array('b_stone',17),  array('b_iron',16),
        array('b_smith',16),  array('b_stable',15),
        array('b_barracks',18),
        array('b_storage',21),array('b_farm',21),
        array('b_wood',19),   array('b_stone',19),  array('b_iron',18),
        array('b_smith',18),  array('b_stable',18),
        array('b_barracks',20),
        // Wall — moderate priority, enough to deter farms
        array('b_wall',5),

        // -------------------------------------------------------
        // PHASE 4: The Push to Noble
        // HQ20, Smith20, Market10, Workshop1, Academy
        // Storage must be high enough to bank the Academy's 15k/25k/10k cost
        // -------------------------------------------------------
        array('b_storage',23),array('b_farm',23),
        array('b_wood',20),   array('b_stone',20),  array('b_iron',20),
        array('b_main',15),
        array('b_smith',19),
        array('b_stable',20), array('b_barracks',22),
        array('b_storage',25),array('b_farm',25),
        array('b_wood',22),   array('b_stone',22),  array('b_iron',22),
        array('b_main',20),           // HQ20 — min-max cap, do not go higher
        array('b_smith',20),          // Smith20 — Academy prerequisite
        array('b_barracks',25),
        array('b_market',5),
        array('b_market',10),         // Market10 — Academy prerequisite
        array('b_garage',1),          // Workshop1 — enables Rams for wall clearing
        array('b_market',15),
        array('b_market',20),
        array('b_wall',8),
        array('b_snob',1),            // Academy — begin nobling second village

        // -------------------------------------------------------
        // FINAL: Max all buildings to reach ~9800 points
        // Iron equalised now that army is self-sustaining
        // -------------------------------------------------------
        array('b_storage',27),array('b_farm',27),
        array('b_wood',25),   array('b_stone',25),  array('b_iron',24),
        array('b_storage',29),array('b_farm',29),
        array('b_wood',27),   array('b_stone',27),  array('b_iron',26),
        array('b_storage',30),array('b_farm',30),
        array('b_wood',28),   array('b_stone',28),  array('b_iron',28),
        array('b_wood',30),   array('b_stone',30),  array('b_iron',30),
        array('b_wall',10),   array('b_wall',15),   array('b_wall',20)
    );
}

/* =========================================================
   SECTION 7: GAME FORMULA HELPERS
   ========================================================= */

// Population capacity of a farm at the given level
// Formula: floor(240 * 1.172103 ^ (level-1))
function superbot_farm_cap($level) {
    if ($level < 1) return 240;
    return (int) floor(240 * pow(1.172103, $level - 1));
}

// Warehouse capacity at the given level
// Formula: floor(1000 * 1.2294934 ^ (level-1))
function superbot_storage_cap($level) {
    if ($level < 1) return 1000;
    return (int) floor(1000 * pow(1.2294934, $level - 1));
}

// Actual resource cost to build a building FROM current_level TO current_level+1
// Formula: floor(base * growth_factor ^ current_level)
// (current_level is 0-indexed exponent: buying level 1 costs base*factor^0 = base)
function superbot_building_cost($bspec, $current_level) {
    $mult = pow($bspec['cost'][3], $current_level);
    return array(
        (int) floor($bspec['cost'][0] * $mult),
        (int) floor($bspec['cost'][1] * $mult),
        (int) floor($bspec['cost'][2] * $mult)
    );
}

// Resource production rate per hour at a given building level
// Formula: floor(30 * 1.163118 ^ (level-1))
function superbot_prod_rate($level) {
    if ($level < 1) return 30;
    return (int) floor(30 * pow(1.163118, $level - 1));
}


/* =========================================================
   SECTION 7b: SYNC r_bh FROM GROUND TRUTH

   r_bh (population used) cannot be trusted as a running ledger because:
     - SQL resets zero it while troops already exist in unit_place
     - The game engine adjusts it independently on troop return
     - Any drift cascades into the recruit throttle, allowing infinite queuing

   We RECOMPUTE r_bh each tick from:
     1. Home troops in unit_place (ground truth)
     2. In-training troops in recruit queue (pop pre-paid but not yet home)
   Troops in transit (outgoing attacks/support) are NOT counted here —
   the game engine owns that accounting when they depart and return.
   ========================================================= */
function superbot_sync_rbh($vid, $COL, $conn) {
    $vid = intval($vid);

    $pop_costs = array(
        'unit_spear'    => 1, 'unit_sword'    => 1, 'unit_axe'      => 1,
        'unit_archer'   => 1, 'unit_spy'      => 2, 'unit_light'    => 4,
        'unit_marcher'  => 5, 'unit_heavy'    => 6, 'unit_ram'      => 5,
        'unit_catapult' => 8, 'unit_snob'     => 100, 'unit_knight' => 10,
        'unit_miliz'    => 1,
    );

    // 1. All troops OWNED by this village — home garrison + troops stationed at
    //    sibling/foreign villages via support movements. unit_place.villages_from_id
    //    is the owning village; villages_to_id is where they currently stand.
    //    Both count against this village's farm cap.
    $home_pop = 0;
    $uq = mysql_query(
        "SELECT unit_spear, unit_sword, unit_axe, unit_archer, unit_spy," .
        " unit_light, unit_marcher, unit_heavy, unit_ram, unit_catapult," .
        " unit_snob, unit_knight, unit_miliz" .
        " FROM unit_place" .
        " WHERE villages_from_id='" . $vid . "'",
        $conn
    );
    if ($uq) {
        while ($ur = mysql_fetch_assoc($uq)) {
            $cols = array_keys($pop_costs);
            for ($i = 0; $i < count($cols); $i++) {
                $c = $cols[$i];
                if (isset($ur[$c])) $home_pop += intval($ur[$c]) * $pop_costs[$c];
            }
        }
    }

    // 2. In-training pop — troops in recruit queue, already consuming farm space
    $queue_pop = 0;
    $rq = mysql_query(
        "SELECT unit, (num_unit - num_finished) AS in_training" .
        " FROM recruit WHERE villageid='" . $vid . "'",
        $conn
    );
    if ($rq) {
        while ($rr = mysql_fetch_assoc($rq)) {
            $in_t  = max(0, intval($rr['in_training']));
            $pcost = isset($pop_costs[$rr['unit']]) ? $pop_costs[$rr['unit']] : 1;
            $queue_pop += $in_t * $pcost;
        }
    }

    // 3. Away troops — troops in-flight on movements still consume farm space.
    // The game engine tracks these separately; we must include them so r_bh
    // reflects true farm usage and the farm-full check works correctly.
    $away_pop = 0;
    $away_pc  = array(1,1,1,1,2,4,5,6,5,8,10,100,1); // spear/sword/axe/archer/spy/lc/ma/hc/ram/cat/knight/noble/miliz
    $mq = mysql_query(
        "SELECT units FROM movements WHERE from_village='" . $vid . "' AND end_time > " . time(),
        $conn
    );
    if ($mq) {
        while ($mr = mysql_fetch_row($mq)) {
            $mparts = explode(';', $mr[0]);
            for ($mi = 0; $mi < count($mparts) && $mi < count($away_pc); $mi++) {
                $away_pop += intval($mparts[$mi]) * $away_pc[$mi];
            }
        }
    }

    $true_rbh = $home_pop + $queue_pop + $away_pop;

    // Write back only if changed — avoids unnecessary DB writes
    $cq = mysql_query(
        "SELECT `" . $COL['r_pop'] . "` AS rbh FROM villages WHERE id='" . $vid . "'", $conn
    );
    if ($cq) {
        $cr = mysql_fetch_assoc($cq);
        if (intval($cr['rbh']) != $true_rbh) {
            mysql_query(
                "UPDATE villages SET `" . $COL['r_pop'] . "`=" . $true_rbh .
                " WHERE id='" . $vid . "'",
                $conn
            );
        }
    }
}

/* =========================================================
   SECTION 8: LAZY PRODUCTION UPDATE

   TWLan stores a frozen resource snapshot + timestamp (last_prod_aktu).
   It only recalculates when a real player loads a page.
   NPC villages are never visited, so their snapshot goes stale.

   Before touching resources we calculate elapsed production and
   write the updated values back — exactly as the game engine would.
   ========================================================= */

function superbot_apply_production($vid, $COL, $now, $conn) {
    $vid = intval($vid);

    $q = mysql_query(
        "SELECT r_wood, r_stone, r_iron," .
        " `" . $COL['b_wood']    . "` AS wl," .
        " `" . $COL['b_stone']   . "` AS sl," .
        " `" . $COL['b_iron']    . "` AS il," .
        " `" . $COL['b_storage'] . "` AS stor_lv," .
        " `" . $COL['last_prod'] . "` AS last_ts" .
        " FROM villages WHERE id='" . $vid . "'",
        $conn
    );
    if (!$q) return;
    $v = mysql_fetch_assoc($q);
    if (!$v) return;

    $last_ts = intval($v['last_ts']);
    if ($last_ts <= 0) $last_ts = $now;
    $elapsed = $now - $last_ts;
    if ($elapsed <= 0) return;

    $max_stor = superbot_storage_cap(intval($v['stor_lv']));

    $gain_w = (int) floor(superbot_prod_rate(intval($v['wl'])) * $elapsed / 3600);
    $gain_s = (int) floor(superbot_prod_rate(intval($v['sl'])) * $elapsed / 3600);
    $gain_i = (int) floor(superbot_prod_rate(intval($v['il'])) * $elapsed / 3600);

    $new_w = min($max_stor, intval($v['r_wood'])  + $gain_w);
    $new_s = min($max_stor, intval($v['r_stone']) + $gain_s);
    $new_i = min($max_stor, intval($v['r_iron'])  + $gain_i);

    mysql_query(
        "UPDATE villages SET" .
        "  r_wood='"  . $new_w . "'," .
        "  r_stone='" . $new_s . "'," .
        "  r_iron='"  . $new_i . "'," .
        "  `" . $COL['last_prod'] . "`='" . $now . "'" .
        " WHERE id='" . $vid . "'",
        $conn
    );
}

/* =========================================================
   SECTION 8b: PROCESS COMPLETED EVENTS

   The TWLan game engine only processes build/recruit completions
   when a real player loads a page (via run_events.php triggered
   by include_inc.php). NPC villages are never visited, so events
   sit in the queue forever unless we process them ourselves.

   This function must be called BEFORE apply_production and
   BEFORE any action on a village. It:

   1. Finds completed builds (build.end_time <= now)
      - Increments the building column in villages
      - Recalculates and updates village points
      - Deletes the build row and any matching events row

   2. Finds completed recruits (recruit.time_finished <= now)
      - Adds troops to unit_place home garrison
      - Deletes the recruit row

   3. Finds completed research (research.end_time <= now)
      - Ensures tec_level is set in villages (we set it immediately
        on insert but belt-and-suspenders)
      - Deletes the research row

   This makes NPC villages fully self-contained — no login required.

   Points formula: TWLan awards points per building level.
   Each level costs resources; points = sum(cost) / 1000 per level.
   We recalculate total points from all building levels each time
   rather than trying to add a delta (simpler and always correct).
   ========================================================= */

function superbot_process_events($vid, $COL, $BUILDINGS, $UNITS, $now, $conn) {
    $vid  = intval($vid);
    $msgs = array();

    // ---- 1. COMPLETED BUILDS ----
    $bq = mysql_query(
        "SELECT id, building FROM build" .
        " WHERE villageid='" . $vid . "' AND end_time <= " . $now,
        $conn
    );
    if ($bq) {
        while ($br = mysql_fetch_assoc($bq)) {
            $bid       = intval($br['id']);
            $bname_raw = $br['building']; // e.g. 'main', 'barracks', 'wood'

            // Find which COL key maps to this DB column name
            $db_col  = $bname_raw; // the actual column name
            $bspec   = null;
            $bcolkey = null;
            $bkeys   = array_keys($BUILDINGS);
            for ($i = 0; $i < count($bkeys); $i++) {
                $bk = $bkeys[$i];
                if (isset($COL[$bk]) && $COL[$bk] == $bname_raw) {
                    $bspec   = $BUILDINGS[$bk];
                    $bcolkey = $bk;
                    break;
                }
            }

            if ($bspec) {
                // Increment the building level
                mysql_query(
                    "UPDATE villages SET `" . $db_col . "`=`" . $db_col . "`+1" .
                    " WHERE id='" . $vid . "'",
                    $conn
                );

                // Re-read ALL building levels and recompute points from scratch
                // Collect all building col names
                $build_cols = array();
                $bk2s = array_keys($BUILDINGS);
                for ($i = 0; $i < count($bk2s); $i++) {
                    $bk2 = $bk2s[$i];
                    if (isset($COL[$bk2])) $build_cols[] = '`' . $COL[$bk2] . '`';
                }
                $sel = implode(',', $build_cols);
                $pq  = mysql_query("SELECT " . $sel . " FROM villages WHERE id='" . $vid . "'", $conn);
                if ($pq) {
                    $prow = mysql_fetch_assoc($pq);
                    $total_pts = 0;
                    $bk2s2 = array_keys($BUILDINGS);
                    for ($i = 0; $i < count($bk2s2); $i++) {
                        $bk2 = $bk2s2[$i];
                        if (!isset($COL[$bk2])) continue;
                        $lv = intval($prow[$COL[$bk2]]);
                        $bs = $BUILDINGS[$bk2];
                        // Sum cost of all levels 0..lv-1
                        for ($lev = 0; $lev < $lv; $lev++) {
                            $c = superbot_building_cost($bs, $lev);
                            $total_pts += (int) floor(($c[0] + $c[1] + $c[2]) / 1000);
                        }
                    }
                    // Minimum 1 pt per building level so village is always visible
                    if ($total_pts < 1) $total_pts = 1;
                    mysql_query(
                        "UPDATE villages SET `" . $COL['points'] . "`='" . $total_pts . "'" .
                        " WHERE id='" . $vid . "'",
                        $conn
                    );
                }

                $msgs[] = 'COMPLETED BUILD: ' . $db_col;
            }

            // Delete the build row (whether we found a spec or not)
            mysql_query("DELETE FROM build WHERE id='" . $bid . "'", $conn);
            // Delete matching events row if it exists
            mysql_query(
                "DELETE FROM events WHERE event_type='build' AND event_id='" . $bid . "'" .
                " AND villageid='" . $vid . "'",
                $conn
            );
        }
    }

    // ---- 2. RECRUIT TRICKLE + COMPLETION ----
    // Mirrors the real TW mechanic: units become available one-by-one
    // throughout training, not all at once at the end.
    //
    // IMPORTANT: The game engine (triggered by the HTTP session ping) ALSO
    // processes recruit rows and writes to unit_place + r_bh. To prevent
    // double-crediting troops, we only run our trickle if the game engine
    // has NOT already touched this row in the current tick window (2 seconds).
    // We detect this via last_reload: if last_reload >= (now - 2), the game
    // engine just processed it and we must skip it entirely.

    $rq = mysql_query(
        "SELECT id, unit, num_unit, num_finished, last_reload, time_start, time_per_unit, time_finished" .
        " FROM recruit WHERE villageid='" . $vid . "'",
        $conn
    );
    if ($rq) {
        while ($rr = mysql_fetch_assoc($rq)) {
            $rid           = intval($rr['id']);
            $unit_col      = $rr['unit'];
            $num_total     = intval($rr['num_unit']);
            $num_done_prev = intval($rr['num_finished']);
            $last_reload   = intval($rr['last_reload']);
            $time_start    = intval($rr['time_start']);
            $tpu           = intval($rr['time_per_unit']);
            if ($tpu < 1) $tpu = 1;

            // If the game engine updated this row within the last 2 seconds,
            // it already graduated the troops — skip to avoid duplication.
            if ($last_reload >= ($now - 2)) {
                // Game engine handled it. Only clean up completed rows.
                $time_finished_row = intval($rr['time_finished']);
                if ($time_finished_row <= $now) {
                    mysql_query("DELETE FROM recruit WHERE id='" . $rid . "'", $conn);
                }
                continue;
            }

            // Use actual total duration for accuracy (stored tpu may be rounded)
            $time_finished_row = intval($rr['time_finished']);
            $total_duration    = max(1, $time_finished_row - $time_start);
            $elapsed           = max(0, $now - $time_start);
            $num_done_now      = min($num_total, (int)floor($num_total * $elapsed / $total_duration));

            // Units newly ready since last tick
            $newly_done = $num_done_now - $num_done_prev;

            if ($newly_done > 0) {
                superbot_ensure_unit_place($vid, $conn);

                // Credit newly finished units to home garrison immediately
                mysql_query(
                    "UPDATE unit_place SET `" . $unit_col . "`=`" . $unit_col . "`+" . $newly_done .
                    " WHERE villages_from_id='" . $vid . "' AND villages_to_id='" . $vid . "'",
                    $conn
                );

                // r_bh was pre-paid when the batch was queued — do NOT increment here.
                // The game engine also does NOT increment r_bh on graduation (it stays
                // pre-paid). r_bh is only decremented when troops are sent on attack.

                // Increment all_unit_X counter
                $au_col = 'all_unit_' . str_replace('unit_', '', $unit_col);
                mysql_query(
                    "UPDATE villages SET `" . $au_col . "`=`" . $au_col . "`+" . $newly_done .
                    " WHERE id='" . $vid . "'",
                    $conn
                );

                // Update num_finished so we don't double-credit next tick
                mysql_query(
                    "UPDATE recruit SET num_finished=" . $num_done_now .
                    ", last_reload=" . $now .
                    " WHERE id='" . $rid . "'",
                    $conn
                );

                $msgs[] = 'TRICKLE: +' . $newly_done . ' ' . $unit_col . ' (' . $num_done_now . '/' . $num_total . ')';
            }

            // If entire batch is done, clean up the row
            if ($num_done_now >= $num_total) {
                mysql_query("DELETE FROM recruit WHERE id='" . $rid . "'", $conn);
            }
        }
    }

    // ---- 3. COMPLETED RESEARCH ----
    $reseq = mysql_query(
        "SELECT id, research FROM research" .
        " WHERE villageid='" . $vid . "' AND end_time <= " . $now,
        $conn
    );
    if ($reseq) {
        while ($resr = mysql_fetch_assoc($reseq)) {
            $resid     = intval($resr['id']);
            $res_name  = $resr['research']; // e.g. 'unit_sword'

            // Map research name to tec_col in villages
            // research stores 'unit_sword', COL maps 'tec_sword' -> 'unit_sword_tec_level'
            $tec_key = 'tec_' . str_replace('unit_', '', $res_name);
            if (isset($COL[$tec_key])) {
                mysql_query(
                    "UPDATE villages SET `" . $COL[$tec_key] . "`=1 WHERE id='" . $vid . "'",
                    $conn
                );
            }
            mysql_query("DELETE FROM research WHERE id='" . $resid . "'", $conn);
            $msgs[] = 'COMPLETED RESEARCH: ' . $res_name;
        }
    }


    // ---- 4. ENSURE GAME ENGINE CAN PROCESS ARRIVED ATTACKS -------------------
    // We do NOT resolve combat manually. TWLan's run_events.php handles
    // everything correctly: combat calc, reports, OD stats, troop returns.
    //
    // Our only job here is to ensure a 'movement' event row exists for every
    // arrived attack so the game engine's event loop picks it up. If a movement
    // has been sitting with end_time in the past for over 60 seconds and has
    // no event row, we insert one to re-queue it.
    //
    // The HTTP session ping (bot_session.php) + fake session (superbot_maintain_session)
    // ensures run_events.php fires on every bot tick for each user.
    $stale_cutoff = $now - 60; // movements arrived more than 60s ago with no event = missed
    $mv_q = mysql_query(
        "SELECT m.id AS mid, m.from_userid, m.to_userid" .
        " FROM movements m" .
        " WHERE m.to_village='" . $vid . "' AND m.type='attack' AND m.end_time <= " . $stale_cutoff .
        " AND NOT EXISTS (" .
        "   SELECT 1 FROM events e WHERE e.event_type='movement' AND e.event_id=m.id" .
        " )",
        $conn
    );
    if ($mv_q) {
        while ($mv = mysql_fetch_assoc($mv_q)) {
            $mid     = intval($mv['mid']);
            $fuid    = intval($mv['from_userid']);
            // Re-insert the missing event row so run_events will process this movement
            mysql_query(
                "INSERT INTO events (event_time, event_type, event_id, user_id, villageid)" .
                " VALUES ('" . $now . "', 'movement', '" . $mid . "', '" . $fuid . "', '" . $vid . "')",
                $conn
            );
            $msgs[] = 'REQUEUED stale attack movement id=' . $mid . ' for game engine';
        }
    }

    return $msgs;
}

/* =========================================================
   SECTION 8c: MAINTAIN FAKE SESSION

   The game engine (run_events.php, include_inc.php) only processes
   a user's events if they have an active row in the `sessions` table.
   We upsert a fake session for each bot user so the game's own
   event processor also runs for them when any real player is logged in
   and triggers the global event loop.

   sessions columns: id, userid, sid, hkey, is_vacation
   We use a fixed fake sid derived from the userid.
   ========================================================= */

function superbot_maintain_session($uid, $conn) {
    $uid     = intval($uid);
    $fake_sid = 'superbot_' . $uid . '_' . substr(md5('superbot' . $uid), 0, 16);
    $fake_hkey = 'sb';

    // Check if a session row exists for this user
    $sq = mysql_query(
        "SELECT id FROM sessions WHERE userid='" . $uid . "'",
        $conn
    );
    if ($sq && mysql_num_rows($sq) > 0) {
        // Update existing — refresh hkey to keep it alive
        $row = mysql_fetch_assoc($sq);
        mysql_query(
            "UPDATE sessions SET sid='" . mysql_real_escape_string($fake_sid) . "'," .
            " hkey='" . $fake_hkey . "', is_vacation=0" .
            " WHERE id='" . intval($row['id']) . "'",
            $conn
        );
    } else {
        // Insert new fake session
        mysql_query(
            "INSERT INTO sessions (userid, sid, hkey, is_vacation)" .
            " VALUES ('" . $uid . "', '" . mysql_real_escape_string($fake_sid) . "'," .
            " '" . $fake_hkey . "', 0)",
            $conn
        );
    }
}



   //Tech levels are stored directly in the villages table as
   //unit_X_tec_level (1 = researched, 0 = not).
   //Spear fighter has tec_level=1 by default (always available).

   //Returns true if a unit is recruitable for this village.
   //Checks:
   //  1. Required building exists at minimum level
   //  2. Smithy is at minimum required level
    // 3. unit_X_tec_level = 1 in villages table
   //========================================================= */

function superbot_unit_is_unlocked($uspec, $village, $COL) {
    if ($uspec['always']) return true;
    $smith_lv = isset($village[$COL['b_smith']]) ? intval($village[$COL['b_smith']]) : 0;
    if ($smith_lv < $uspec['smith']) return false;
    // Nobleman has no tec_col — Academy building IS the unlock (bmin=1 on b_snob)
    if (!isset($uspec['tec_col']) || $uspec['tec_col'] === null) return true;
    $tec_col = isset($COL[$uspec['tec_col']]) ? $COL[$uspec['tec_col']] : null;
    if (!$tec_col) return false;
    if (!isset($village[$tec_col])) return false;
    return (intval($village[$tec_col]) == 1);
}

/* =========================================================
   SECTION 10: RESEARCH ACTION

   If a unit can be researched (smithy level met, not yet researched),
   insert a row into the `research` table so the game engine processes it.
   Also sets the tec_level directly in villages for immediate bot use.

   research table: id, research, villageid, end_time
   ========================================================= */

function superbot_action_research($village, $COL, $UNITS, $now, $conn) {
    $vid   = intval($village['id']);
    $vname = $village['name'];

    $smith_lv = isset($village[$COL['b_smith']]) ? intval($village[$COL['b_smith']]) : 0;
    if ($smith_lv < 1) {
        return array('info', '[' . $vname . '] RESEARCH: No smithy yet');
    }

    // Clean stale research rows first
    mysql_query(
        "DELETE FROM research WHERE villageid='" . $vid . "' AND end_time <= " . $now,
        $conn
    );

    // Check if research queue is busy
    $rq = mysql_query(
        "SELECT COUNT(*) AS c FROM research WHERE villageid='" . $vid . "' AND end_time > " . $now,
        $conn
    );
    if ($rq) {
        $rrow = mysql_fetch_assoc($rq);
        if (intval($rrow['c']) > 0) {
            return array('info', '[' . $vname . '] RESEARCH: Queue busy');
        }
    }

    // Priority order for research.
    // Human min-max logic:
    //   1. Axe first (smith 2) — barracks is already training, axe is the best barracks unit
    //   2. Spy (smith 2, stable prereq met at same time) — needed before LC can be used effectively
    //   3. Light cav (smith 5) — THE priority unlock; once stable 3 + smith 5 exists, get LC asap
    //   4. Sword (smith 1) — good def unit but lower priority than LC for early growth
    //   5. Ram (smith 3) — needed for wall clearing later
    //   6. Marcher/archer/heavy/catapult — late game
    $order = array('u_axe', 'u_spy', 'u_light', 'u_sword', 'u_archer', 'u_ram', 'u_marcher', 'u_heavy', 'u_catapult');

    for ($i = 0; $i < count($order); $i++) {
        $ukey  = $order[$i];
        $uspec = $UNITS[$ukey];

        // Already researched?
        $tec_col = isset($COL[$uspec['tec_col']]) ? $COL[$uspec['tec_col']] : null;
        if (!$tec_col) continue;
        if (isset($village[$tec_col]) && intval($village[$tec_col]) == 1) continue;

        // Smithy level must be met before research is possible
        if ($smith_lv < $uspec['smith']) continue;

        // Queue the research (5 second completion for bot purposes)
        $end_time = $now + 5;

        // The research column name matches the unit column name in unit_place
        $research_name = $COL['u_' . str_replace('u_', '', $ukey)];

        $ins = mysql_query(
            "INSERT INTO research (research, villageid, end_time)" .
            " VALUES ('" . $research_name . "', '" . $vid . "', '" . $end_time . "')",
            $conn
        );
        if (!$ins) {
            return array('error', '[' . $vname . '] RESEARCH: Insert failed - ' . mysql_error($conn));
        }

        // Also immediately set tec_level so the bot can recruit this unit next cycle
        mysql_query(
            "UPDATE villages SET `" . $tec_col . "`=1 WHERE id='" . $vid . "'",
            $conn
        );

        return array('research', '[' . $vname . '] RESEARCHING: ' . $research_name);
    }

    return array('info', '[' . $vname . '] RESEARCH: All available units researched');
}

/* =========================================================
   SECTION 11: BUILD QUEUE CHECK
   Cleans expired build rows first (NPC villages never get engine-processed),
   then returns true if the queue is clear.
   ========================================================= */

function superbot_build_queue_clear($vid, $now, $conn) {
    $vid = intval($vid);
    // Remove any completed/stale build rows so they don't block future builds
    mysql_query(
        "DELETE FROM build WHERE villageid='" . $vid . "' AND end_time <= " . $now,
        $conn
    );
    $r = mysql_query(
        "SELECT COUNT(*) AS c FROM build WHERE villageid='" . $vid . "' AND end_time > " . $now,
        $conn
    );
    if (!$r) return true;
    $row = mysql_fetch_assoc($r);
    return (intval($row['c']) == 0);
}

/* =========================================================
   SECTION 12: ENSURE unit_place ROW EXISTS
   Villages sometimes start without a home garrison row.
   ========================================================= */

function superbot_ensure_unit_place($vid, $conn) {
    $vid = intval($vid);
    $r = mysql_query(
        "SELECT COUNT(*) AS c FROM unit_place" .
        " WHERE villages_from_id='" . $vid . "' AND villages_to_id='" . $vid . "'",
        $conn
    );
    if (!$r) return;
    $row = mysql_fetch_assoc($r);
    if (intval($row['c']) == 0) {
        mysql_query(
            "INSERT INTO unit_place (villages_from_id, villages_to_id)" .
            " VALUES ('" . $vid . "', '" . $vid . "')",
            $conn
        );
    }
}

/* =========================================================
   SECTION 13: BUILD ACTION

   Walks the build plan for this village's profile.
   Checks prerequisites (building requirements) before each step.
   Uses exponential cost formula — never flat costs.
   Emergency farm upgrade fires when population is near cap.
   Upgrades warehouse automatically if a build cost exceeds capacity.
   ========================================================= */

function superbot_action_build($village, $COL, $BUILDINGS, $profile, $now, $conn) {
    $vid   = intval($village['id']);
    $uid   = intval($village['userid']);
    $vname = $village['name'];

    // Read fresh resources from DB (production was applied before this call)
    $q = mysql_query(
        "SELECT r_wood, r_stone, r_iron," .
        " `" . $COL['r_pop']     . "` AS rp," .
        " `" . $COL['b_farm']    . "` AS farm_lv," .
        " `" . $COL['b_storage'] . "` AS stor_lv" .
        " FROM villages WHERE id='" . $vid . "'",
        $conn
    );
    if (!$q) return array('error', '[' . $vname . '] BUILD: DB error - ' . mysql_error($conn));

    $f        = mysql_fetch_assoc($q);
    $wood     = (int) round($f['r_wood']);
    $stone    = (int) round($f['r_stone']);
    $iron     = (int) round($f['r_iron']);
    $pop      = intval($f['rp']);
    $farm_lv  = intval($f['farm_lv']);
    $stor_lv  = intval($f['stor_lv']);
    $max_pop  = superbot_farm_cap($farm_lv);
    $max_stor = superbot_storage_cap($stor_lv);

    // Emergency farm: upgrade farm immediately if population within 20 of cap
    $target_key = null;
    if (($max_pop - $pop) < 20 && $farm_lv < 30) {
        $target_key = 'b_farm';
    }

    // Emergency storage: if any resource is >= 90% of warehouse capacity, upgrade storage NOW.
    // At 90% we are actively wasting production every hour. A human player never lets this happen.
    if ($target_key == null && $stor_lv < 30) {
        $overflow_threshold = (int)($max_stor * 0.90);
        if ($wood >= $overflow_threshold || $stone >= $overflow_threshold || $iron >= $overflow_threshold) {
            $target_key = 'b_storage';
        }
    }

    // Otherwise find the next incomplete build plan step
    if ($target_key == null) {
        $plan = superbot_get_build_plan($profile);
        for ($i = 0; $i < count($plan); $i++) {
            $bkey      = $plan[$i][0];
            $target_lv = $plan[$i][1];

            if (!isset($COL[$bkey])) continue;
            $db_col = $COL[$bkey];
            $cur_lv = isset($village[$db_col]) ? intval($village[$db_col]) : 0;

            if ($cur_lv >= $target_lv) continue;        // already done
            if (!isset($BUILDINGS[$bkey])) continue;
            $bspec = $BUILDINGS[$bkey];
            if ($cur_lv >= $bspec['max']) continue;     // at max already

            // Check all building prerequisites
            $ok   = true;
            $rkeys = array_keys($bspec['req']);
            for ($ri = 0; $ri < count($rkeys); $ri++) {
                $rk     = $rkeys[$ri];
                $rlv    = $bspec['req'][$rk];
                $rc     = isset($COL[$rk]) ? $COL[$rk] : null;
                $have   = ($rc && isset($village[$rc])) ? intval($village[$rc]) : 0;
                if ($have < $rlv) { $ok = false; break; }
            }
            if (!$ok) continue;

            $target_key = $bkey;
            break;
        }
    }

    if ($target_key == null) {
        return array('info', '[' . $vname . '] BUILD: Plan complete');
    }

    $bspec  = $BUILDINGS[$target_key];
    $db_col = $COL[$target_key];
    $cur_lv = isset($village[$db_col]) ? intval($village[$db_col]) : 0;
    $cost   = superbot_building_cost($bspec, $cur_lv);

    // If cost exceeds warehouse, upgrade warehouse first
    if (($cost[0] > $max_stor || $cost[1] > $max_stor || $cost[2] > $max_stor)
        && $target_key != 'b_storage') {
        $target_key = 'b_storage';
        $bspec      = $BUILDINGS['b_storage'];
        $db_col     = $COL['b_storage'];
        $cur_lv     = isset($village[$db_col]) ? intval($village[$db_col]) : 0;
        $cost       = superbot_building_cost($bspec, $cur_lv);
    }

    if ($wood >= $cost[0] && $stone >= $cost[1] && $iron >= $cost[2]) {

        // Deduct resources
        $ok = mysql_query(
            "UPDATE villages SET" .
            "  r_wood = r_wood - "   . $cost[0] . "," .
            "  r_stone = r_stone - " . $cost[1] . "," .
            "  r_iron = r_iron - "   . $cost[2] . "," .
            "  `" . $COL['last_prod'] . "` = " . $now .
            " WHERE id='" . $vid . "'",
            $conn
        );
        if (!$ok) return array('error', '[' . $vname . '] BUILD: Deduct failed - ' . mysql_error($conn));

        $end_time = $now + 5;

        $ins = mysql_query(
            "INSERT INTO build (building, villageid, end_time, build_time)" .
            " VALUES ('" . $db_col . "', '" . $vid . "', '" . $end_time . "', '5')",
            $conn
        );
        if (!$ins) {
            // Rollback resource deduction
            mysql_query(
                "UPDATE villages SET r_wood = r_wood+" . $cost[0] .
                ", r_stone = r_stone+" . $cost[1] .
                ", r_iron = r_iron+"   . $cost[2] .
                " WHERE id='" . $vid . "'",
                $conn
            );
            return array('error', '[' . $vname . '] BUILD: Queue insert failed - ' . mysql_error($conn));
        }

        $bid = mysql_insert_id($conn);
        mysql_query(
            "INSERT INTO events (event_time, event_type, event_id, user_id, villageid)" .
            " VALUES ('" . $end_time . "', 'build', '" . $bid . "', '" . $uid . "', '" . $vid . "')",
            $conn
        );

        // Reset save-stall counter — the building fired, throttle lifts immediately
        @mysql_query(
            "UPDATE bot_villages SET save_ticks=0, save_target=''" .
            " WHERE vid='" . $vid . "' AND uid='" . $uid . "'",
            $conn
        );

        return array(
            'build',
            '[' . $vname . '] BUILT: ' . $db_col . ' Lv' . ($cur_lv + 1) .
            ' (cost ' . $cost[0] . 'W ' . $cost[1] . 'S ' . $cost[2] . 'I)'
        );
    }

    // Not enough resources yet
    $need_w = max(0, $cost[0] - $wood);
    $need_s = max(0, $cost[1] - $stone);
    $need_i = max(0, $cost[2] - $iron);
    return array(
        'wait',
        '[' . $vname . '] SAVING for ' . $db_col . ' Lv' . ($cur_lv + 1) .
        ' — need ' . $need_w . 'W ' . $need_s . 'S ' . $need_i . 'I more'
    );
}

/* =========================================================
   SECTION 13b: TROOP:POINTS RATIO GOVERNOR + SAVE-STALL THROTTLE

   Two interlocking mechanisms that together implement the human
   "phase-based" spend table from the strategy guide:

     Phase        | Points    | Troop focus | Recruit multiplier
     -------------|-----------|-------------|-------------------
     Early Start  | < 500     | 80% troops  | 0.80 of avail resources
     Expansion    | 500-3000  | 60% troops  | 0.60
     Mid/Late     | 3000+     | 90% troops  | 0.90 (buildings done)

   Additionally, when the 1:2 ratio (2× points in farm-pop troops) is already
   met or exceeded for the current phase, we slow recruitment further so
   buildings can catch up. If we are UNDER the ratio target, we recruit freely.

   SAVE-STALL THROTTLE:
   When the bot has been "SAVING for barracks Lv6" for 3+ consecutive ticks,
   it means recruitment is eating every incoming wood before it can accumulate.
   We track this with bot_villages.save_ticks + save_target. When stalled ≥ 3
   ticks, recruitment is throttled to 15% of available resources — enough to
   keep the queue warm but not drain the resource needed for the building.
   The moment the build fires, save_ticks resets to 0.

   These two multipliers are combined as min(phase_mult, stall_mult) so the
   more restrictive always wins.
   ========================================================= */

/*
 * Compute how much farm-population the troops in unit_place consume.
 * Matches game pop costs: spear/sword/axe/archer=1, spy=2, LC/marcher=4,
 * heavy=6, ram=5, catapult=8.
 */
function superbot_troop_farm_pop($vid, $conn) {
    // Sum ALL unit_place rows where this village owns the troops (from_id=vid),
    // including troops currently stationed at sibling villages via support.
    $total = 0;
    $q = mysql_query(
        "SELECT unit_spear*1 + unit_sword*1 + unit_axe*1 + unit_archer*1 +" .
        " unit_spy*2 + unit_light*4 + unit_marcher*4 + unit_heavy*6 +" .
        " unit_ram*5 + unit_catapult*8 AS tfp" .
        " FROM unit_place" .
        " WHERE villages_from_id='" . intval($vid) . "'",
        $conn
    );
    if ($q) {
        while ($r = mysql_fetch_assoc($q)) {
            $total += intval($r['tfp']);
        }
    }
    return $total;
}

/*
 * Returns a recruitment resource multiplier (0.0–1.0) based on:
 *  (a) the village's phase (points-based) — how aggressively should we recruit?
 *  (b) whether the 1:2 troop:points ratio is already met — if over-trooped, slow down.
 *  (c) how many ticks we have been stuck saving for the same build (stall throttle).
 *
 * $village_pts  : current village points (int)
 * $troop_farm   : current farm-pop used by troops (int)
 * $save_ticks   : consecutive ticks stuck on same build (int, 0 = not saving)
 *
 * Returns float multiplier applied to avail_w/s/i before passing to queue_building.
 */
function superbot_recruit_multiplier($village_pts, $troop_farm, $save_ticks) {

    // ── Phase multiplier (from the strategy table) ────────────────────────────
    if ($village_pts < 500) {
        // Early Start: 80% troop focus, but only to the ratio target
        $phase_mult       = 0.80;
        $ratio_target     = 1.5;  // aim for 1.5× pts in troops (gentler early on)
    } elseif ($village_pts < 3000) {
        // Expansion: 60% troop, 40% building — balanced phase
        $phase_mult       = 0.60;
        $ratio_target     = 2.0;  // 1:2 ratio target
    } else {
        // Mid/Late: 90% troop (buildings nearly done)
        $phase_mult       = 0.90;
        $ratio_target     = 2.0;
    }

    // ── Ratio governor ────────────────────────────────────────────────────────
    // If already over the ratio target, halve the phase multiplier to let
    // buildings close the gap. Over 1.5× the target = nearly full stop.
    $ratio_mult = 1.0;
    if ($village_pts > 0) {
        $ratio_have = (float)$troop_farm / (float)$village_pts;
        if ($ratio_have >= $ratio_target * 1.5) {
            $ratio_mult = 0.10;  // Way over-trooped: almost stop recruiting
        } elseif ($ratio_have >= $ratio_target) {
            $ratio_mult = 0.30;  // Over ratio: slow right down
        } elseif ($ratio_have >= $ratio_target * 0.80) {
            $ratio_mult = 0.70;  // Approaching ratio: slight brake
        }
        // Under ratio target: ratio_mult stays 1.0 — full speed
    }

    // ── Save-stall throttle ───────────────────────────────────────────────────
    // When saving for a building, throttle recruit spend so resources pool faster.
    // EXCEPTION: if the village is already under its troop ratio target (ratio_mult=1.0),
    // troops are urgently needed — don't throttle, let the building wait.
    // This prevents a wiped village from being permanently stuck at 5-8% recruit speed.
    $stall_mult = 1.0;
    if ($ratio_mult < 1.0) {
        // Already over troop ratio — ratio governor is already throttling, no need for stall too
        $stall_mult = 1.0;
    } elseif ($save_ticks >= 6) {
        $stall_mult = 0.10;  // Hard stall: 6+ ticks — almost pause recruiting
    } elseif ($save_ticks >= 3) {
        $stall_mult = 0.25;  // Soft stall: 3-5 ticks — spend only 25% on troops
    }
    // 0-2 ticks saving, or under troop ratio: no throttle

    // Combined: take the most restrictive of all three factors
    $mult = $phase_mult * $ratio_mult * $stall_mult;

    // Never go below 0.05 — always allow a tiny trickle so queues don't go cold
    return max(0.05, min(1.0, $mult));
}

/* =========================================================
   SECTION 14: RECRUIT ACTION

   THREE INDEPENDENT QUEUES — barracks, stable, workshop.
   A real player trains axe, LC, and rams simultaneously from
   three separate buildings. We do the same.

   Each building checks its own queue slot independently:
     - If barracks queue is empty and axe/spear/sword/archer needed → queue 50
     - If stable queue is empty and LC/scout/marcher/HC needed → queue 50
     - If workshop queue is empty and ram/catapult needed → queue 50

   Batch size = 50 (max). This means:
     - Units arrive in ~10-30 second bursts at world_speed 1000
     - Always available in the rally point soon after queuing
     - Three buildings all training in parallel like a real player

   TIME FORMULA (calibrated):
     Axe barracks 25 speed 1000 → 1000 units = 194s
     time_per_unit = base_s / (world_speed × 3.6748) / 1.026^(level−1)
   ========================================================= */

$UNIT_BASE_TIMES = array(
    'u_spear'    => 1080,
    'u_sword'    => 1500,
    'u_axe'      => 1320,
    'u_archer'   => 1800,
    'u_spy'      =>  900,
    'u_light'    => 1800,
    'u_marcher'  => 2400,
    'u_heavy'    => 3600,
    'u_ram'      => 4800,
    'u_catapult' => 7200
);

function superbot_recruit_time_per_unit($ukey, $building_level, $world_speed) {
    global $UNIT_BASE_TIMES;
    $base            = isset($UNIT_BASE_TIMES[$ukey]) ? $UNIT_BASE_TIMES[$ukey] : 1800;
    $speed_divisor   = max(1, $world_speed) * 3.6748;
    $building_factor = pow(1.026, max(0, $building_level - 1));
    // Return float — caller rounds the TOTAL batch time, not per-unit
    return $base / $speed_divisor / $building_factor;
}

/*
 * Returns the army target appropriate for this village's current development stage.
 *
 * Human min-max logic:
 *   Phase 1 (no stable): Recruit only spear — cheap, wood-light (50W each),
 *     fills farm and gives early defensive bulk. Do NOT waste iron on axes yet.
 *   Phase 2 (stable 1-2, LC not researched): Spy scouts to enable future raiding.
 *   Phase 3 (LC researched, stable 1+): LC is the priority — best DPS/pop ratio.
 *     Small axe queue to keep barracks busy while iron allows.
 *   Phase 4 (stable 10+, army growing): Scale to full targets.
 *
 * This prevents the "drain wood" problem: early LC + axe queues simultaneously
 * exhaust wood (60W axe + 125W LC each tick) leaving nothing for buildings.
 */
function superbot_get_army_target($profile, $village = null, $COL = null) {
    // If no village context provided, return full end-game targets (backward compat)
    if ($village === null || $COL === null) {
        if ($profile == 'off') {
            return array('u_axe' => 6500, 'u_light' => 2800, 'u_ram' => 300, 'u_spy' => 100, 'u_snob' => 1);
        }
        if ($profile == 'deff') {
            return array('u_spear' => 10000, 'u_sword' => 10000, 'u_spy' => 100, 'u_snob' => 1);
        }
        return array(
            'u_spear' => 4000, 'u_sword' => 4000, 'u_axe'  => 3000,
            'u_light' => 1000, 'u_ram'   => 150,  'u_spy'  => 100, 'u_snob' => 1
        );
    }

    // Read current building levels from village
    $stb_lv  = isset($village[$COL['b_stable']])   ? intval($village[$COL['b_stable']])   : 0;
    $brk_lv  = isset($village[$COL['b_barracks']]) ? intval($village[$COL['b_barracks']]) : 0;
    $smi_lv  = isset($village[$COL['b_smith']])    ? intval($village[$COL['b_smith']])    : 0;
    $grg_lv  = isset($village[$COL['b_garage']])   ? intval($village[$COL['b_garage']])   : 0;
    $lc_researched = (isset($village[$COL['tec_light']]) && intval($village[$COL['tec_light']]) == 1);
    $axe_researched = (isset($village[$COL['tec_axe']]) && intval($village[$COL['tec_axe']]) == 1);
    $spy_researched = (isset($village[$COL['tec_spy']]) && intval($village[$COL['tec_spy']]) == 1);

    // --- OFF profile: phased ---
    if ($profile == 'off') {
        // No stable yet: only spear (wood-cheap filler) — save wood/iron for buildings
        if ($stb_lv < 1) {
            return array('u_spear' => 200);
        }
        // Stable exists but LC not researched yet: spy scouting + small axe
        if (!$lc_researched) {
            return array('u_spy' => 5, 'u_axe' => ($axe_researched ? 100 : 0), 'u_spear' => 50);
        }
        // LC available, stable < 10: build up LC + axe in proportion
        if ($stb_lv < 10) {
            return array('u_light' => 200, 'u_axe' => 500, 'u_spy' => 20, 'u_spear' => 100);
        }
        // Stable 10+: push toward full nuke
        if ($stb_lv < 18) {
            return array('u_light' => 1000, 'u_axe' => 2000, 'u_spy' => 50, 'u_ram' => ($grg_lv >= 1 ? 50 : 0));
        }
        // Full nuke targets
        return array('u_axe' => 6500, 'u_light' => 2800, 'u_ram' => 300, 'u_spy' => 100, 'u_snob' => 1);
    }

    // --- DEFF profile: phased ---
    if ($profile == 'deff') {
        // No stable yet: build spear + sword (both wood+iron cheap for def bulk)
        if ($stb_lv < 1) {
            $sw_target = ($village[$COL['b_smith']] >= 1 ? 500 : 0);
            return array('u_spear' => 1000, 'u_sword' => $sw_target);
        }
        // Has stable: add scout to enable scouting out attackers
        if (!$spy_researched) {
            return array('u_spear' => 2000, 'u_sword' => 1000);
        }
        // Growing: add spy, a few LC for mobility
        if ($stb_lv < 10) {
            return array('u_spear' => 3000, 'u_sword' => 2000, 'u_spy' => 50,
                'u_light' => ($lc_researched ? 100 : 0));
        }
        // Full deff targets — 10k spear + 10k sword + 100 scout + 1 noble = ~20,300 pop (cap 24,000)
        // Noble included so deff villages can conquer once their wall/army is maxed.
        return array('u_spear' => 10000, 'u_sword' => 10000, 'u_spy' => 100, 'u_snob' => 1);
    }

    // --- OFFDEFF (default) profile: phased ---
    // Phase 1: No stable — only spear (50W/30S/10I, cheap on wood)
    if ($stb_lv < 1) {
        // If barracks is up and axe is researched, train axe instead of spear
        // (better value than spear for mixed villages) — but only if we have iron
        if ($axe_researched) {
            return array('u_axe' => 200, 'u_spear' => 100);
        }
        return array('u_spear' => 300);
    }
    // Phase 2: Stable exists, no LC yet — spy + light prep + axe
    if (!$lc_researched) {
        return array('u_spy' => 5, 'u_axe' => ($axe_researched ? 150 : 0), 'u_spear' => 100);
    }
    // Phase 3: LC researched, stable 1-9 — LC + small axe, spear fills farm cheap
    if ($stb_lv < 8) {
        return array('u_light' => 150, 'u_axe' => 300, 'u_spy' => 20, 'u_spear' => 200);
    }
    // Phase 4: Stable 8-17 — push toward meaningful numbers
    if ($stb_lv < 17) {
        return array(
            'u_light' => 400, 'u_axe' => 1000, 'u_spy' => 50,
            'u_spear' => 500, 'u_sword' => 500,
            'u_ram'   => ($grg_lv >= 1 ? 50 : 0)
        );
    }
    // Phase 5: Stable 17+ — full targets
    return array(
        'u_spear' => 4000, 'u_sword' => 4000, 'u_axe'  => 3000,
        'u_light' => 1000, 'u_ram'   => 150,  'u_spy'  => 100, 'u_snob' => 1
    );
}

/*
 * Try to queue one batch (max 50) for a single building slot.
 * Returns log string or empty string if nothing queued.
 *
 * $candidates = ordered array of ukeys this building can train
 * $building   = 'b_barracks' | 'b_stable' | 'b_garage'
 * $bld_col    = the DB column name for this building (e.g. 'barracks')
 */
function superbot_queue_building($vid, $vname, $building, $bld_col,
                                  $candidates, $targets, $home,
                                  $village, $COL, $UNITS,
                                  $wood, $stone, $iron, $free_pop,
                                  $world_speed, $now, $conn,
                                  $avail_w = -1, $avail_s = -1, $avail_i = -1) {
    // Check if this building's queue slot is busy
    $aq = mysql_query(
        "SELECT COUNT(*) AS c FROM recruit" .
        " WHERE villageid='" . $vid . "'" .
        "   AND building='" . $bld_col . "'" .
        "   AND time_finished > " . $now,
        $conn
    );
    if ($aq) {
        $ar = mysql_fetch_assoc($aq);
        if (intval($ar['c']) > 0) return ''; // busy
    }

    // Find most-needed unit for this building (by deficit ratio)
    $best_ukey  = null;
    $best_ratio = -1.0;
    for ($i = 0; $i < count($candidates); $i++) {
        $ukey   = $candidates[$i];
        $target = isset($targets[$ukey]) ? $targets[$ukey] : 0;
        if ($target <= 0) continue;
        $have   = isset($home[$ukey]) ? $home[$ukey] : 0;
        if ($have >= $target) continue;
        $ratio  = ($target - $have) / (float)$target;
        if ($ratio <= $best_ratio) continue;
        if (!isset($UNITS[$ukey])) continue;
        $uspec  = $UNITS[$ukey];
        if (!superbot_unit_is_unlocked($uspec, $village, $COL)) continue;
        $b_lv_check = isset($COL[$uspec['bi']]) ? intval($village[$COL[$uspec['bi']]]) : 0;
        if ($b_lv_check < $uspec['bmin']) continue;
        $best_ukey  = $ukey;
        $best_ratio = $ratio;
    }

    if ($best_ukey === null) return ''; // nothing needed from this building

    $uspec  = $UNITS[$best_ukey];
    $target = $targets[$best_ukey];
    $have   = isset($home[$best_ukey]) ? $home[$best_ukey] : 0;
    $need   = $target - $have;

    // Use caller-supplied available resources (after build reservation).
    // Fall back to old 15% reserve behaviour if not supplied (backward compat).
    if ($avail_w < 0) $avail_w = (int)($wood  * 0.85);
    if ($avail_s < 0) $avail_s = (int)($stone * 0.85);
    if ($avail_i < 0) $avail_i = (int)($iron  * 0.85);

    $max_w = ($uspec['cost'][0] > 0) ? (int)floor($avail_w / $uspec['cost'][0]) : 99999;
    $max_s = ($uspec['cost'][1] > 0) ? (int)floor($avail_s / $uspec['cost'][1]) : 99999;
    $max_i = ($uspec['cost'][2] > 0) ? (int)floor($avail_i / $uspec['cost'][2]) : 99999;
    $max_p = ($uspec['pop']     > 0) ? (int)floor($free_pop / $uspec['pop'])     : 99999;

    // CAP AT 50 — units arrive quickly, always in rally point within seconds
    $batch = min(50, $need, $max_w, $max_s, $max_i, $max_p);

    if ($batch < 1) return '';

    $b_lv          = isset($village[$bld_col]) ? intval($village[$bld_col]) : 1;
    $tpu_float     = superbot_recruit_time_per_unit($best_ukey, $b_lv, $world_speed);
    $total_secs    = max(1, (int) round($tpu_float * $batch));
    $time_per_unit = max(1, (int) round($tpu_float));
    $time_finished = $now + $total_secs;

    $cost_w = $uspec['cost'][0] * $batch;
    $cost_s = $uspec['cost'][1] * $batch;
    $cost_i = $uspec['cost'][2] * $batch;
    $cost_p = $uspec['pop']     * $batch;

    // r_bh NOT updated here — superbot_sync_rbh() recalculates it each tick.
    mysql_query(
        "UPDATE villages SET" .
        "  r_wood  = r_wood  - "  . $cost_w . "," .
        "  r_stone = r_stone - "  . $cost_s . "," .
        "  r_iron  = r_iron  - "  . $cost_i .
        " WHERE id='" . $vid . "'",
        $conn
    );

    $unit_col = $COL[$best_ukey];
    mysql_query(
        "INSERT INTO recruit" .
        " (unit, num_unit, num_finished, last_reload, time_finished, time_start, time_per_unit, building, villageid)" .
        " VALUES" .
        " ('" . $unit_col . "', " . $batch . ", 0, " . $now . ", " . $time_finished . "," .
        "  " . $now . ", " . $time_per_unit . ", '" . $bld_col . "', '" . $vid . "')",
        $conn
    );

    $have_after = $have + $batch;
    $pct        = (int)(($have_after / (float)$target) * 100);
    $unit_name  = str_replace('u_', '', $best_ukey);
    $secs       = $total_secs;
    $eta        = $secs >= 60 ? ((int)ceil($secs / 60)) . 'min' : $secs . 's';

    return '+' . $batch . ' ' . $unit_name . ' (' . $have_after . '/' . $target . '=' . $pct . '% ETA ' . $eta . ')';
}

function superbot_action_recruit($village, $COL, $UNITS, $BUILDINGS, $profile, $world_speed, $now, $conn) {
    $vid   = intval($village['id']);
    $vname = $village['name'];

    // Fresh resources and farm
    $q = mysql_query(
        "SELECT r_wood, r_stone, r_iron," .
        " `" . $COL['b_farm'] . "` AS farm_lv" .
        " FROM villages WHERE id='" . $vid . "'",
        $conn
    );
    if (!$q) return array('error', '[' . $vname . '] RECRUIT: DB error');
    $f        = mysql_fetch_assoc($q);
    $wood     = (int) round($f['r_wood']);
    $stone    = (int) round($f['r_stone']);
    $iron     = (int) round($f['r_iron']);
    $farm_cap = superbot_farm_cap(intval($f['farm_lv']));

    // Calculate actual population used directly from unit_place + recruit queue.
    // Do NOT trust r_bh — it drifts after SQL resets and game engine interactions.
    // This is the same calculation as superbot_sync_rbh() but inline so it is
    // always accurate even on the same tick before the sync has written back.
    $_pc = array(
        'unit_spear'=>1,'unit_sword'=>1,'unit_axe'=>1,'unit_archer'=>1,
        'unit_spy'=>2,'unit_light'=>4,'unit_marcher'=>5,'unit_heavy'=>6,
        'unit_ram'=>5,'unit_catapult'=>8,'unit_snob'=>100,'unit_knight'=>10,'unit_miliz'=>1
    );
    $pop = 0;
    // Count ALL troops owned by this village (villages_from_id = vid), including
    // any currently stationed at a sibling village via a support movement.
    // A support wave that has arrived is stored in unit_place with from=Blaives,
    // to=Beowulfs — it still consumes Blaives' farm cap, not Beowulfs'.
    // Using only home garrison (from=to=vid) was the root cause of massive overcap.
    $_uq = mysql_query(
        "SELECT unit_spear,unit_sword,unit_axe,unit_archer,unit_spy," .
        "unit_light,unit_marcher,unit_heavy,unit_ram,unit_catapult,unit_snob,unit_knight,unit_miliz" .
        " FROM unit_place WHERE villages_from_id='" . $vid . "'",
        $conn
    );
    if ($_uq) {
        while ($_ur = mysql_fetch_assoc($_uq)) {
            foreach ($_pc as $_col => $_cost) {
                if (isset($_ur[$_col])) $pop += intval($_ur[$_col]) * $_cost;
            }
        }
    }
    $_rq = mysql_query(
        "SELECT unit, (num_unit - num_finished) AS in_training FROM recruit WHERE villageid='" . $vid . "'",
        $conn
    );
    if ($_rq) {
        while ($_rr = mysql_fetch_assoc($_rq)) {
            $pop += max(0, intval($_rr['in_training'])) * (isset($_pc[$_rr['unit']]) ? $_pc[$_rr['unit']] : 1);
        }
    }

    // Also count troops currently away (in-flight movements) — they still consume farm space.
    // Without this, villages with large outgoing/returning stacks appear to have free farm
    // and keep queuing units, causing total troops to far exceed the farm cap.
    $_aq = mysql_query(
        "SELECT units FROM movements WHERE from_village='" . $vid . "' AND end_time > " . $now,
        $conn
    );
    if ($_aq) {
        // units string format: spear;sword;axe;archer;spy;light;marcher;heavy;ram;cat;knight;noble;miliz
        $_away_pc = array(1,1,1,1,2,4,5,6,5,8,10,100,1); // pop costs per position
        while ($_amr = mysql_fetch_row($_aq)) {
            $_parts = explode(';', $_amr[0]);
            for ($_pi = 0; $_pi < count($_parts) && $_pi < count($_away_pc); $_pi++) {
                $pop += intval($_parts[$_pi]) * $_away_pc[$_pi];
            }
        }
    }

    $free_pop = $farm_cap - $pop;
    if ($free_pop < 1) {
        return array('info', '[' . $vname . '] RECRUIT: Farm full (' . $pop . '/' . $farm_cap . ')');
    }

    superbot_ensure_unit_place($vid, $conn);

    // Total troops OWNED by this village — home + stationed at siblings via support.
    // This prevents double-queuing: if Blaives sent 20k axe as support to Beowulfs,
    // those rows exist in unit_place with from=Blaives, to=Beowulfs. We must count
    // them as "have" so Blaives doesn't think it has 0 axe and queue another 20k.
    $tq = mysql_query(
        "SELECT unit_spear, unit_sword, unit_axe, unit_spy, unit_light," .
        " unit_heavy, unit_ram, unit_catapult, unit_archer, unit_marcher, unit_snob" .
        " FROM unit_place" .
        " WHERE villages_from_id='" . $vid . "'",
        $conn
    );
    $home = array(
        'u_spear'=>0,'u_sword'=>0,'u_axe'=>0,'u_spy'=>0,'u_light'=>0,
        'u_heavy'=>0,'u_ram'=>0,'u_catapult'=>0,'u_archer'=>0,'u_marcher'=>0,
        'u_snob'=>0
    );
    $col_to_ukey = array(
        'unit_spear'=>'u_spear','unit_sword'=>'u_sword','unit_axe'=>'u_axe',
        'unit_spy'=>'u_spy','unit_light'=>'u_light','unit_heavy'=>'u_heavy',
        'unit_ram'=>'u_ram','unit_catapult'=>'u_catapult',
        'unit_archer'=>'u_archer','unit_marcher'=>'u_marcher',
        'unit_snob'=>'u_snob'
    );
    if ($tq) {
        $ckeys = array_keys($col_to_ukey);
        while ($tr = mysql_fetch_assoc($tq)) {
            for ($ci = 0; $ci < count($ckeys); $ci++) {
                $ck = $ckeys[$ci];
                if (isset($tr[$ck])) $home[$col_to_ukey[$ck]] += intval($tr[$ck]);
            }
        }
    }

    // Add in-queue units so deficit calc doesn't double-queue
    $iq = mysql_query(
        "SELECT unit, (num_unit - num_finished) AS in_flight FROM recruit" .
        " WHERE villageid='" . $vid . "' AND time_finished > " . $now,
        $conn
    );
    if ($iq) {
        while ($ir = mysql_fetch_assoc($iq)) {
            $uc = $ir['unit']; // e.g. 'unit_axe'
            $n  = max(0, intval($ir['in_flight']));
            if (isset($col_to_ukey[$uc]) && isset($home[$col_to_ukey[$uc]])) {
                $home[$col_to_ukey[$uc]] += $n;
            }
        }
    }

    $targets = superbot_get_army_target($profile, $village, $COL);

    // --- Save-stall tracking ---
    // Each tick where build returns 'wait' (SAVING) increments save_ticks on this village.
    // We read and use this below to throttle recruitment when we've been stuck too long.
    @mysql_query("ALTER TABLE bot_villages ADD COLUMN save_ticks INT DEFAULT 0", $conn);
    @mysql_query("ALTER TABLE bot_villages ADD COLUMN save_target VARCHAR(50) DEFAULT ''", $conn);

    $vid_int  = intval($village['id']);
    $uid_int  = intval($village['userid']);
    $bv_q = mysql_query(
        "SELECT save_ticks, save_target FROM bot_villages" .
        " WHERE vid='" . $vid_int . "' AND uid='" . $uid_int . "'",
        $conn
    );
    $save_ticks  = 0;
    $save_target = '';
    if ($bv_q && mysql_num_rows($bv_q) > 0) {
        $bv_r        = mysql_fetch_assoc($bv_q);
        $save_ticks  = intval($bv_r['save_ticks']);
        $save_target = $bv_r['save_target'];
    }

    // Determine what the build action is currently saving for so we can
    // increment the stall counter if it hasn't changed since last tick.
    $current_save_key = '';
    $build_plan = superbot_get_build_plan($profile);
    for ($bpi = 0; $bpi < count($build_plan); $bpi++) {
        $bkey     = $build_plan[$bpi][0];
        $btgt_lv  = $build_plan[$bpi][1];
        if (!isset($COL[$bkey])) continue;
        $db_col   = $COL[$bkey];
        $cur_lv   = isset($village[$db_col]) ? intval($village[$db_col]) : 0;
        if ($cur_lv >= $btgt_lv) continue;
        if (!isset($BUILDINGS[$bkey])) continue;
        $bspec    = $BUILDINGS[$bkey];
        if ($cur_lv >= $bspec['max']) continue;
        $prereq_ok = true;
        $rkeys2 = array_keys($bspec['req']);
        for ($ri = 0; $ri < count($rkeys2); $ri++) {
            $rk2 = $rkeys2[$ri]; $rlv2 = $bspec['req'][$rk2];
            $rc2 = isset($COL[$rk2]) ? $COL[$rk2] : null;
            $have2 = ($rc2 && isset($village[$rc2])) ? intval($village[$rc2]) : 0;
            if ($have2 < $rlv2) { $prereq_ok = false; break; }
        }
        if (!$prereq_ok) continue;
        $next_cost = superbot_building_cost($bspec, $cur_lv);
        // Only mark as "actively saving" if we're actually short on at least one resource
        if ($wood < $next_cost[0] || $stone < $next_cost[1] || $iron < $next_cost[2]) {
            $current_save_key = $bkey . '_' . ($cur_lv + 1);
        }
        // Reserve exact cost, capped at 50% of current stock
        $reserve_w = min((int)($wood  * 0.5), $next_cost[0]);
        $reserve_s = min((int)($stone * 0.5), $next_cost[1]);
        $reserve_i = min((int)($iron  * 0.5), $next_cost[2]);
        break;
    }

    // Update save_ticks counter: increment if stuck on same target, reset otherwise.
    // Capped at 20 — beyond that the throttle is already at minimum (0.10) and
    // further growth just means a resource-poor village can never rebuild troops.
    if ($current_save_key != '' && $current_save_key == $save_target) {
        $save_ticks = min(20, $save_ticks + 1);
        mysql_query(
            "UPDATE bot_villages SET save_ticks=" . $save_ticks .
            " WHERE vid='" . $vid_int . "' AND uid='" . $uid_int . "'",
            $conn
        );
    } elseif ($current_save_key != $save_target) {
        // Either finished saving (build fired) or moved to new target — reset
        $save_ticks = 0;
        mysql_query(
            "UPDATE bot_villages SET save_ticks=0, save_target='" .
            mysql_real_escape_string($current_save_key) . "'" .
            " WHERE vid='" . $vid_int . "' AND uid='" . $uid_int . "'",
            $conn
        );
    }

    // --- Troop:Points ratio governor + phase multiplier ---
    // Compute current troop farm usage and village points, then get a combined
    // multiplier that throttles recruitment based on phase, ratio, and save stall.
    $village_pts  = isset($village[$COL['points']]) ? intval($village[$COL['points']]) : 0;
    $troop_farm   = superbot_troop_farm_pop($vid_int, $conn);
    $recruit_mult = superbot_recruit_multiplier($village_pts, $troop_farm, $save_ticks);

    // Available for recruitment = (stock minus reservation) × recruit_mult
    $avail_w = (int)floor(max(0, $wood  - $reserve_w) * $recruit_mult);
    $avail_s = (int)floor(max(0, $stone - $reserve_s) * $recruit_mult);
    $avail_i = (int)floor(max(0, $iron  - $reserve_i) * $recruit_mult);

    // --- Three independent building queues ---
    $queued = array();

    // BARRACKS: spear, sword, axe, archer
    // Candidate priority: axe first (best off unit in barracks), then spear (cheap wood filler),
    // then sword (high iron), then archer. Matches human early-game preference.
    $brk_lv = isset($village[$COL['b_barracks']]) ? intval($village[$COL['b_barracks']]) : 0;
    if ($brk_lv >= 1) {
        $brk_candidates = array('u_axe', 'u_spear', 'u_sword', 'u_archer');
        $r = superbot_queue_building(
            $vid, $vname, 'b_barracks', $COL['b_barracks'],
            $brk_candidates, $targets, $home,
            $village, $COL, $UNITS,
            $wood, $stone, $iron, $free_pop,
            $world_speed, $now, $conn,
            $avail_w, $avail_s, $avail_i
        );
        if ($r != '') {
            $queued[] = 'BRK:' . $r;
            // Update available resources for next building check
            // (re-read from DB to get accurate remaining values)
            $rq2 = mysql_query("SELECT r_wood, r_stone, r_iron, `" . $COL['r_pop'] . "` AS pop FROM villages WHERE id='" . $vid . "'", $conn);
            if ($rq2) {
                $rf2 = mysql_fetch_assoc($rq2);
                $wood    = (int)round($rf2['r_wood']);
                $stone   = (int)round($rf2['r_stone']);
                $iron    = (int)round($rf2['r_iron']);
                $free_pop = $farm_cap - intval($rf2['pop']);
                // Recompute available after reservation (reservation amount unchanged)
                $avail_w = max(0, $wood  - $reserve_w);
                $avail_s = max(0, $stone - $reserve_s);
                $avail_i = max(0, $iron  - $reserve_i);
            }
        }
    }

    // STABLE: light cav (priority), scout, mounted archer, heavy cav
    $stb_lv = isset($village[$COL['b_stable']]) ? intval($village[$COL['b_stable']]) : 0;
    if ($stb_lv >= 1) {
        $stb_candidates = array('u_light', 'u_spy', 'u_marcher', 'u_heavy');
        $r = superbot_queue_building(
            $vid, $vname, 'b_stable', $COL['b_stable'],
            $stb_candidates, $targets, $home,
            $village, $COL, $UNITS,
            $wood, $stone, $iron, $free_pop,
            $world_speed, $now, $conn,
            $avail_w, $avail_s, $avail_i
        );
        if ($r != '') {
            $queued[] = 'STB:' . $r;
            $rq2 = mysql_query("SELECT r_wood, r_stone, r_iron, `" . $COL['r_pop'] . "` AS pop FROM villages WHERE id='" . $vid . "'", $conn);
            if ($rq2) {
                $rf2 = mysql_fetch_assoc($rq2);
                $wood    = (int)round($rf2['r_wood']);
                $stone   = (int)round($rf2['r_stone']);
                $iron    = (int)round($rf2['r_iron']);
                $free_pop = $farm_cap - intval($rf2['pop']);
                $avail_w = max(0, $wood  - $reserve_w);
                $avail_s = max(0, $stone - $reserve_s);
                $avail_i = max(0, $iron  - $reserve_i);
            }
        }
    }

    // WORKSHOP: ram, catapult
    $grg_lv = isset($village[$COL['b_garage']]) ? intval($village[$COL['b_garage']]) : 0;
    if ($grg_lv >= 1) {
        $grg_candidates = array('u_ram', 'u_catapult');
        $r = superbot_queue_building(
            $vid, $vname, 'b_garage', $COL['b_garage'],
            $grg_candidates, $targets, $home,
            $village, $COL, $UNITS,
            $wood, $stone, $iron, $free_pop,
            $world_speed, $now, $conn,
            $avail_w, $avail_s, $avail_i
        );
        if ($r != '') $queued[] = 'WRK:' . $r;
    }

    // ACADEMY (snob): nobleman — highest priority end-game unit.
    // Hard cap: maximum 1 nobleman per village at any time.
    // Check all three locations before training: home garrison, recruit queue,
    // and troops currently away on movements (attacks/returns).
    $snb_lv = isset($village[$COL['b_snob']]) ? intval($village[$COL['b_snob']]) : 0;
    $snb_in_target = isset($targets['u_snob']) && $targets['u_snob'] > 0;
    $snb_have = isset($home['u_snob']) ? $home['u_snob'] : 0;
    if ($snb_lv >= 1 && $snb_in_target && $snb_have < $targets['u_snob']) {
        // Count nobles in recruit queue (training but not yet delivered)
        $snb_queue_count = 0;
        $snbqq = mysql_query(
            "SELECT SUM(num_unit - num_finished) AS q FROM recruit" .
            " WHERE villageid='" . $vid . "' AND unit='" . $COL['u_snob'] . "'" .
            "   AND time_finished > " . $now,
            $conn
        );
        if ($snbqq && ($snbqr = mysql_fetch_assoc($snbqq))) {
            $snb_queue_count = intval($snbqr['q']);
        }
        // Count nobles currently away on movements (position 11 in the units string,
        // format: spear;sword;axe;archer;spy;light;marcher;heavy;ram;cat;knight;noble;miliz)
        $snb_away_count = 0;
        $snbaq = mysql_query(
            "SELECT units FROM movements WHERE from_village='" . $vid . "' AND end_time > " . $now,
            $conn
        );
        if ($snbaq) {
            while ($snbar = mysql_fetch_assoc($snbaq)) {
                $uparts = explode(';', $snbar['units']);
                // Noble is at index 11 (0-based)
                if (isset($uparts[11])) $snb_away_count += max(0, intval($uparts[11]));
            }
        }
        $snb_total = $snb_have + $snb_queue_count + $snb_away_count;

        if ($snb_total < 1 && $free_pop >= 100) {
            $uspec_snob = $UNITS['u_snob'];
            // Use full stock minus building reserve (ignore recruit_mult — noble wins)
            $snb_avail_w = max(0, $wood  - $reserve_w);
            $snb_avail_s = max(0, $stone - $reserve_s);
            $snb_avail_i = max(0, $iron  - $reserve_i);
            $can_afford  = ($snb_avail_w >= $uspec_snob['cost'][0])
                        && ($snb_avail_s >= $uspec_snob['cost'][1])
                        && ($snb_avail_i >= $uspec_snob['cost'][2]);
            if ($can_afford) {
                // Train exactly 1 noble
                $snb_cost_w = $uspec_snob['cost'][0]; // 28000
                $snb_cost_s = $uspec_snob['cost'][1]; // 30000
                $snb_cost_i = $uspec_snob['cost'][2]; // 25000
                // Noble train time ~ 7 days base / world_speed
                $snb_base_secs = 604800; // 7 days
                $snb_train_secs = max(10, (int)round($snb_base_secs / max(1, $world_speed)));
                $snb_end = $now + $snb_train_secs;
                $snb_col = $COL['u_snob'];
                mysql_query(
                    "UPDATE villages SET r_wood=r_wood-" . $snb_cost_w .
                    ", r_stone=r_stone-" . $snb_cost_s .
                    ", r_iron=r_iron-"   . $snb_cost_i .
                    ", `" . $COL['r_pop'] . "`=`" . $COL['r_pop'] . "`+100" .
                    " WHERE id='" . $vid . "'",
                    $conn
                );
                mysql_query(
                    "INSERT INTO recruit (unit, num_unit, num_finished, last_reload, time_finished, time_start, time_per_unit, building, villageid)" .
                    " VALUES ('" . $snb_col . "', 1, 0, " . $now . ", " . $snb_end . "," .
                    "  " . $now . ", " . $snb_train_secs . ", '" . $snb_bld_col . "', '" . $vid . "')",
                    $conn
                );
                $eta_label = $snb_train_secs >= 3600
                    ? (int)ceil($snb_train_secs / 3600) . 'h'
                    : (int)ceil($snb_train_secs / 60) . 'min';
                $queued[] = 'SNB:+1 noble (ETA ' . $eta_label . ')';
            } else {
                // Report what we're short on
                $short = array();
                if ($snb_avail_w < $uspec_snob['cost'][0]) $short[] = ($uspec_snob['cost'][0] - $snb_avail_w) . 'W';
                if ($snb_avail_s < $uspec_snob['cost'][1]) $short[] = ($uspec_snob['cost'][1] - $snb_avail_s) . 'S';
                if ($snb_avail_i < $uspec_snob['cost'][2]) $short[] = ($uspec_snob['cost'][2] - $snb_avail_i) . 'I';
                $queued[] = 'SNB:SAVING need ' . implode(' ', $short) . ' more';
            }
        }
    }

    if (count($queued) == 0) {
        // Check if army is complete
        $total_have = 0; $total_need = 0;
        $tkeys = array_keys($targets);
        for ($i = 0; $i < count($tkeys); $i++) {
            $total_have += isset($home[$tkeys[$i]]) ? $home[$tkeys[$i]] : 0;
            $total_need += $targets[$tkeys[$i]];
        }
        if ($total_need > 0 && $total_have >= $total_need) {
            return array('info', '[' . $vname . '] RECRUIT: Army complete (' . $total_have . ' troops)');
        }
        $mult_pct = (int)round($recruit_mult * 100);
        $stall_info = $save_ticks >= 3 ? ' stall=' . $save_ticks : '';
        return array('info', '[' . $vname . '] RECRUIT: All queues busy or saving resources [mult=' . $mult_pct . '%' . $stall_info . ']');
    }

    $mult_pct = (int)round($recruit_mult * 100);
    $mult_suffix = ($recruit_mult < 0.99) ? ' [throttle=' . $mult_pct . '%]' : '';
    return array('recruit', '[' . $vname . '] ' . implode(' | ', $queued) . $mult_suffix);
}


/* =========================================================
   SECTION 15: ATTACK ACTION

   Smart attack logic with proper cooldown, loss memory, and strength estimation.

   1. Cooldown stored in bot_villages.last_attack_time (DB-persisted).
   2. Win/loss memory: after 2 losses backs off for 4 hours, picks new target.
   3. Pre-flight strength check vs actual garrison before sending.
   4. Noble: if a nobleman is home, he is included in the attack wave.
      Only 1 nobleman can exist per village at a time (enforced in recruit).

   REQUIRES (auto-added if missing):
     bot_villages.last_attack_time  INT DEFAULT 0
     bot_villages.attack_losses     INT DEFAULT 0
     bot_villages.target_vid        INT DEFAULT 0
     bot_villages.last_attack_ratio FLOAT DEFAULT 0
   ========================================================= */

function superbot_attack_min_send($target_pts) {
    if ($target_pts < 100)  return 50;
    if ($target_pts < 300)  return 150;
    if ($target_pts < 600)  return 400;
    if ($target_pts < 1000) return 800;
    if ($target_pts < 2000) return 1500;
    if ($target_pts < 3500) return 3000;
    if ($target_pts < 5000) return 5000;
    return 7000;
}

function superbot_estimate_target_defence($tid, $tpts, $conn) {
    $dq = mysql_query(
        "SELECT unit_spear, unit_sword, unit_archer, unit_heavy, unit_light, unit_marcher, unit_spy" .
        " FROM unit_place WHERE villages_from_id='" . intval($tid) . "'" .
        " AND villages_to_id='" . intval($tid) . "' LIMIT 1",
        $conn
    );
    if ($dq && mysql_num_rows($dq) > 0) {
        $dr = mysql_fetch_assoc($dq);
        return intval($dr['unit_spear'])   * 25 +
               intval($dr['unit_sword'])   * 50 +
               intval($dr['unit_archer'])  * 50 +
               intval($dr['unit_heavy'])   * 40 +
               intval($dr['unit_light'])   * 15 +
               intval($dr['unit_marcher']) * 30 +
               intval($dr['unit_spy'])     * 2;
    }
    return $tpts * 8;
}

function superbot_wave_offence($ax, $li, $hv, $rm, $sy, $sp, $sw, $arc, $ma) {
    return $ax*40 + $li*130 + $hv*150 + $rm*2 + $sy*1 + $sp*10 + $sw*15 + $arc*15 + $ma*30;
}

function superbot_action_attack($village, $COL, $ATTACK_CONFIG, $profile, $world_speed, $now, $conn) {
    $vid   = intval($village['id']);
    $uid   = intval($village['userid']);
    $vname = $village['name'];
    $vx    = intval($village['x']);
    $vy    = intval($village['y']);

    superbot_ensure_unit_place($vid, $conn);
    $tr = mysql_query(
        "SELECT unit_axe, unit_light, unit_heavy, unit_ram, unit_spy," .
        " unit_spear, unit_sword, unit_archer, unit_marcher, unit_snob" .
        " FROM unit_place" .
        " WHERE villages_from_id='" . $vid . "' AND villages_to_id='" . $vid . "'",
        $conn
    );
    if (!$tr || mysql_num_rows($tr) == 0) {
        return array('info', '[' . $vname . '] ATTACK SKIP: no troop row');
    }
    $t   = mysql_fetch_assoc($tr);
    $ax  = intval($t['unit_axe']);
    $li  = intval($t['unit_light']);
    $hv  = intval($t['unit_heavy']);
    $rm  = intval($t['unit_ram']);
    $sy  = intval($t['unit_spy']);
    $sp  = intval($t['unit_spear']);
    $sw  = intval($t['unit_sword']);
    $arc = intval($t['unit_archer']);
    $ma  = intval($t['unit_marcher']);
    $snb = intval($t['unit_snob']);

    $off_count = ($profile == 'deff')
        ? $ax+$li+$hv+$rm+$sy+$sp+$sw+$arc+$ma
        : $ax+$li+$hv+$rm+$sy+$ma;

    if ($profile == 'deff')       $profile_min = 6000;
    elseif ($profile == 'off')    $profile_min = 500;
    else                          $profile_min = 350;

    // GATE 0: Only one outgoing attack allowed at a time.
    // If any attack movement is already in-flight from this village, skip.
    $ifc = mysql_query(
        "SELECT COUNT(*) AS c FROM movements" .
        " WHERE from_village='" . $vid . "' AND type='attack' AND end_time > " . $now,
        $conn
    );
    if ($ifc) {
        $ifr = mysql_fetch_assoc($ifc);
        if (intval($ifr['c']) > 0) {
            return array('info', '[' . \$vname . '] ATTACK SKIP: attack already in-flight');
        }
    }

    // ── Add required columns (safe if already exist) ──────────────────────────
    @mysql_query("ALTER TABLE bot_villages ADD COLUMN last_attack_time INT DEFAULT 0", $conn);
    @mysql_query("ALTER TABLE bot_villages ADD COLUMN attack_losses INT DEFAULT 0", $conn);
    @mysql_query("ALTER TABLE bot_villages ADD COLUMN last_attack_ratio FLOAT DEFAULT 0", $conn);

    $bvq = mysql_query(
        "SELECT target_vid, last_attack_time, attack_losses, last_attack_ratio" .
        " FROM bot_villages WHERE vid='" . $vid . "' AND uid='" . $uid . "'",
        $conn
    );
    $target_vid        = 0;
    $last_attack_time  = 0;
    $attack_losses     = 0;
    $last_attack_ratio = 0.0;
    if ($bvq && mysql_num_rows($bvq) > 0) {
        $bvr               = mysql_fetch_assoc($bvq);
        $target_vid        = intval($bvr['target_vid']);
        $last_attack_time  = intval($bvr['last_attack_time']);
        $attack_losses     = intval($bvr['attack_losses']);
        $last_attack_ratio = (float)$bvr['last_attack_ratio'];
    }

    // ── Noble ─────────────────────────────────────────────────────────────────
    // If a nobleman is home, send him with the attack. The strength check (GATE 3)
    // already ensures the target is weak enough to win cleanly.
    // ─────────────────────────────────────────────────────────────────────────

    // ── GATE 1: Minimum troops ────────────────────────────────────────────────
    if ($off_count < $profile_min) {
        return array('info',
            '[' . $vname . '] ATTACK SKIP: ' . $off_count . ' troops (need ' . $profile_min . ')'
        );
    }
    // ── GATE 2: Dynamic outcome-based cooldown ────────────────────────────────
    // Cooldown scales with how dominant the last attack was (stored ratio).
    $max_cooldown = isset($ATTACK_CONFIG['cooldown_seconds']) ? intval($ATTACK_CONFIG['cooldown_seconds']) : 3600;

    // Compute dynamic cooldown from last attack's strength ratio
    if ($last_attack_ratio >= 3.0) {
        $cooldown = max(60,  (int)($max_cooldown * 0.017));  // ~60s regardless of max
    } elseif ($last_attack_ratio >= 2.0) {
        $cooldown = max(300, (int)($max_cooldown * 0.083));  // ~5 min at default 1h
    } elseif ($last_attack_ratio >= 1.5) {
        $cooldown = max(900, (int)($max_cooldown * 0.25));   // ~15 min
    } elseif ($last_attack_ratio >= 1.0) {
        $cooldown = (int)($max_cooldown * 0.50);             // ~30 min
    } else {
        $cooldown = $max_cooldown;                           // Full cooldown for losses
    }

    if ($last_attack_time > 0 && ($now - $last_attack_time) < $cooldown) {
        $remaining = $cooldown - ($now - $last_attack_time);
        $cd_label  = $remaining >= 60 ? (int)ceil($remaining / 60) . 'min' : $remaining . 's';
        return array('info',
            '[' . $vname . '] ATTACK SKIP: cooldown ' . $cd_label .
            ($last_attack_ratio > 0 ? ' (ratio ' . number_format($last_attack_ratio, 1) . ')' : '')
        );
    }

    $loss_backoff = 4 * 3600;
    if ($attack_losses >= 2) {
        if (($now - $last_attack_time) < $loss_backoff) {
            $hrs = (int)ceil(($loss_backoff - ($now - $last_attack_time)) / 3600);
            return array('info',
                '[' . $vname . '] ATTACK SKIP: backed off after losses (' . $hrs . 'h remaining)'
            );
        }
        mysql_query(
            "UPDATE bot_villages SET target_vid=0, attack_losses=0" .
            " WHERE vid='" . $vid . "' AND uid='" . $uid . "'",
            $conn
        );
        $target_vid = 0; $attack_losses = 0;
    }

    if ($profile == 'off')      $chance_key = 'chance_off';
    elseif ($profile == 'deff') $chance_key = 'chance_deff';
    else                        $chance_key = 'chance_offdeff';
    $attack_chance = isset($ATTACK_CONFIG[$chance_key]) ? (float)$ATTACK_CONFIG[$chance_key] : 0.50;
    if (mt_rand(0, 999) >= (int)($attack_chance * 1000)) {
        return array('info', '[' . $vname . '] ATTACK SKIP: chance roll');
    }

    $target = null;
    if ($target_vid > 0) {
        $tv_q = mysql_query(
            "SELECT id, userid, name, x, y, `" . $COL['points'] . "` AS pts" .
            " FROM villages WHERE id='" . $target_vid . "'",
            $conn
        );
        if ($tv_q && mysql_num_rows($tv_q) > 0) {
            $tv = mysql_fetch_assoc($tv_q);
            if (intval($tv['userid']) == $uid) {
                mysql_query(
                    "UPDATE bot_villages SET target_vid=0, attack_losses=0" .
                    " WHERE vid='" . $vid . "' AND uid='" . $uid . "'", $conn
                );
                return array('info', '[' . $vname . '] ATTACK: ' . $tv['name'] . ' CONQUERED!');
            }
            $target = $tv;
        } else {
            mysql_query(
                "UPDATE bot_villages SET target_vid=0, attack_losses=0" .
                " WHERE vid='" . $vid . "' AND uid='" . $uid . "'", $conn
            );
            $target_vid = 0;
        }
    }

    if ($target === null) {
        // Pick the single closest enemy village by Euclidean distance.
        $tq = mysql_query(
            "SELECT id, userid, name, x, y, `" . $COL['points'] . "` AS pts," .
            " (POW(x-" . $vx . ",2) + POW(y-" . $vy . ",2)) AS dsq" .
            " FROM villages WHERE userid != '" . $uid . "' AND userid != 0" .
            " ORDER BY dsq ASC LIMIT 1",
            $conn
        );
        if (!$tq || mysql_num_rows($tq) == 0) {
            return array('info', '[' . $vname . '] ATTACK: No player targets found');
        }
        $target     = mysql_fetch_assoc($tq);
        $target_vid = intval($target['id']);
        mysql_query(
            "UPDATE bot_villages SET target_vid=" . $target_vid . ", attack_losses=0" .
            " WHERE vid='" . $vid . "' AND uid='" . $uid . "'", $conn
        );
        $attack_losses = 0;
    }

    $tid     = intval($target['id']);
    $tuid    = intval($target['userid']);
    $tgt_pts = intval($target['pts']);

    $min_for_target = superbot_attack_min_send($tgt_pts);
    if ($off_count < $min_for_target) {
        return array('info',
            '[' . $vname . '] ATTACK SKIP: ' . $off_count . ' troops, need ' .
            $min_for_target . ' for ' . $target['name'] . ' (' . $tgt_pts . 'pts)'
        );
    }

    // Send the entire home garrison — no fraction, all troops go.
    // For deff profile: send all unit types. For off/offdeff: offensive only.
    if ($profile == 'deff') {
        $send_ax = $ax; $send_li = $li; $send_hv = $hv; $send_rm = $rm;
        $send_sy = $sy; $send_sp = $sp; $send_sw = $sw; $send_arc = $arc; $send_ma = $ma;
    } else {
        $send_ax = $ax; $send_li = $li; $send_hv = $hv; $send_rm = $rm;
        $send_sy = $sy; $send_sp = 0;   $send_sw = 0;   $send_arc = 0;   $send_ma = $ma;
    }
    $send_snob = $snb; // Send noble if one is home

    $send_total = $send_ax+$send_li+$send_hv+$send_rm+$send_sy+
                  $send_sp+$send_sw+$send_arc+$send_ma+$send_snob;
    if ($send_total < 5) {
        return array('info', '[' . $vname . '] ATTACK SKIP: no troops to send');
    }

    $wave_off   = superbot_wave_offence(
        $send_ax, $send_li, $send_hv, $send_rm,
        $send_sy, $send_sp, $send_sw, $send_arc, $send_ma
    );
    $target_def = superbot_estimate_target_defence($tid, $tgt_pts, $conn);
    $min_ratio  = isset($ATTACK_CONFIG['min_strength_ratio']) ? (float)$ATTACK_CONFIG['min_strength_ratio'] : 0.60;

    if ($target_def > 0 && $wave_off < ($target_def * $min_ratio)) {
        // Store ratio < 1.0 so full loss cooldown applies next time
        $fail_ratio = min(9.9, $wave_off / (float)$target_def);
        mysql_query(
            "UPDATE bot_villages SET attack_losses = attack_losses + 1," .
            " last_attack_time=" . $now .
            ", last_attack_ratio=" . number_format($fail_ratio, 4, '.', '') .
            " WHERE vid='" . $vid . "' AND uid='" . $uid . "'",
            $conn
        );
        return array('info',
            '[' . $vname . '] ATTACK SKIP: too weak (off ' . $wave_off .
            ' vs def ' . $target_def . ' on ' . htmlspecialchars($target['name']) . ')'
        );
    }

    $distance = sqrt(pow($target['x'] - $vx, 2) + pow($target['y'] - $vy, 2));
    $spd = 9;
    if ($send_sp  > 0) $spd = max($spd, 18);
    if ($send_sw  > 0) $spd = max($spd, 22);
    if ($send_ax  > 0) $spd = max($spd, 18);
    if ($send_arc > 0) $spd = max($spd, 18);
    if ($send_sy  > 0) $spd = max($spd, 9);
    if ($send_li  > 0) $spd = max($spd, 10);
    if ($send_ma  > 0) $spd = max($spd, 10);
    if ($send_hv  > 0) $spd = max($spd, 11);
    if ($send_rm  > 0) $spd = max($spd, 30);
    if ($send_snob> 0) $spd = max($spd, 35);
    $jitter_max  = isset($ATTACK_CONFIG['travel_jitter']) ? intval($ATTACK_CONFIG['travel_jitter']) : 120;
    $jitter      = ($jitter_max > 0) ? mt_rand(0, $jitter_max) : 0;
    $base_travel = max(30, (int) round(($distance * $spd * 60) / max(1, $world_speed)));
    $travel      = $base_travel + $jitter;
    $end_time    = $now + $travel;

    $unit_str = $send_sp  . ';' . $send_sw  . ';' . $send_ax  . ';' . $send_arc . ';' .
                $send_sy  . ';' . $send_li  . ';' . $send_ma  . ';' . $send_hv  . ';' .
                $send_rm  . ';0;0;' . $send_snob;

    $ok = mysql_query(
        "INSERT INTO movements" .
        " (from_village, to_village, units, type, start_time, end_time, building," .
        "  from_userid, to_userid, to_hidden, wood, stone, iron," .
        "  send_from_village, send_from_user, send_to_user, send_to_village, die)" .
        " VALUES" .
        " ('" . $vid . "', '" . $tid . "', '" . $unit_str . "', 'attack'," .
        "  " . $now . ", " . $end_time . ", NULL," .
        "  '" . $uid . "', '" . $tuid . "', 0, 0, 0, 0," .
        "  '" . $vid . "', '" . $uid . "', '" . $tuid . "', '" . $tid . "', 0)",
        $conn
    );
    if (!$ok) return array('error', '[' . $vname . '] ATTACK ERROR: ' . mysql_error($conn));

    $mov_id = mysql_insert_id($conn);
    mysql_query(
        "INSERT INTO events (event_time, event_type, event_id, user_id, villageid," .
        " knot_event, cid, can_knot, is_locked)" .
        " VALUES (" . $end_time . ", 'movement', " . $mov_id . ", '" . $uid . "', '" . $vid . "'," .
        " '', '0', '0', '')",
        $conn
    );

    $subs = array();
    if ($send_ax  > 0) $subs[] = 'unit_axe     = GREATEST(0, unit_axe     - ' . $send_ax  . ')';
    if ($send_li  > 0) $subs[] = 'unit_light   = GREATEST(0, unit_light   - ' . $send_li  . ')';
    if ($send_hv  > 0) $subs[] = 'unit_heavy   = GREATEST(0, unit_heavy   - ' . $send_hv  . ')';
    if ($send_rm  > 0) $subs[] = 'unit_ram     = GREATEST(0, unit_ram     - ' . $send_rm  . ')';
    if ($send_sy  > 0) $subs[] = 'unit_spy     = GREATEST(0, unit_spy     - ' . $send_sy  . ')';
    if ($send_sp  > 0) $subs[] = 'unit_spear   = GREATEST(0, unit_spear   - ' . $send_sp  . ')';
    if ($send_sw  > 0) $subs[] = 'unit_sword   = GREATEST(0, unit_sword   - ' . $send_sw  . ')';
    if ($send_arc > 0) $subs[] = 'unit_archer  = GREATEST(0, unit_archer  - ' . $send_arc . ')';
    if ($send_ma  > 0) $subs[] = 'unit_marcher = GREATEST(0, unit_marcher - ' . $send_ma  . ')';
    if ($send_snob> 0) $subs[] = 'unit_snob    = GREATEST(0, unit_snob    - ' . $send_snob. ')';
    if (count($subs) > 0) {
        mysql_query("UPDATE unit_place SET " . implode(', ', $subs) .
            " WHERE villages_from_id='" . $vid . "' AND villages_to_id='" . $vid . "'", $conn);
    }

    // Deduct the sent troop population from r_bh so when the game engine adds
    // them back on return, r_bh stays balanced. Without this, the self-heal
    // (now removed) was writing a low r_bh, then the game engine incremented
    // on return causing double-counted population (60k+ farm usage).
    $pop_cost = $send_ax*1 + $send_li*4 + $send_hv*6 + $send_rm*5 + $send_sy*2
              + $send_sp*1 + $send_sw*1 + $send_arc*1 + $send_ma*4 + $send_snob*100;
    if ($pop_cost > 0) {
        mysql_query(
            "UPDATE villages SET `" . $COL['r_pop'] . "` = GREATEST(0, `" . $COL['r_pop'] . "` - " . $pop_cost . ")" .
            " WHERE id='" . $vid . "'",
            $conn
        );
    }

    // Persist cooldown timestamp — critical at speed 1000 where movement rows are
    // deleted seconds after insertion. Without this, cooldown was always 0.
    // Also store the wave_off/target_def ratio so the dynamic cooldown gate can
    // give a short cooldown after dominant wins and a long one after close fights.
    $ratio_store = $target_def > 0 ? min(9.9, $wave_off / (float)$target_def) : 9.9;
    mysql_query(
        "UPDATE bot_villages SET last_attack_time=" . $now .
        ", attack_losses=0" .
        ", last_attack_ratio=" . number_format($ratio_store, 4, '.', '') .
        " WHERE vid='" . $vid . "' AND uid='" . $uid . "'",
        $conn
    );

    return array('attack',
        '[' . $vname . '] ATTACK -> ' . $target['name'] .
        ' (' . $target['x'] . '|' . $target['y'] . ')' .
        ' [ax:' . $send_ax . ' lc:' . $send_li . ' rm:' . $send_rm . ']' .
        ' off:' . $wave_off . '/def:' . $target_def .
        ($send_snob > 0 ? ' +NOBLE' : '') .
        ' ETA ' . $travel . 's'
    );
}


/* =========================================================
   SECTION 16: CROSS-VILLAGE SUPPORT

   When a bot player owns more than one village and one of those villages
   has an incoming enemy attack, another of their villages (with spare
   defensive troops) will send a support movement to reinforce it.

   This mirrors real player behaviour: a multi-village player watching
   their second village get farmed would immediately send deff support
   from their main base to shore it up.

   LOGIC:
   1. For each village being ticked, check if any OTHER village owned by
      the same player has an incoming enemy attack (movements.type='attack',
      to_village = sibling_vid, end_time > now, from_userid != uid).
   2. If yes, and this village has defensive troops above its own local
      minimum reserve (30% of its deff target), send the surplus as support.
   3. Support cooldown: stored in bot_villages.last_support_time per village.
      Won't re-send support for the same threatened village until the current
      support wave has returned (end_time of existing support movement).
   4. Send only pure defensive units: spear, sword, archer.
      Axe/LC/ram stay home (they're offensive and waste deff slots).
   5. Movement type = 'support' — game engine parks these as defenders
      and returns them home after the battle automatically.

   REQUIRES (auto-added if missing):
     bot_villages.last_support_time INT DEFAULT 0
     bot_villages.support_target_vid INT DEFAULT 0
   ========================================================= */

function superbot_action_support($village, $COL, $uid, $world_speed, $now, $conn) {
    $vid   = intval($village['id']);
    $vname = $village['name'];
    $vx    = intval($village['x']);
    $vy    = intval($village['y']);

    // Only villages that have defensive troops can support
    // (deff and offdeff profiles both hold spear/sword)
    @mysql_query("ALTER TABLE bot_villages ADD COLUMN last_support_time INT DEFAULT 0", $conn);
    @mysql_query("ALTER TABLE bot_villages ADD COLUMN support_target_vid INT DEFAULT 0", $conn);

    // Read support state for this village
    $sq = mysql_query(
        "SELECT last_support_time, support_target_vid FROM bot_villages" .
        " WHERE vid='" . $vid . "' AND uid='" . $uid . "'",
        $conn
    );
    $last_support_time  = 0;
    $support_target_vid = 0;
    if ($sq && mysql_num_rows($sq) > 0) {
        $sr = mysql_fetch_assoc($sq);
        $last_support_time  = intval($sr['last_support_time']);
        $support_target_vid = intval($sr['support_target_vid']);
    }

    // If we already have a support movement in flight to our target, don't send another
    if ($support_target_vid > 0) {
        $in_flight = mysql_query(
            "SELECT COUNT(*) AS c FROM movements" .
            " WHERE from_village='" . $vid . "' AND to_village='" . $support_target_vid . "'" .
            " AND type='support' AND end_time > " . $now,
            $conn
        );
        if ($in_flight) {
            $ifr = mysql_fetch_assoc($in_flight);
            if (intval($ifr['c']) > 0) {
                return array('info', '[' . $vname . '] SUPPORT: wave already in flight to vid=' . $support_target_vid);
            }
        }
        // Support wave has returned — clear the target
        mysql_query(
            "UPDATE bot_villages SET support_target_vid=0" .
            " WHERE vid='" . $vid . "' AND uid='" . $uid . "'",
            $conn
        );
        $support_target_vid = 0;
    }

    // Find all other villages owned by the same player
    $siblings_q = mysql_query(
        "SELECT id, name, x, y FROM villages WHERE userid='" . $uid . "' AND id != '" . $vid . "'",
        $conn
    );
    if (!$siblings_q || mysql_num_rows($siblings_q) == 0) {
        return array('info', '[' . $vname . '] SUPPORT: only village, no siblings');
    }
    $siblings = array();
    while ($s = mysql_fetch_assoc($siblings_q)) $siblings[] = $s;

    // Find which sibling has the most threatening inbound attack
    $threatened_vid  = 0;
    $threatened_name = '';
    $threatened_x    = 0;
    $threatened_y    = 0;
    $earliest_hit    = PHP_INT_MAX;

    foreach ($siblings as $sib) {
        $sib_vid = intval($sib['id']);
        $aq = mysql_query(
            "SELECT end_time FROM movements" .
            " WHERE to_village='" . $sib_vid . "'" .
            "   AND type='attack'" .
            "   AND end_time > " . $now .
            "   AND from_userid != '" . $uid . "'" .
            " ORDER BY end_time ASC LIMIT 1",
            $conn
        );
        if ($aq && mysql_num_rows($aq) > 0) {
            $ar = mysql_fetch_assoc($aq);
            $hit_time = intval($ar['end_time']);
            if ($hit_time < $earliest_hit) {
                $earliest_hit    = $hit_time;
                $threatened_vid  = $sib_vid;
                $threatened_name = $sib['name'];
                $threatened_x    = intval($sib['x']);
                $threatened_y    = intval($sib['y']);
            }
        }
    }

    if ($threatened_vid == 0) {
        return array('info', '[' . $vname . '] SUPPORT: no siblings under attack');
    }

    // Check how much time we have — troops must arrive before the attack lands
    $dist         = sqrt(pow($threatened_x - $vx, 2) + pow($threatened_y - $vy, 2));
    // Spear speed = 18 min/tile base (slowest deff unit we'd send)
    $travel_secs  = max(10, (int)round(($dist * 18 * 60) / max(1, $world_speed)));
    $arrival_time = $now + $travel_secs;

    if ($arrival_time >= $earliest_hit) {
        return array('info',
            '[' . $vname . '] SUPPORT: too far — troops arrive ' .
            ($arrival_time - $earliest_hit) . 's after attack hits ' . $threatened_name
        );
    }

    // Read home garrison of THIS village
    superbot_ensure_unit_place($vid, $conn);
    $tq = mysql_query(
        "SELECT unit_spear, unit_sword, unit_archer FROM unit_place" .
        " WHERE villages_from_id='" . $vid . "' AND villages_to_id='" . $vid . "'",
        $conn
    );
    if (!$tq || mysql_num_rows($tq) == 0) {
        return array('info', '[' . $vname . '] SUPPORT: no troop row');
    }
    $tr  = mysql_fetch_assoc($tq);
    $sp  = intval($tr['unit_spear']);
    $sw  = intval($tr['unit_sword']);
    $arc = intval($tr['unit_archer']);

    // Reserve 40% of each for own village defence — only send the surplus
    $reserve_ratio = 0.40;
    $send_sp  = max(0, (int)floor($sp  * (1.0 - $reserve_ratio)));
    $send_sw  = max(0, (int)floor($sw  * (1.0 - $reserve_ratio)));
    $send_arc = max(0, (int)floor($arc * (1.0 - $reserve_ratio)));

    $total_sending = $send_sp + $send_sw + $send_arc;
    if ($total_sending < 100) {
        return array('info',
            '[' . $vname . '] SUPPORT: not enough spare deff (' . $total_sending . ' available)'
        );
    }

    // Build unit string: spear, sword, axe, archer, spy, LC, HC, ram, cat, knight, noble, marcher, miliz
    // Positions:           0      1     2    3       4    5   6   7    8    9       10     11       12
    $unit_str = $send_sp . ';' . $send_sw . ';0;' . $send_arc . ';0;0;0;0;0;0;0;0;0';

    $tuid_q = mysql_query(
        "SELECT userid FROM villages WHERE id='" . $threatened_vid . "'", $conn
    );
    $tuid = $uid; // support goes to own village — userid is same
    if ($tuid_q && ($tuid_r = mysql_fetch_assoc($tuid_q))) $tuid = intval($tuid_r['userid']);

    $end_time = $now + $travel_secs;

    $ok = mysql_query(
        "INSERT INTO movements" .
        " (from_village, to_village, units, type, start_time, end_time, building," .
        "  from_userid, to_userid, to_hidden, wood, stone, iron," .
        "  send_from_village, send_from_user, send_to_user, send_to_village, die)" .
        " VALUES" .
        " ('" . $vid . "', '" . $threatened_vid . "', '" . $unit_str . "', 'support'," .
        "  " . $now . ", " . $end_time . ", NULL," .
        "  '" . $uid . "', '" . $tuid . "', 0, 0, 0, 0," .
        "  '" . $vid . "', '" . $uid . "', '" . $tuid . "', '" . $threatened_vid . "', 0)",
        $conn
    );
    if (!$ok) return array('error', '[' . $vname . '] SUPPORT ERROR: ' . mysql_error($conn));

    $mov_id = mysql_insert_id($conn);
    mysql_query(
        "INSERT INTO events (event_time, event_type, event_id, user_id, villageid," .
        " knot_event, cid, can_knot, is_locked)" .
        " VALUES (" . $end_time . ", 'movement', " . $mov_id . ", '" . $uid . "', '" . $vid . "'," .
        " '', '0', '0', '')",
        $conn
    );

    // Deduct sent units from home garrison
    $subs = array();
    if ($send_sp  > 0) $subs[] = 'unit_spear  = GREATEST(0, unit_spear  - ' . $send_sp  . ')';
    if ($send_sw  > 0) $subs[] = 'unit_sword  = GREATEST(0, unit_sword  - ' . $send_sw  . ')';
    if ($send_arc > 0) $subs[] = 'unit_archer = GREATEST(0, unit_archer - ' . $send_arc . ')';
    mysql_query("UPDATE unit_place SET " . implode(', ', $subs) .
        " WHERE villages_from_id='" . $vid . "' AND villages_to_id='" . $vid . "'", $conn);

    // Deduct pop from r_bh (same as attack — game engine will re-add on return)
    $pop_cost = $send_sp * 1 + $send_sw * 1 + $send_arc * 1;
    mysql_query(
        "UPDATE villages SET `" . $COL['r_pop'] . "` = GREATEST(0, `" . $COL['r_pop'] . "` - " . $pop_cost . ")" .
        " WHERE id='" . $vid . "'",
        $conn
    );

    // Record support state
    mysql_query(
        "UPDATE bot_villages SET last_support_time=" . $now .
        ", support_target_vid=" . $threatened_vid .
        " WHERE vid='" . $vid . "' AND uid='" . $uid . "'",
        $conn
    );

    $secs_before_hit = $earliest_hit - $arrival_time;
    return array('support',
        '[' . $vname . '] SUPPORT -> ' . $threatened_name .
        ' [sp:' . $send_sp . ' sw:' . $send_sw . ' arc:' . $send_arc . ']' .
        ' ETA ' . $travel_secs . 's (arrives ' . $secs_before_hit . 's before attack)'
    );
}

/* =========================================================
   SECTION 17: VILLAGE TICK

   Processes a single village. Called by bot.php which handles
   round-robin selection across all users and all villages.

   Parameters:
     $vid          - village id to process
     $uid          - owner user id
     $total        - total villages across all bot users (for log display)
     $idx          - current round-robin index (for log display)
     $fire_*       - boolean: should this action fire this tick?
     $world_speed  - game world speed setting
     $conn         - active DB connection
   ========================================================= */

function superbot_tick_village($vid, $uid, $total, $idx,
                                $COL, $BUILDINGS, $UNITS, $ATTACK_CONFIG,
                                $fire_build, $fire_research, $fire_recruit, $fire_attack,
                                $world_speed, $conn) {

    $now = time();
    $vid = intval($vid);
    $uid = intval($uid);

    // STEP 1: Maintain fake session so the game engine also processes this user's events
    superbot_maintain_session($uid, $conn);

    // STEP 2: Process any completed builds/recruits/research ourselves.
    // The game engine only does this when a user is logged in. Since NPC users
    // are never logged in, we must do it ourselves before reading any state.
    $completed = superbot_process_events($vid, $COL, $BUILDINGS, $UNITS, $now, $conn);

    // STEP 3: Apply lazy production (resource accumulation since last tick)
    superbot_apply_production($vid, $COL, $now, $conn);

    // STEP 4: Sync r_bh (population used) from ground truth every tick.
    // Recomputes from unit_place home troops + recruit queue — self-healing
    // after any SQL reset or game engine drift. Keeps recruit throttle accurate.
    superbot_sync_rbh($vid, $COL, $conn);

    // Read fresh village row (includes bot_villages profile if set)
    $vq = mysql_query(
        "SELECT v.*, bv.type AS bot_profile" .
        " FROM villages v" .
        " LEFT JOIN bot_villages bv ON bv.vid = v.id AND bv.uid = '" . $uid . "'" .
        " WHERE v.id = '" . $vid . "'",
        $conn
    );
    if (!$vq || mysql_num_rows($vq) == 0) {
        return '[bot] Village ' . $vid . ' not found' . "\n";
    }
    $village = mysql_fetch_assoc($vq);
    $vname   = $village['name'];

    // Profile: use bot_villages.type if set, else offdeff
    $profile = 'offdeff';
    if (isset($village['bot_profile']) && $village['bot_profile'] != '') {
        $profile = $village['bot_profile'];
    }

    $parts = array();

    if ($fire_build) {
        if (superbot_build_queue_clear($vid, $now, $conn)) {
            $r = superbot_action_build($village, $COL, $BUILDINGS, $profile, $now, $conn);
        } else {
            $r = array('info', 'build queue busy');
        }
        $parts[] = 'BUILD: ' . $r[1];
    }

    if ($fire_research) {
        $r = superbot_action_research($village, $COL, $UNITS, $now, $conn);
        $parts[] = 'RESEARCH: ' . $r[1];
    }

    if ($fire_recruit) {
        $r = superbot_action_recruit($village, $COL, $UNITS, $BUILDINGS, $profile, $world_speed, $now, $conn);
        $parts[] = 'RECRUIT: ' . $r[1];
    }

    if ($fire_attack) {
        // Support check: if this player owns multiple villages and a sibling is
        // under attack, send defensive reinforcements before our own attack.
        $profile_supports = ($profile == 'deff' || $profile == 'offdeff');
        if ($profile_supports) {
            $r_sup = superbot_action_support($village, $COL, $uid, $world_speed, $now, $conn);
            if ($r_sup[0] === 'support') {
                $parts[] = 'SUPPORT: ' . $r_sup[1];
            }
        }

        $r = superbot_action_attack($village, $COL, $ATTACK_CONFIG, $profile, $world_speed, $now, $conn);
        $parts[] = 'ATTACK: ' . $r[1];
    }

    $log = '[' . date('H:i:s') . '] ' . $vname .
           ' (v' . ($idx + 1) . '/' . $total . ' | ' . $profile . ')' . "\n";

    // Show completed events first
    if (count($completed) > 0) {
        for ($i = 0; $i < count($completed); $i++) {
            $log .= '  >> ' . $completed[$i] . "\n";
        }
    }

    if (count($parts) == 0) {
        $log .= '  (no actions fired this tick)' . "\n";
    } else {
        for ($i = 0; $i < count($parts); $i++) {
            $log .= '  ' . $parts[$i] . "\n";
        }
    }

    return $log;
}
?>
