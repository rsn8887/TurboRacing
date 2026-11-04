#include "track.h"
#include <cmath>
#include <random>

const SpriteInfo SPRITE_INFO[SPR_COUNT] = {
    {3000, 0.17},  // PALM (hit width: the trunk, the bitmap is wide for the fronds)
    {2800, 0.30},  // PINE
    {2800, 0.30},  // PINE_SNOW
    { 800, 0.70},  // BUSH
    {1700, 0.45},  // CACTUS
    { 900, 0.80},  // ROCK
    {2700, 0.12},  // LAMP
    {2000, 0.90},  // BILLBOARD1
    {2000, 0.90},  // BILLBOARD2
    {2000, 0.90},  // BILLBOARD3
    {6500, 0.95},  // BUILDING1
    {5200, 0.95},  // BUILDING2
    {1300, 0.60},  // SNOWMAN
    {1300, 0.80},  // TURN_LEFT
    {1300, 0.80},  // TURN_RIGHT
    {2160, 0.00},  // GANTRY_START (posts handled separately)
    {2160, 0.00},  // GANTRY_CHECK
    {1500, 0.80},  // SIGN_JUMP
    {1500, 0.80},  // SIGN_NARROW
    {1500, 0.80},  // SIGN_TUNNEL
    {1500, 0.80},  // SIGN_DIRT
};

const char* STAGE_NAMES[NUM_STAGES] = {
    "COCONUT COAST", "DESERT CANYON", "NEON NIGHT", "ALPINE PASS", "MISTY FOREST", "SUNSET BOULEVARD"
};

