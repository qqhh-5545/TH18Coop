#ifndef TH18_COOP_API_H

#define TH18_COOP_API_H

#include <stdint.h>

#include <stddef.h>

#define COOP_VERSION "0.20.0"

#define COOP_HUD_PLAYERS 3

#define COOP_HUD_CARDS 64

typedef struct {int id,selected,remaining,duration,category;uint32_t native_ui;} HudCard;

typedef struct {

    int local_player,shared_keyboard,player_count;

    int lives[3],bombs[3],power[3];

    int funds[3],card_count[3],shop_owner;

    HudCard cards[3][COOP_HUD_CARDS];

    float x[3],y[3];

    int state[3];

    int ghost[3],life_taps[3],power_taps[3],sprite_file[3],sprite[3];

    int life_frag[3],bomb_frag[3];

} HudStats;

void coop_hud_stats(HudStats *stats);

void coop_menu_hud(int owner);

int coop_view_player(void);

int coop_shared_keyboard(void);

typedef struct {float x[4],y[4],u[4],v[4];uint32_t color;int file,sprite,additive;} HudNativeQuad;

unsigned coop_hud_card_quads(const HudCard *card,float x,float y,float width,float height,HudNativeQuad *out,unsigned capacity);

void coop_hud_card_flush(void);

int coop_hud_capture_path(wchar_t *path,unsigned length);

#endif

