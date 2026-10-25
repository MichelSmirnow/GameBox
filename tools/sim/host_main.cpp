// ---------------------------------------------------------------------------
//  GameBox :: host_main.cpp — симулятор для ПК.
//  Прогоняет интерфейс приставки и сохраняет кадры 128x64 в PPM,
//  чтобы проверить вёрстку без железа.
// ---------------------------------------------------------------------------
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <vector>

#include "app.h"
#include "config.h"
#include "host.h"
#include "storage.h"
#include "util.h"

uint32_t g_simMillis = 0;

static std::string outDir = "/tmp/gbshots";
static int shotIndex = 0;
static App* g_app = nullptr;
static std::vector<std::string> manifest;

static void savePPM(const Canvas& c, const std::string& name) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%03d_%s.ppm", outDir.c_str(), shotIndex++, name.c_str());
  FILE* f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P5\n128 64\n255\n");
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 128; x++) fputc(c.get(x, y) ? 255 : 0, f);
  fclose(f);
  manifest.push_back(std::string(path) + "|" + name);
}

// Прокрутить время и отрисовать кадр
static void frames(int n, int stepMs = 20) {
  for (int i = 0; i < n; i++) {
    g_simMillis += stepMs;
    g_app->tick();
  }
}

// ---- проверка наложений текста -------------------------------------------
static int g_overlaps = 0;
static void checkOverlaps(const char* name) {
  g_textBoxes.clear();
  g_logCanvas = &g_app->frame();
  g_textLog = true;
  g_app->tick();                 // один кадр с включённым логом
  g_textLog = false;
  const auto& v = g_textBoxes;
  for (size_t i = 0; i < v.size(); i++) {
    for (size_t j = i + 1; j < v.size(); j++) {
      const TextBox& a = v[i];
      const TextBox& b = v[j];
      int ix = std::min(a.x + a.w, b.x + b.w) - std::max(a.x, b.x);
      int iy = std::min(a.y + a.h, b.y + b.h) - std::max(a.y, b.y);
      if (ix > 1 && iy > 1) {
        printf("  ! %-26s «%s» (%d,%d) x «%s» (%d,%d)\n", name,
               a.s.c_str(), a.x, a.y, b.s.c_str(), b.x, b.y);
        g_overlaps++;
      }
    }
  }
}

static void shot(const char* name) {
  frames(3);
  savePPM(g_app->frame(), name);
  checkOverlaps(name);
}

static void send(Ev e) {
  g_app->inject(e);
  frames(2, 40);
  if (getenv("GB_TRACE")) fprintf(stderr, "  ev %-14s -> screen %d\n", evName(e), (int)g_app->screen());
}

