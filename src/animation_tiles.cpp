#include "game.h"

#if (defined SDLTILES)
#   include "debug.h"
#   include "cata_tiles.h" // all animation functions will be pushed out to a cata_tiles function in some manner

extern cata_tiles *tilecontext; // obtained from sdltiles.cpp
extern void try_update();

#endif

namespace {
void draw_animation_delay(long const scale = 1)
{
    auto const delay = static_cast<long>(OPTIONS["ANIMATION_DELAY"]) * scale * 1000000l;

    timespec const ts = {0, delay};
    if (ts.tv_nsec > 0) {
        nanosleep(&ts, nullptr);
    }
}

void draw_explosion_curses(WINDOW *const w, int const x, int const y,
    int const r, nc_color const col)
{
    if (r == 0) { // TODO why not always print '*'?
        mvwputch(w, x, y, col, '*');
    }

    for (int i = 1; i <= r; ++i) {
        mvwputch(w, y - i, x - i, col, '/');  // corner: top left
        mvwputch(w, y - i, x + i, col, '\\'); // corner: top right
        mvwputch(w, y + i, x - i, col, '\\'); // corner: bottom left
        mvwputch(w, y + i, x + i, col, '/');  // corner: bottom right
        for (int j = 1 - i; j < 0 + i; j++) {
            mvwputch(w, y - i, x + j, col, '-'); // edge: top
            mvwputch(w, y + i, x + j, col, '-'); // edge: bottom
            mvwputch(w, y + j, x - i, col, '|'); // edge: left
            mvwputch(w, y + j, x + i, col, '|'); // edge: right
        }
    }
    
    wrefresh(w);
    draw_animation_delay(EXPLOSION_MULTIPLIER);
}
} // namespace

#if !defined(SDLTILES)
void game::draw_explosion(int const x, int const y, int const r, nc_color const col)
{
    const int ypos = POSY + (y - (u.posy() + u.view_offset_y));
    const int xpos = POSX + (x - (u.posx() + u.view_offset_x));
    draw_explosion_curses(w_terrain, xpos, ypos, r, col);
}
#else
void game::draw_explosion(int const x, int const y, int const r, nc_color const col)
{
    // added offset values to keep from calculating the same value over and over again.
    const int ypos = POSY + (y - (u.posy() + u.view_offset_y));
    const int xpos = POSX + (x - (u.posx() + u.view_offset_x));

    if (!use_tiles) {
        draw_explosion_curses(w_terrain, xpos, ypos, r, col);
        return;
    }

    for (int i = 1; i <= r; i++) {
        tilecontext->init_explosion(x, y, i); // TODO not xpos ypos?
        wrefresh(w_terrain);
        try_update();
        draw_animation_delay(EXPLOSION_MULTIPLIER);
    }

    if (r > 0) {
        tilecontext->void_explosion();
    }
}
#endif

namespace {
void draw_bullet_curses(WINDOW *const w, player &u, map &m, int const tx, int const ty,
    char const bullet, point const *const p, bool const wait)
{
    int const posx = u.posx() + u.view_offset_x;
    int const posy = u.posy() + u.view_offset_y;

    if (p) {
        m.drawsq(w, u, p->x, p->y, false, true, posx, posy);
    }

    mvwputch(w, POSY + (ty - posy), POSX + (tx - posx), c_red, bullet);
    wrefresh(w);

    if (wait) {
        draw_animation_delay();
    }
}

} ///namespace

