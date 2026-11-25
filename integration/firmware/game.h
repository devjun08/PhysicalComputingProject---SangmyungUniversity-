#ifndef GAME_H
#define GAME_H
#include <stdint.h>
void Game_Init(uint32_t now);
void Game_Command(int command, uint32_t now);
void Game_Update(uint32_t now);
void Game_Sensors(const uint16_t values[9], uint32_t now);
#endif
