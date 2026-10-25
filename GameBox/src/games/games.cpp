// ---------------------------------------------------------------------------
//  GameBox :: games.cpp
// ---------------------------------------------------------------------------
#include "games.h"

static Game* g_games[8];
static uint8_t g_count = 0;

void registerGames() {
  if (g_count) return;
  g_games[g_count++] = makeGameFish();
  g_games[g_count++] = makeGameSpts();
  g_games[g_count++] = makeGameShot();
  g_games[g_count++] = makeGameRng();
  g_games[g_count++] = makeGameLife();
}

Game* gameAt(uint8_t idx) {
  registerGames();
  if (idx >= g_count) return nullptr;
  return g_games[idx];
}

uint8_t gameCount() {
  registerGames();
  return g_count;
}