#if !defined(SDLTILES)
void game::draw_bullet(Creature const &p, int const tx, int const ty, int const i,
    std::vector<point> const &trajectory, char const bullet)
{
    if (!u.sees(tx, ty)) {
        return;
    }

    draw_bullet_curses(w_terrain, u, m, tx, ty, bullet,
        (i > 0) ? &trajectory[i - 1] : nullptr, p.is_player());
}
#else
/* Bullet Animation -- Maybe change this to animate the ammo itself flying through the air?*/
// need to have a version where there is no player defined, possibly. That way shrapnel works as intended
void game::draw_bullet(Creature const &p, int const tx, int const ty, int const i,
    std::vector<point> const &trajectory, char const bullet)
{
    //TODO signature and impl could be changed to eliminate these params

    (void)i;          //unused
    (void)trajectory; //unused

    if (!u.sees(tx, ty)) {
        return;
    }

    if (!use_tiles) {
        draw_bullet_curses(w_terrain, u, m, tx, ty, bullet, nullptr, p.is_player());
    }

    static std::string const bullet_unknown  {};
    static std::string const bullet_normal   {"animation_bullet_normal"};
    static std::string const bullet_flame    {"animation_bullet_flame"};
    static std::string const bullet_shrapnel {"animation_bullet_shrapnel"};

    std::string const &bullet_type = 
        (bullet == '*') ? bullet_normal
      : (bullet == '#') ? bullet_flame
      : (bullet == '`') ? bullet_shrapnel
      : bullet_unknown;

    tilecontext->init_draw_bullet(tx, ty, bullet_type);
    wrefresh(w_terrain);

    if (p.is_player()) {
        try_update();
        draw_animation_delay();
    }

    tilecontext->void_bullet();
}
#endif

namespace {
void draw_hit_mon_curses(int const x, int const y, const monster &m, player const& u, bool const dead)
{
    hit_animation(POSX + (x - (u.posx() + u.view_offset_x)),
                  POSY + (y - (u.posy() + u.view_offset_y)),
                  red_background(m.type->color), dead ? "%" : m.symbol());
}

} // namespace

#if !defined(SDLTILES)
void game::draw_hit_mon(int const x, int const y, const monster &m, bool const dead)
{
    draw_hit_mon_curses(x, y, m, u, dead);
}
#else
void game::draw_hit_mon(int const x, int const y, const monster &m, bool const dead)
{
    if (!use_tiles) {
        draw_hit_mon_curses(x, y, m, u, dead);
        return;
    }

    tilecontext->init_draw_hit(x, y, m.type->id);
    wrefresh(w_terrain);
    try_update();
    draw_animation_delay();
}
#endif

namespace {
void draw_hit_player_curses(game const& g, player const &p, const int dam)
{
    nc_color const col = (!dam) ? yellow_background(p.symbol_color())
                                : red_background(p.symbol_color());

    hit_animation(POSX + (p.posx() - (g.u.posx() + g.u.view_offset_x)),
                  POSY + (p.posy() - (g.u.posy() + g.u.view_offset_y)), col, p.symbol());
}
} //namespace

#if !defined(SDLTILES)
void game::draw_hit_player(player const &p, const int dam)
{
    draw_hit_player_curses(*this, p, dam);
}
#else
/* Player hit animation */
void game::draw_hit_player(player const &p, const int dam)
{
    if (!use_tiles) {
        draw_hit_player_curses(*this, p, dam);
        return;
    }

    static std::string const player_male   {"player_male"};
    static std::string const player_female {"player_female"};
    static std::string const npc_male      {"npc_male"};
    static std::string const npc_female    {"npc_female"};

    std::string const& type = p.is_player() ? (p.male ? player_male : player_female)
                                            : (p.male ? npc_male    : npc_female);

    tilecontext->init_draw_hit(p.posx(), p.posy(), type);
    wrefresh(w_terrain);
    try_update();
    draw_animation_delay();
}
#endif

/* Line drawing code, not really an animation but should be separated anyway */

namespace {
void draw_line_curses(game &g, int const x, int const y, point const center,
    std::vector<point> const &ret)
{
    for (point const &p : ret) {
        auto const critter = g.critter_at(p.x, p.y);

        // NPCs and monsters get drawn with inverted colors
        if (critter && g.u.sees(*critter)) {
            critter->draw(g.w_terrain, center.x, center.y, true);
        } else {
            g.m.drawsq(g.w_terrain, g.u, p.x, p.y, true, true, center.x, center.y);
        }
    }
}
} //namespace