namespace {
constexpr int SHORT = 25, MEDIUM = 50, LONG = 100;
constexpr double EASY = 2, MED = 4, HARD = 6;
constexpr double LOW = 20, MID = 40, HIGH = 60;

double easeIn(double a, double b, double t)    { return a + (b - a) * t * t; }
double easeInOut(double a, double b, double t) { return a + (b - a) * ((-std::cos(t * M_PI) / 2) + 0.5); }

Theme makeTheme(int stage) {
    Theme t{};
    t.name = STAGE_NAMES[stage];
    t.lane = rgb(240, 240, 240);
    t.rumbleA = rgb(220, 20, 20); t.rumbleB = rgb(240, 240, 240);
    switch (stage) {
    case 0: // sunny coast - the colors of the original test1.png/test2.png
        t.skyTop = rgb(40, 110, 230); t.skyBottom = rgb(170, 215, 255); t.horizonGround = rgb(0, 160, 40);
        t.grassLight = rgb(0, 255, 32); t.grassDark = rgb(0, 200, 32);
        t.roadLight = rgb(128, 128, 128); t.roadDark = rgb(120, 120, 120);
        t.background = 0; break;
    case 1: // desert
        t.skyTop = rgb(60, 120, 210); t.skyBottom = rgb(250, 200, 140); t.horizonGround = rgb(200, 150, 90);
        t.grassLight = rgb(232, 196, 120); t.grassDark = rgb(216, 176, 100);
        t.roadLight = rgb(140, 125, 115); t.roadDark = rgb(130, 115, 105);
        t.rumbleB = rgb(250, 220, 60);
        t.background = 1; t.sun = true; break;
    case 2: // night city
        t.skyTop = rgb(10, 5, 35); t.skyBottom = rgb(90, 30, 110); t.horizonGround = rgb(25, 25, 40);
        t.grassLight = rgb(45, 45, 60); t.grassDark = rgb(38, 38, 52);
        t.roadLight = rgb(70, 70, 82); t.roadDark = rgb(62, 62, 74);
        t.rumbleA = rgb(255, 40, 200); t.rumbleB = rgb(40, 220, 255); t.lane = rgb(230, 230, 120);
        t.background = 2; t.stars = true; t.night = true; break;
    case 3: // snow
        t.skyTop = rgb(120, 150, 190); t.skyBottom = rgb(215, 225, 240); t.horizonGround = rgb(230, 235, 245);
        t.fogColor = rgb(215, 225, 240); t.fogDensity = 0.000035;
        t.grassLight = rgb(250, 250, 255); t.grassDark = rgb(225, 232, 245);
        t.roadLight = rgb(118, 122, 130); t.roadDark = rgb(108, 112, 120);
        t.background = 3; break;
    case 4: // fog forest
        t.skyTop = rgb(110, 125, 120); t.skyBottom = rgb(170, 180, 175); t.horizonGround = rgb(60, 90, 60);
        t.fogColor = rgb(170, 180, 175); t.fogDensity = 0.00009;
        t.grassLight = rgb(40, 120, 40); t.grassDark = rgb(30, 100, 35);
        t.roadLight = rgb(100, 100, 100); t.roadDark = rgb(92, 92, 92);
        t.background = 4; break;
    default: // sunset
        t.skyTop = rgb(40, 20, 90); t.skyBottom = rgb(255, 120, 60); t.horizonGround = rgb(120, 60, 60);
        t.grassLight = rgb(210, 120, 60); t.grassDark = rgb(190, 100, 50);
        t.roadLight = rgb(96, 80, 96); t.roadDark = rgb(88, 72, 88);
        t.rumbleB = rgb(255, 220, 120);
        t.background = 5; t.sun = true; break;
    }
    // dirt, gravel shoulders, guardrails and default kerb style
    t.dirtLight = rgb(176, 132, 84); t.dirtDark = rgb(160, 118, 74);
    t.shoulder = rgb(150, 140, 120); t.rail = rgb(190, 196, 206);
    t.kerb = KERB_RUMBLE;
    switch (stage) {
    case 1: t.dirtLight = rgb(206, 150, 92); t.dirtDark = rgb(190, 136, 80); t.shoulder = rgb(214, 178, 120); break;
    case 2: t.kerb = KERB_ZEBRA; t.shoulder = rgb(60, 60, 74); break;
    case 3: t.dirtLight = rgb(150, 140, 130); t.dirtDark = rgb(136, 126, 118); t.shoulder = rgb(200, 205, 215); t.kerb = KERB_SOLID; break;
    case 4: t.dirtLight = rgb(130, 98, 64); t.dirtDark = rgb(116, 86, 56); t.shoulder = rgb(110, 100, 80); t.kerb = KERB_SOLID; break;
    case 5: t.shoulder = rgb(200, 150, 110); break;
    default: break;
    }
    // tunnel looks: concrete by default, rock faces in the countryside
    t.wallA = rgb(176, 168, 146); t.wallB = rgb(140, 132, 114);
    t.ceilA = rgb(62, 60, 58);    t.ceilB = rgb(50, 48, 46);
    t.lamp = rgb(255, 236, 170);
    t.facade = rgb(140, 120, 100); t.facadeCap = rgb(40, 140, 50); t.rockFacade = true;
    switch (stage) {
    case 1: t.wallA = rgb(200, 140, 92); t.wallB = rgb(168, 112, 72); t.facade = rgb(196, 110, 66); t.facadeCap = rgb(220, 160, 100); break;
    case 2: t.wallA = rgb(80, 80, 130); t.wallB = rgb(58, 58, 100); t.ceilA = rgb(36, 36, 56); t.ceilB = rgb(28, 28, 44);
            t.lamp = rgb(120, 240, 255); t.facade = rgb(70, 70, 96); t.facadeCap = rgb(255, 60, 200); t.rockFacade = false; break;
    case 3: t.facade = rgb(130, 136, 150); t.facadeCap = rgb(250, 252, 255); break;
    case 4: t.facade = rgb(110, 110, 100); t.facadeCap = rgb(40, 100, 45); break;
    case 5: t.wallA = rgb(176, 126, 136); t.wallB = rgb(146, 100, 112); t.facade = rgb(150, 90, 90); t.facadeCap = rgb(230, 130, 70); t.rockFacade = false; break;
    default: break;
    }
    return t;
}
} // namespace

void Track::addSegment(double c, double y, double w) {
    Segment s;
    s.index = int(segs.size());
    s.y1 = lastY();
    s.y2 = y;
    s.w1 = lastW();
    s.w2 = w;
    s.curve = c;
    s.tunnel = inTunnel_;
    s.surface = inTunnel_ ? SURF_ASPHALT : surface_;
    s.kerb = inTunnel_ ? KERB_RUMBLE : (surface_ == SURF_DIRT ? KERB_NONE : kerb_);
    segs.push_back(s);
}

