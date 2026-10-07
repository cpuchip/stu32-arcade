// The reference driver: babalcore (fa985cd, read-only) printing what oracle/driver.c prints.
// Floats are printed as their bits. Usage: babal-ref pins | level SEED SKILL N | game SCRIPT
use babalcore::*;
use euclid::default::Vector3D;
use rand::prelude::*;
use rand::rngs::ChaCha20Rng;
use std::fmt::Write as _;
use std::io::Write as _;
use std::time::{Duration, Instant};

struct SplitMix(u64);
impl SplitMix {
    fn next(&mut self) -> u64 {
        self.0 = self.0.wrapping_add(0x9e3779b97f4a7c15);
        let mut z = self.0;
        z = (z ^ (z >> 30)).wrapping_mul(0xbf58476d1ce4e5b9);
        z = (z ^ (z >> 27)).wrapping_mul(0x94d049bb133111eb);
        z ^ (z >> 31)
    }
    fn unit(&mut self) -> f64 {
        (self.next() >> 11) as f64 * (1.0 / 9007199254740992.0)
    }
}

fn b64(x: f64) -> String {
    format!("{:016x}", x.to_bits())
}
fn b32(x: f32) -> String {
    format!("{:08x}", x.to_bits())
}

fn pins(out: &mut String) {
    let mut sm = SplitMix(0x5eed);
    // ChaCha20 f64 streams, seeded as row_generator seeds them.
    let mut seeds: Vec<(u64, u64)> = vec![(0, 0), (0, 1), (123, 0), (u64::MAX, u64::MAX)];
    for _ in 0..60 {
        seeds.push((sm.next(), sm.next() % 20000));
    }
    for (s, j) in seeds {
        let mut buf = [0u8; 32];
        buf[0..8].copy_from_slice(&s.to_le_bytes());
        buf[8..16].copy_from_slice(&j.to_le_bytes());
        let mut rng = ChaCha20Rng::from_seed(buf);
        let _ = write!(out, "chacha {} {}", s, j);
        for _ in 0..70 {
            let f: f64 = rng.random();
            let _ = write!(out, " {}", b64(f));
        }
        out.push('\n');
    }
    // euclid lerp.
    let ts = [0.0, 1.0, 0.5, 0.25, 1.0 / 3.0];
    for k in 0..300 {
        let a = Vector3D::new(sm.unit() * 2000.0 - 1000.0, sm.unit() * 800.0 - 400.0, sm.unit() * 1e6);
        let b = Vector3D::new(sm.unit() * 2000.0 - 1000.0, sm.unit() * 800.0 - 400.0, sm.unit() * 1e6);
        let t = if k < ts.len() { ts[k] } else { sm.unit() };
        let r = a.lerp(b, t);
        let _ = writeln!(out, "lerp {} {} {} {} {} {} {} -> {} {} {}",
            b64(a.x), b64(a.y), b64(a.z), b64(b.x), b64(b.y), b64(b.z), b64(t), b64(r.x), b64(r.y), b64(r.z));
    }
    // Duration::from_secs_f64 and as_secs_f64.
    let mut secs: Vec<f64> = vec![0.16, 0.0, 1e-9, 0.5e-9, 1.5e-9, 2.5e-9, 0.999999999, 1.0, 0.16 * 0.5, 0.16 * 1.5, 0.16 * 3.0,
        1.0 / 1024.0, 3.0 / 1024.0, 5.0 / 1024.0, 7.0 / 1024.0, 1e-10, 3.0 * 1e-10];
    for _ in 0..300 {
        secs.push(sm.unit() * 3.0);
    }
    for _ in 0..100 {
        secs.push((sm.next() % 3_000_000_000) as f64 / 1e9 + (sm.unit() - 0.5) * 1e-9);
    }
    for s in secs {
        if s >= 0.0 {
            let _ = writeln!(out, "from_secs_f64 {} {}", b64(s), Duration::from_secs_f64(s).as_nanos());
        }
    }
    for k in 0..300 {
        let ns: u64 = if k < 5 { [0, 1, 999_999_999, 1_000_000_000, 160_000_000][k] } else { sm.next() % 100_000_000_000_000 };
        let _ = writeln!(out, "as_secs_f64 {} {}", ns, b64(Duration::from_nanos(ns).as_secs_f64()));
    }
    // round, fract, trunc-to-integer, f32 casts.
    let mut xs: Vec<f64> = vec![0.5, -0.5, 1.5, 2.5, -2.5, 0.49999999999999994, 4503599627370495.5, -0.0];
    for _ in 0..300 {
        xs.push((sm.unit() - 0.5) * 2e4);
    }
    for _ in 0..50 {
        xs.push((sm.next() % 20000) as f64 * 0.5 - 5000.0);
    }
    for x in xs {
        let _ = writeln!(out, "num {} round={} fract={} trunc_i={} floor={} f32={}",
            b64(x), b64(x.round()), b64(x.fract()), x as i64, b64(x.floor()), b32(x as f32));
    }
}