#if !defined (SDLTILES)
void game::draw_line(int const x, int const y, point const center, std::vector<point> const &ret)
{
    if (!u.sees(x, y)) {
        return;
    }

    draw_line_curses(*this, x, y, center, ret);
}
#else
void game::draw_line(int const x, int const y, point const center, std::vector<point> const &ret)
{
    if (!u.sees(x, y)) {
        return;
    }

    if (!use_tiles) {
        draw_line_curses(*this, x, y, center, ret); // TODO needed for tiles ver too??
        return;
    }

    tilecontext->init_draw_line(x, y, ret, "line_target", true);
}
#endif

#if (defined SDLTILES)

void game::draw_line(const int x, const int y, std::vector<point> const &vPoint)
{
    int crx = POSX, cry = POSY;

    if(!vPoint.empty()) {
        crx += (vPoint[vPoint.size() - 1].x - (u.posx() + u.view_offset_x));
        cry += (vPoint[vPoint.size() - 1].y - (u.posy() + u.view_offset_y));
    }
    for (point const& p : vPoint) {
        m.drawsq(w_terrain, u, p.x, p.y, true, true);
    }

    mvwputch(w_terrain, cry, crx, c_white, 'X');

    tilecontext->init_draw_line(x, y, vPoint, "line_trail", false);
}

void game::draw_weather(weather_printable wPrint)
{
    if (use_tiles) {
        std::string weather_name;

        switch(wPrint.wtype) {
        // Acid weathers, uses acid droplet tile, fallthrough intended
        case WEATHER_ACID_DRIZZLE:
        case WEATHER_ACID_RAIN:
            weather_name = "weather_acid_drop";
            break;

        // Normal rainy weathers, uses normal raindrop tile, fallthrough intended
        case WEATHER_DRIZZLE:
        case WEATHER_RAINY:
        case WEATHER_THUNDER:
        case WEATHER_LIGHTNING:
            weather_name = "weather_rain_drop";
            break;

        // Snowy weathers, uses snowflake tile, fallthrough intended
        case WEATHER_FLURRIES:
        case WEATHER_SNOW:
        case WEATHER_SNOWSTORM:
            weather_name = "weather_snowflake";
            break;

        default:
            break;
        }

        tilecontext->init_draw_weather(wPrint, weather_name);
    } else {
        for (std::vector<std::pair<int, int> >::iterator weather_iterator = wPrint.vdrops.begin();
             weather_iterator != wPrint.vdrops.end();
             ++weather_iterator) {
            mvwputch(w_terrain, weather_iterator->second, weather_iterator->first, wPrint.colGlyph,
                     wPrint.cGlyph);
        }
    }
}

void game::draw_sct()
{
    if (use_tiles) {
        tilecontext->init_draw_sct();
    } else {
        for (std::vector<scrollingcombattext::cSCT>::iterator iter = SCT.vSCT.begin();
             iter != SCT.vSCT.end(); ++iter) {
            const int iDY = POSY + (iter->getPosY() - (u.posy() + u.view_offset_y));
            const int iDX = POSX + (iter->getPosX() - (u.posx() + u.view_offset_x));

            mvwprintz(w_terrain, iDY, iDX, msgtype_to_color(iter->getMsgType("first"),
                      (iter->getStep() >= SCT.iMaxSteps / 2)), "%s", iter->getText("first").c_str());
            wprintz(w_terrain, msgtype_to_color(iter->getMsgType("second"),
                                                (iter->getStep() >= SCT.iMaxSteps / 2)), iter->getText("second").c_str());
        }
    }
}

void game::draw_zones(const point &p_pointStart, const point &p_pointEnd,
                      const point &p_pointOffset)
{
    if (use_tiles) {
        tilecontext->init_draw_zones(p_pointStart, p_pointEnd, p_pointOffset);
    } else {
        for (int iY = p_pointStart.y; iY <= p_pointEnd.y; ++iY) {
            for (int iX = p_pointStart.x; iX <= p_pointEnd.x; ++iX) {
                mvwputch_inv(w_terrain, iY + p_pointOffset.y, iX + p_pointOffset.x, c_ltgreen, '~');
            }
        }
    }
}
#endif