void Track::addRoad(int enter, int hold, int leave, double c, double height, double width) {
    double startY = lastY(), startW = lastW();
    double endY = startY + height * SEG_LEN;
    double endW = width > 0 ? width : startW;
    int total = enter + hold + leave;
    auto W = [&](int k) { return easeInOut(startW, endW, double(k + 1) / total); };
    for (int n = 0; n < enter; ++n) addSegment(easeIn(0, c, double(n) / enter), easeInOut(startY, endY, double(n) / total), W(n));
    for (int n = 0; n < hold; ++n)  addSegment(c, easeInOut(startY, endY, double(enter + n) / total), W(enter + n));
    for (int n = 0; n < leave; ++n) addSegment(easeInOut(c, 0, double(n) / leave), easeInOut(startY, endY, double(enter + hold + n) / total), W(enter + hold + n));
}

void Track::width(int n, double w) {
    if (w < lastW() - 0.1) narrows_.push_back(count());
    addRoad(n, n, n, 0, 0, w);
}

void Track::tunnel(int n, double c) {
    tunnels_.push_back(count());
    inTunnel_ = true;
    addRoad(n, n * 30, n, c, 0, 1.0);     // long: a SHORT tunnel is ~800 segments (about 8 s at top speed)
    inTunnel_ = false;
}

void Track::jump(double kick) {
    // kicker: the slope grows steadily to 'kick' (a quadratic ramp), then the road drops straight back
    // to the original level. Fast cars leave the lip with an upward speed of v * kick and fly a free-fall
    // parabola (about 1 s and 100 segments at top speed), landing on the flat road beyond.
    const int n1 = 32, n2 = 14;
    double y0 = lastY(), w = lastW();
    double rampLen = n1 * SEG_LEN, top = y0 + kick * rampLen / 2;
    for (int k = 1; k <= n1; ++k) {
        double z = k * SEG_LEN;
        addSegment(0, y0 + kick * z * z / (2 * rampLen), w);
    }
    jumps_.push_back(count());
    for (int k = 1; k <= n2; ++k) {
        double t = double(k) / n2;
        addSegment(0, top + (y0 - top) * (0.5 - 0.5 * std::cos(t * M_PI)), w);
    }
    straight(25);                      // landing zone (75 segments)
}

void Track::sCurves(int n, double c, double h) {
    addRoad(n, n, n, -c, h * 0.5);
    addRoad(n, n, n,  c, -h * 0.5);
    addRoad(n, n, n,  c * 1.5, 0);
    addRoad(n, n, n, -c, h * 0.3);
    addRoad(n, n, n, -c * 0.7, -h * 0.3);
}

void Track::bumps(double h) {
    // rolling bumps: gentle enough to stay on the road at moderate speed, small hops when flat out
    addRoad(15, 5, 15, 0, h * 0.10);
    addRoad(15, 5, 15, 0, -h * 0.14);
    addRoad(15, 5, 15, 0, h * 0.12);
    addRoad(15, 5, 15, 0, -h * 0.08);
}

void Track::downhillToEnd(int n) {
    if (std::fabs(lastW() - 1) > 0.01) addRoad(n, n, n, 0, 0, 1.0);
    // long enough that the descent back to the start height stays gentle (max slope ~0.12)
    int m = std::max(n, int(std::ceil(std::fabs(lastY()) / (SEG_LEN * 0.12) / 3)));
    addRoad(m, m, m, 0, -lastY() / SEG_LEN);
}

