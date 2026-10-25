// ---------------------------------------------------------------------------
//  GameBox :: games.h — реестр игр
// ---------------------------------------------------------------------------
#pragma once
#include "../core/game.h"

Game* gameAt(uint8_t idx);
uint8_t gameCount();
void registerGames();

// Фабрики игр (по одной на файл в src/games)
Game* makeGameFish();
Game* makeGameSpts();
Game* makeGameShot();
Game* makeGameRng();
Game* makeGameLife();