fn level(out: &mut String, seed: u64, skill: Skill, n: usize) {
    let mut lvl = Level::new(seed, skill);
    let w = lvl.width();
    let _ = writeln!(out, "level seed={} skill={} width={}", seed, skill.as_str(), w);
    while (lvl.last() as usize) < n {
        let last0 = lvl.last();
        lvl.generate(100);
        // Each new row as Row's Display prints it, read through slab_kind (row relative to first).
        for abs in last0..lvl.last() {
            let rel = abs - lvl.first();
            let text: String = (0..w as isize).map(|c| format!("{}", lvl.slab_kind(c, rel, false))).collect();
            let _ = writeln!(out, "r {} {}", abs, text);
        }
    }
    let _ = writeln!(out, "first={} last={} len={}", lvl.first(), lvl.last(), lvl.len());
    let (first, last) = (lvl.first(), lvl.last());
    let mut rows: Vec<isize> = vec![-3, -1, 0, 1, 2, 3, 50, last / 2];
    for d in -3..3 {
        rows.push(last + d);
        rows.push(first + d);
        rows.push(last - first + d);
    }
    rows.sort();
    rows.dedup();
    for r in rows {
        let mut line = format!("q {}", r);
        for c in -2..(WIDTH_MAX as isize + 2) {
            let _ = write!(line, " {}{}{}", lvl.item(c, r, false), lvl.item(c, r, true),
                lvl.slab_kind(c, r, true).as_item());
        }
        for c in -2..(w as isize + 2) {
            match lvl.find_start_spot(c, r) {
                Some((c2, r2)) => { let _ = write!(line, " ({},{})", c2, r2); }
                None => line.push_str(" none"),
            }
        }
        out.push_str(&line);
        out.push('\n');
    }
    // The rows the deque holds, read directly (Level's Display): past the slide, slab_kind's
    // reads are shifted by row0 (quirk 2), so the "r" lines above do not show these.
    let _ = writeln!(out, "deque first={}", lvl.first());
    let _ = writeln!(out, "{}", lvl);
}

// Every private field of the player, read back from its Debug text (Rust prints an f64 there
// shortest-round-trip, so parsing it back gives the exact bits): floats as bits, integers as
// they are, booleans 1/0, Option as N or S. The scores log (a HashMap, printed in random order)
// is cut out and returned sorted by row.
fn player_state(p: &Player) -> (String, String) {
    let dbg = format!("{:?}", p);
    let a = dbg.find("scores_log: {").expect("scores_log") + "scores_log: {".len();
    let b = a + dbg[a..].find('}').expect("end of scores_log");
    let mut log: Vec<(i64, f64)> = dbg[a..b]
        .split(", ")
        .filter(|e| !e.is_empty())
        .map(|e| {
            let (k, v) = e.split_once(": ").unwrap();
            (k.parse().unwrap(), v.parse().unwrap())
        })
        .collect();
    log.sort_by_key(|e| e.0);
    let rest = format!("{}{}", &dbg[..a], &dbg[b..]);
    let mut toks: Vec<String> = Vec::new();
    for t in rest.split(|c: char| !(c.is_ascii_alphanumeric() || c == '_' || c == '.' || c == '-' || c == '+')) {
        if t.is_empty() {
            continue;
        }
        let first = t.chars().next().unwrap();
        let tok = match t {
            "true" => "1".to_string(),
            "false" => "0".to_string(),
            "None" => "N".to_string(),
            "Some" => "S".to_string(),
            "NaN" | "inf" | "-inf" => b64(t.parse().unwrap()),
            _ if first.is_ascii_digit() || first == '-' => {
                if t.contains('.') || t.contains('e') {
                    b64(t.parse().unwrap())
                } else {
                    t.to_string()
                }
            }
            _ => continue,
        };
        toks.push(tok);
    }
    let log_text: Vec<String> = log.iter().map(|(k, v)| format!("{}:{}", k, b64(*v))).collect();
    (toks.join(","), log_text.join(" "))
}

struct Adapter<'a> {
    steer: &'a mut InputSteer,
    jump: &'a mut InputJump,
    now: Instant,
}
impl InputQuery for Adapter<'_> {
    fn pop_jump(&mut self) -> bool {
        self.jump.pop_jump(Some(self.now))
    }
    fn pop_steer(&mut self) -> f64 {
        self.steer.pop_steer(Some(self.now))
    }
}