void Track::layout(int st) {
    switch (st) {
    case 0:
        straight(SHORT);
        curve(MEDIUM, EASY);
        hill(MEDIUM, LOW);
        curve(MEDIUM, -MED, -LOW);
        straight(MEDIUM);
        tunnel(SHORT, EASY);                 // short curved tunnel through the headland
        straight(SHORT);
        surface(SURF_DIRT);                  // sandy beach track
        sCurves(SHORT, EASY);
        surface(SURF_ASPHALT);
        curve(LONG, MED, MID);
        bumps(LOW);
        kerb(KERB_GUARDRAIL);
        width(SHORT, 0.65);                  // narrow coastal road along the cliffs
        curve(MEDIUM, -EASY, -MID);
        sCurves(SHORT, EASY);
        kerb(KERB_SOLID);
        width(SHORT, 1.4);                   // opens up into a wide straight
        straight(MEDIUM);
        kerbDefault();
        jump(0.18);
        width(SHORT, 1.0);
        curve(MEDIUM, HARD);
        curve(MEDIUM, -MED, LOW);
        hill(MEDIUM, -LOW);
        sCurves(SHORT, MED, LOW);
        curve(LONG, -EASY);
        break;
    case 1:
        straight(MEDIUM);
        hill(LONG, HIGH);
        curve(MEDIUM, MED, -MID);
        kerb(KERB_SOLID);
        width(MEDIUM, 1.5);                  // wide desert highway
        straight(LONG);
        jump(0.18);
        jump(0.14);
        kerbDefault();
        width(SHORT, 1.0);
        bumps(MID);
        curve(LONG, -EASY, LOW);
        hill(MEDIUM, -HIGH);
        tunnel(SHORT, -MED);                 // canyon tunnel
        surface(SURF_DIRT);                  // dusty canyon track
        curve(MEDIUM, HARD);
        straight(LONG);
        surface(SURF_ASPHALT);
        kerb(KERB_ZEBRA);
        width(SHORT, 0.7);
        curve(MEDIUM, -MED, MID);
        bumps(HIGH);
        kerbDefault();
        width(SHORT, 1.0);
        curve(LONG, EASY, -LOW);
        jump(0.22);
        straight(MEDIUM);
        break;
    case 2:
        straight(SHORT);
        curve(SHORT, HARD);
        tunnel(SHORT);                       // underpass
        straight(SHORT);
        curve(SHORT, -HARD);
        kerb(KERB_SOLID);
        width(SHORT, 1.5);                   // wide boulevard
        straight(MEDIUM);
        sCurves(SHORT, MED);
        kerbDefault();
        width(SHORT, 1.0);
        curve(MEDIUM, MED, LOW);
        tunnel(MEDIUM, EASY);
        curve(SHORT, -HARD, -LOW);
        kerb(KERB_GUARDRAIL);
        width(SHORT, 0.7);                   // back alley between crash barriers
        curve(SHORT, HARD);
        straight(MEDIUM);
        kerbDefault();
        width(SHORT, 1.0);
        jump(0.14);
        straight(MEDIUM);
        sCurves(SHORT, HARD);
        curve(MEDIUM, -MED);
        straight(MEDIUM);
        break;
    case 3:
        straight(SHORT);
        curve(MEDIUM, MED, HIGH);
        tunnel(SHORT, -EASY);
        curve(MEDIUM, -MED, MID);
        kerb(KERB_GUARDRAIL);
        width(SHORT, 0.65);                  // narrow mountain pass with crash barriers
        hill(MEDIUM, -LOW);
        sCurves(SHORT, MED, MID);
        kerbDefault();
        width(SHORT, 1.0);
        curve(LONG, EASY, -HIGH);
        jump(0.18);
        surface(SURF_DIRT);                  // gravel section
        bumps(MID);
        curve(MEDIUM, -HARD, HIGH);
        surface(SURF_ASPHALT);
        tunnel(MEDIUM, MED);                 // long curved mountain tunnel
        curve(MEDIUM, HARD, -MID);
        kerb(KERB_RUMBLE);
        straight(MEDIUM);
        curve(LONG, -MED, -LOW);
        kerbDefault();
        break;
    case 4:
        straight(SHORT);
        surface(SURF_DIRT);                  // forest dirt track
        width(SHORT, 0.7);
        sCurves(SHORT, MED);
        curve(MEDIUM, HARD, LOW);
        curve(MEDIUM, -HARD, -LOW);
        surface(SURF_ASPHALT);
        width(SHORT, 1.0);
        sCurves(SHORT, HARD, LOW);
        jump(0.18);
        kerb(KERB_RUMBLE);
        hill(MEDIUM, MID);
        curve(MEDIUM, MED, -MID);
        kerbDefault();
        tunnel(SHORT, -EASY);
        bumps(LOW);
        surface(SURF_DIRT);
        width(SHORT, 0.6);
        curve(LONG, -MED);
        width(SHORT, 1.0);
        surface(SURF_ASPHALT);
        sCurves(SHORT, MED, -LOW);
        curve(MEDIUM, HARD);
        break;
    default:
        straight(MEDIUM);
        kerb(KERB_GUARDRAIL);
        width(SHORT, 1.5);                   // wide beach highway with crash barriers
        curve(LONG, EASY, LOW);
        straight(LONG);
        kerbDefault();
        width(SHORT, 1.0);
        curve(MEDIUM, -MED, MID);
        bumps(MID);
        tunnel(SHORT, MED);
        curve(MEDIUM, HARD, -MID);
        jump(0.18);
        straight(LONG);
        sCurves(SHORT, MED, LOW);
        kerb(KERB_ZEBRA);
        width(SHORT, 0.7);
        curve(LONG, -EASY, HIGH);
        kerbDefault();
        width(SHORT, 1.0);
        hill(LONG, -HIGH);
        jump(0.14);
        curve(MEDIUM, MED);
        curve(MEDIUM, -HARD, LOW);
        straight(MEDIUM);
        break;
    }
    surface(SURF_ASPHALT);
    kerbDefault();
    downhillToEnd(SHORT);
    // flat, straight run-in to the start line, long enough for the whole staggered grid
    straight(int(std::ceil((GRID_FRONT + (NUM_CARS - 1) * GRID_SLOT) / SEG_LEN / 3)) + 6);
}