int main(int argc, char** argv) {
  if (argc > 1) outDir = argv[1];
  std::string cmd = "mkdir -p " + outDir;
  if (system(cmd.c_str())) {}

  App app;
  g_app = &app;
  g_simMillis = 1000;
  app.begin();

  // ---------------- загрузка и меню ----------------
  {
    Canvas b;
    app.bootPreview(b, 3);
    savePPM(b, "boot_phase1");
    app.bootPreview(b, 9);
    savePPM(b, "boot_phase2");
  }
  frames(20, 40);
  shot("menu");

  // ---------------- настройки и справка ----------------
  send(EV_DOWN); shot("menu_settings");
  send(EV_SELECT); frames(10, 40); shot("settings_list");
  send(EV_SELECT); frames(10, 40); shot("help_buttons");
  send(EV_DOWN); send(EV_DOWN); frames(6, 40); shot("help_buttons_scrolled");
  send(EV_BACK); frames(6, 40);
  send(EV_DOWN); send(EV_SELECT); frames(10, 40); shot("help_stick");
  send(EV_BACK); frames(6, 40);
  send(EV_DOWN); send(EV_SELECT); frames(10, 40); shot("help_game_list");
  send(EV_SELECT); frames(10, 40); shot("help_game_fish");
  send(EV_BACK); frames(6, 40); send(EV_BACK); frames(6, 40);
  send(EV_DOWN); send(EV_SELECT); frames(8, 40); shot("calibrated_notice");
  send(EV_DOWN); send(EV_SELECT); frames(10, 40); shot("help_about");
  send(EV_BACK); frames(6, 40);
  send(EV_DOWN); send(EV_SELECT); frames(8, 40); shot("confirm_erase");
  send(EV_BACK); frames(6, 40);
  send(EV_BACK); frames(6, 40);         // назад в меню

  // ---------------- карусель игр ----------------
  send(EV_UP); send(EV_UP); send(EV_UP); frames(6, 40);
  send(EV_SELECT); frames(16, 40);
  shot("games_1_fish");
  for (int i = 2; i <= 5; i++) {
    send(EV_RIGHT); frames(14, 40);
    char n[32];
    snprintf(n, sizeof(n), "games_%d", i);
    shot(n);
  }
  send(EV_BACK); frames(10, 40);

  // ---------------- данные для наглядных кадров ----------------
  {
    Store st("gb_fish");
    st.setF("money", 1250000.0f);
    st.setU8("rod", 9);
    st.setU8("zone", 2);
    st.setU8("boats", 0x1F);
    st.setU8("aboat", 3);
    uint16_t inv[44] = {0};
    inv[2] = 40; inv[4] = 12; inv[7] = 25; inv[10] = 6; inv[16] = 3; inv[19] = 2; inv[21] = 1;
    st.setBytes("inv", inv, sizeof(inv));
    uint8_t pots[16] = {3, 1, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0};
    st.setBytes("pot", pots, sizeof(pots));
    Store se("gb_spts");
    double xp[5] = {1e5, 5e5, 2e5, 8e4, 3e4};
    se.setBytes("xp", xp, sizeof(xp));
    se.setU8("unl", 3); se.setU8("zone", 2); se.setU32("tok", 87);
    uint8_t mult[5] = {2, 1, 0, 0, 3};
    se.setBytes("mult", mult, sizeof(mult));
    Store sr("gb_rng");
    sr.setU32("rolls", 4210); sr.setF("dust", 380000.0f); sr.setU8("best", 14);
    sr.setU8("luck", 3); sr.setU8("speed", 2);
    uint8_t found[26] = {1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0};
    sr.setBytes("found", found, sizeof(found));
  }

  auto openGame = [&](int idx, const char* prefix) {
    app.openGameDebug((uint8_t)idx);
    frames(14, 40);
    char n[48];
    snprintf(n, sizeof(n), "%s_enter", prefix);
    shot(n);
  };
  auto exitGame = [&]() { send(EV_BACK); frames(10, 40); };

  // ---------------- рыбалка ----------------
  openGame(0, "fish");
  shot("fish_rod");
  send(EV_SELECT); frames(6, 40); shot("fish_casting");
  // ждём поклёвку (фаза 4 = поклёвка), потом подсекаем
  for (int i = 0; i < 400 && g_app->debugPhase() != 4; i++) frames(1, 40);
  frames(4, 40); shot("fish_bite");
  send(EV_SELECT); frames(4, 40); shot("fish_caught");
  send(EV_RIGHT); frames(14, 40); shot("fish_boats");
  for (int i = 0; i < 8; i++) send(EV_DOWN);
  frames(14, 40); shot("fish_boat_luck");
  for (int i = 0; i < 8; i++) send(EV_DOWN);
  frames(12, 40); shot("fish_boat_luck_upgrade");
  for (int i = 0; i < 16; i++) send(EV_UP);
  frames(10, 40);
  send(EV_RIGHT); frames(14, 40); shot("fish_fridge");
  send(EV_RIGHT); frames(14, 40); shot("fish_potions");
  send(EV_STICK); frames(10, 40); shot("fish_potions_categories");
  send(EV_BACK); frames(8, 40);
  send(EV_RESET); frames(8, 40); shot("fish_board");
  send(EV_RESET); frames(8, 40);
  send(EV_RIGHT); frames(14, 40); shot("fish_zones");
  send(EV_RIGHT); frames(14, 40); shot("fish_prestige");
  for (int i = 0; i < 5; i++) send(EV_BACK);
  frames(14, 40);
  send(EV_DOWN); frames(14, 40); shot("fish_rod_upgrade");
  send(EV_BACK); frames(10, 40);
  exitGame();

  // ---------------- SPTS ----------------
  openGame(1, "spts");
  shot("spts_train");
  send(EV_DOWN); frames(10, 40); shot("spts_train_2");
  send(EV_SELECT); frames(8, 40); shot("spts_burst_notice");
  send(EV_RIGHT); frames(14, 40); shot("spts_zones");
  send(EV_DOWN); frames(10, 40); shot("spts_zone_locked");
  send(EV_STICK); frames(10, 40); shot("spts_zone_categories");
  send(EV_DOWN); frames(6, 40);
  send(EV_SELECT); frames(10, 40); shot("spts_zones_body");
  send(EV_DOWN); frames(8, 40); shot("spts_zones_body2");
  send(EV_SELECT); frames(10, 40); shot("spts_zone_notice");
  send(EV_RIGHT); frames(14, 40); shot("spts_arena");
  send(EV_SELECT); frames(24, 60); shot("spts_battle");
  frames(60, 60); shot("spts_battle_result");
  send(EV_RIGHT); frames(14, 40); shot("spts_shop");
  send(EV_RIGHT); frames(14, 40); shot("spts_stats");
  for (int i = 0; i < 4; i++) send(EV_BACK);
  frames(10, 40);
  exitGame();

  // ---------------- Buckshot ----------------
  openGame(2, "shot");
  shot("shot_intro");
  frames(60, 60); shot("shot_intro_bath");
  send(EV_SELECT); frames(10, 40); shot("shot_disclaimer");
  send(EV_SELECT); frames(8, 40); shot("shot_round_go");
  frames(20, 60); shot("shot_round_shells");
  frames(26, 60); shot("shot_round_lives");
  frames(26, 60); shot("shot_round_number");
  frames(26, 60); shot("shot_play");
  send(EV_SELECT); frames(8, 40); shot("shot_items");
  send(EV_SELECT); frames(8, 40); shot("shot_item_used");
  frames(6, 40);
  send(EV_LEFT); frames(4, 40); shot("shot_target_self");
  send(EV_RIGHT); frames(4, 40);
  send(EV_STICK); frames(12, 60); shot("shot_fire");
  frames(22, 60); shot("shot_result");
  for (int i = 0; i < 10; i++) { frames(26, 60); send(EV_STICK); }
  frames(26, 60); shot("shot_later");
  exitGame();

  // ---------------- RNG ----------------
  openGame(3, "rng");
  shot("rng_roll");
  send(EV_SELECT); frames(4, 30); shot("rng_anim_1");
  frames(8, 30); shot("rng_anim_2");
  frames(30, 40); shot("rng_after");
  send(EV_RIGHT); frames(14, 40); shot("rng_collection");
  send(EV_RIGHT); frames(14, 40); shot("rng_potions");
  send(EV_SELECT); frames(10, 40); shot("rng_potion_used");
  send(EV_RIGHT); frames(14, 40); shot("rng_upgrades");
  send(EV_RIGHT); frames(14, 40); shot("rng_stats");
  send(EV_BACK); frames(10, 40); exitGame();

  // ---------------- Жизнь ----------------
  openGame(4, "life");
  shot("life_editor");
  send(EV_STICK); frames(50, 60); shot("life_sim");
  send(EV_SELECT); frames(6, 40); shot("life_paused");
  send(EV_STICK); frames(6, 40); shot("life_back_to_editor");
  exitGame();

  FILE* mf = fopen((outDir + "/manifest.txt").c_str(), "w");
  for (auto& s : manifest) fprintf(mf, "%s\n", s.c_str());
  fclose(mf);
  printf("кадров: %zu -> %s\n", manifest.size(), outDir.c_str());
  return 0;
}