fn game(out: &mut String, script: &str) {
    let text = std::fs::read_to_string(script).expect("script");
    let base = Instant::now();
    let mut clock: u64 = 0;
    let mut seed: u64 = 0;
    let mut skill = Skill::default();
    let mut level: Option<Level> = None;
    let mut player = Player::new();
    let mut steer = InputSteer::new();
    let mut jump = InputJump::new();
    let mut ticks = 0usize;
    for raw in text.lines() {
        let f: Vec<&str> = raw.split_whitespace().collect();
        if f.is_empty() || f[0].starts_with('#') {
            continue;
        }
        let now = base + Duration::from_nanos(clock);
        match f[0] {
            "seed" => seed = f[1].parse().unwrap(),
            "skill" => skill = Skill::from_str(f[1]),
            "steer_factor" => player.set_steer_factor(f[1].parse().unwrap()),
            "best" => player.set_best_score(f[1].parse().unwrap()),
            "stick" => jump.set_stickiness_secs_f64(if f[1] == "none" { None } else { Some(f[1].parse().unwrap()) }),
            "sens" => {
                steer.mouse_sensibility = f[1].parse().unwrap();
                steer.keyboard_sensibility = f[2].parse().unwrap();
            }
            "wait" => clock += f[1].parse::<u64>().unwrap(),
            "L" | "R" => {
                let state = f[1] == "1";
                let intensity: f64 = if f.len() > 2 { f[2].parse().unwrap() } else { 1.0 };
                if f[0] == "L" {
                    steer.left_key(1, intensity, state, Some(now));
                } else {
                    steer.right_key(2, intensity, state, Some(now));
                }
            }
            "J" => jump.push_jump(Some(now)),
            "M" => steer.mouse_move(f[1].parse().unwrap()),
            "tick" | "tickd" => {
                let lvl = level.get_or_insert_with(|| Level::new(seed, skill));
                let delta: f64 = if f[0] == "tick" {
                    let ns: u64 = f[1].parse().unwrap();
                    clock += ns;
                    ns as f64 / 1e9
                } else {
                    f[1].parse().unwrap()
                };
                // babalgame's generate_missing: keep 100 rows ahead of the player.
                while lvl.last() < player.level_row() + 100 {
                    lvl.generate(100);
                }
                let mut adapter = Adapter { steer: &mut steer, jump: &mut jump, now: base + Duration::from_nanos(clock) };
                let events = player.tick(delta, lvl, &mut adapter);
                let ev: Vec<String> = events.iter().map(|e| match e.arg {
                    Some(a) => format!("{}:{}", e.kind.as_str(), a),
                    None => e.kind.as_str().to_string(),
                }).collect();
                let (p, s) = (player.position(), player.speed());
                let _ = writeln!(out,
                    "t {} ev={} p={},{},{} s={},{},{} col={} row={} dist={} score={} best_row={} best_dist={} boost={} blend={} sf={} tpd={} tpt={} first={} last={} len={}",
                    ticks, ev.join(","), b32(p.x), b32(p.y), b32(p.z), b32(s.x), b32(s.y), b32(s.z),
                    player.level_col(lvl.width()), player.level_row(), b64(player.dist()), player.score(),
                    player.best_row(), b64(player.best_dist()), player.is_boosting() as u8,
                    b64(player.backing_camera_blend()), b64(player.speed_factor(lvl)),
                    b64(player.time_progress_done(lvl)), b64(player.time_progress_total()),
                    lvl.first(), lvl.last(), lvl.len());
                let _ = writeln!(out, "s {}", player_state(&player).0);
                ticks += 1;
            }
            other => panic!("unknown script word {}", other),
        }
    }
    let _ = writeln!(out, "log {}", player_state(&player).1);
    if let Some(lvl) = level {
        let _ = writeln!(out, "rows first={} last={}", lvl.first(), lvl.last());
        let _ = writeln!(out, "{}", lvl);
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let mut out = String::new();
    match args.get(1).map(|s| s.as_str()) {
        Some("pins") => pins(&mut out),
        Some("level") => level(&mut out, args[2].parse().unwrap(), Skill::from_str(&args[3]), args[4].parse().unwrap()),
        Some("game") => game(&mut out, &args[2]),
        _ => {
            eprintln!("usage: babal-ref pins | level SEED SKILL N | game SCRIPT");
            std::process::exit(2);
        }
    }
    std::io::stdout().write_all(out.as_bytes()).unwrap();
}