void Track::decorate(unsigned seed) {
    std::mt19937 rng(seed);
    auto rnd = [&](double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); };
    auto chance = [&](double p) { return rnd(0, 1) < p; };
    auto side = [&]() { return chance(0.5) ? -1.0 : 1.0; };
    int n = count();
    int checkpoint = int(checkpointZ / SEG_LEN);
    auto nearGantry = [&](int i) { return i < 6 || i > n - 4 || std::abs(i - checkpoint) < 6; };
    auto isTunnel = [&](int i) { return segs[((i % n) + n) % n].tunnel; };
    // offsets are given relative to a standard road; keep the same distance from the edge when the road
    // is narrower or wider. Nothing goes inside tunnels or right in front of a portal.
    auto add = [&](int i, SpriteType t, double off) {
        int k = ((i % n) + n) % n;
        if (isTunnel(k) || isTunnel(k + 1) || isTunnel(k + 2) || isTunnel(k + 3)) return;
        double w = std::max(segs[k].w1, segs[k].w2);
        if (std::fabs(off) >= 1) off = (off < 0 ? -1 : 1) * (w + std::fabs(off) - 1);
        segs[k].sprites.push_back({t, off});
    };

    add(0, SPR_GANTRY_START, 0);
    add(checkpoint, SPR_GANTRY_CHECK, 0);

    // warning signs for jumps, narrow sections and tunnels
    for (int j : jumps_)   for (int k : {48, 30}) { add(j - k, SPR_SIGN_JUMP, -1.5); add(j - k, SPR_SIGN_JUMP, 1.5); }
    for (int j : narrows_) for (int k : {22, 8})  { add(j - k, SPR_SIGN_NARROW, -1.5); add(j - k, SPR_SIGN_NARROW, 1.5); }
    for (int j : tunnels_) for (int k : {40})     { add(j - k, SPR_SIGN_TUNNEL, 1.5); }
    for (int j : dirts_)   for (int k : {30, 12}) { add(j - k, SPR_SIGN_DIRT, -1.5); add(j - k, SPR_SIGN_DIRT, 1.5); }

    // warning signs before sharp curves (on the outside of the bend)
    for (int i = 1; i < n; ++i) {
        double c0 = segs[i - 1].curve, c1 = segs[i].curve;
        if (std::abs(c1) >= 3.5 && std::abs(c0) < 3.5 && std::signbit(c0) == std::signbit(c1)) {
            for (int k = 30; k >= 6; k -= 6) {
                int j = i - k;
                if (j <= 0 || nearGantry(j)) continue;
                add(j, c1 > 0 ? SPR_TURN_RIGHT : SPR_TURN_LEFT, c1 > 0 ? -1.45 : 1.45);
            }
        }
    }

    int billboards[3] = {SPR_BILLBOARD1, SPR_BILLBOARD2, SPR_BILLBOARD3};
    int bb = 0;
    for (int i = 0; i < n; ++i) {
        bool gantry = nearGantry(i);
        bool sign = false;
        for (auto& s : segs[i].sprites)
            sign |= (s.type == SPR_TURN_LEFT || s.type == SPR_TURN_RIGHT || s.type >= SPR_SIGN_JUMP);
        if (segs[i].kerb == KERB_GUARDRAIL) sign = true;    // keep the rail side clear of close scenery
        if (!gantry && !sign && i % 120 == 60) add(i, SpriteType(billboards[bb++ % 3]), side() * rnd(1.9, 2.4));
        switch (stage) {
        case 0:
            if (i % 6 == 0 && !gantry && !sign) { add(i, SPR_PALM, -1.6 - rnd(0, 0.4)); add(i + 3, SPR_PALM, 1.6 + rnd(0, 0.4)); }
            if (chance(0.25)) add(i, SPR_BUSH, side() * rnd(2.6, 6.0));
            if (chance(0.06)) add(i, SPR_PALM, side() * rnd(3.0, 7.0));
            break;
        case 1:
            if (chance(0.12)) add(i, SPR_CACTUS, side() * rnd(1.7, 6.0));
            if (chance(0.10)) add(i, SPR_ROCK, side() * rnd(1.6, 5.0));
            if (i % 40 == 20 && !gantry && !sign) add(i, SPR_ROCK, side() * 1.6);
            break;
        case 2:
            if (i % 8 == 0 && !gantry && !sign) { add(i, SPR_LAMP, -1.35); add(i + 4, SPR_LAMP, 1.35); }
            if (i % 5 == 0) add(i, chance(0.5) ? SPR_BUILDING1 : SPR_BUILDING2, side() * rnd(2.8, 4.5));
            if (chance(0.15)) add(i, chance(0.5) ? SPR_BUILDING1 : SPR_BUILDING2, side() * rnd(5.0, 8.0));
            break;
        case 3:
            if (chance(0.35)) add(i, SPR_PINE_SNOW, side() * rnd(1.7, 7.0));
            if (i % 10 == 0 && !gantry && !sign) add(i, SPR_PINE_SNOW, side() * rnd(1.6, 2.0));
            if (chance(0.02)) add(i, SPR_SNOWMAN, side() * rnd(1.6, 3.0));
            if (chance(0.05)) add(i, SPR_ROCK, side() * rnd(2.0, 5.0));
            break;
        case 4:
            if (chance(0.6)) add(i, SPR_PINE, side() * rnd(1.6, 8.0));
            if (i % 4 == 0 && !gantry && !sign) add(i, SPR_PINE, side() * rnd(1.6, 2.1));
            if (chance(0.2)) add(i, SPR_BUSH, side() * rnd(1.6, 4.0));
            break;
        default:
            if (i % 10 == 0 && !gantry && !sign) { add(i, SPR_PALM, -1.65); add(i, SPR_PALM, 1.65); }
            if (i % 10 == 5 && !gantry && !sign) { add(i, SPR_LAMP, -1.35); add(i, SPR_LAMP, 1.35); }
            if (chance(0.1)) add(i, SPR_BUSH, side() * rnd(2.5, 5.0));
            if (i % 60 == 30 && !gantry && !sign) add(i, SpriteType(billboards[bb++ % 3]), side() * rnd(2.0, 2.4));
            break;
        }
    }
}

void Track::build(int st) {
    stage = st;
    theme = makeTheme(st);
    segs.clear();
    jumps_.clear(); narrows_.clear(); tunnels_.clear(); dirts_.clear();
    surface_ = SURF_ASPHALT;
    kerb_ = theme.kerb;
    layout(st);
    length = count() * SEG_LEN;
    // mid-lap checkpoint: half way round, moved forward out of tunnels and width changes
    int cp = count() / 2;
    while (segs[cp].tunnel || std::fabs(segs[cp].w1 - 1) > 0.02 || std::fabs(segs[cp].w2 - 1) > 0.02 ||
           segs[(cp + 6) % count()].tunnel)
        cp = (cp + 1) % count();
    checkpointZ = cp * SEG_LEN;
    decorate(1234u + unsigned(st) * 777u);
}

double Track::widthAt(double z) const {
    double w = wrap(z);
    const Segment& s = find(w);
    double t = (w - s.index * SEG_LEN) / SEG_LEN;
    return s.w1 + (s.w2 - s.w1) * t;
}

double Track::wallAt(double z) const {
    const Segment& s = find(z);
    if (s.tunnel) return TUNNEL_SIDE * widthAt(z);
    if (s.kerb == KERB_GUARDRAIL) return RAIL_POS * widthAt(z);
    return 0;
}

int Track::checkpointCount(double dist) const {
    double lap = std::floor(dist / length);
    double frac = dist - lap * length;
    return int(lap) * 2 + (frac >= checkpointZ ? 1 : 0);
}

double Track::wrap(double z) const {
    double r = std::fmod(z, length);
    return r < 0 ? r + length : r;
}

const Segment& Track::find(double z) const {
    int i = int(std::floor(wrap(z) / SEG_LEN));
    if (i >= count()) i = count() - 1;
    return segs[i];
}

double Track::heightAt(double z) const {
    double w = wrap(z);
    const Segment& s = find(w);
    double t = (w - s.index * SEG_LEN) / SEG_LEN;
    return s.y1 + (s.y2 - s.y1) * t;
}
