#pragma once
#include <string>
#include <cstdint>

namespace Cheats {

struct State {
    bool godmode = false;
    bool one_hit_kill = false;
    int  start_weapon = 0;             // 0..5 (0..4 fixed, 5 = DEFAULT)
    int  boost_level[5] = {0,0,0,0,0}; // 0..4 per weapon
};

void Init(const char* path = nullptr);
void Reload();
bool Save();
State Get();

bool GetGodMode();
void SetGodMode(bool v);

bool GetOneHitKill();
void SetOneHitKill(bool v);

int  GetStartWeapon();       // 0..5 (0..4 fixed, 5 = DEFAULT)
void SetStartWeapon(int w);  // clamps 0..5

bool GetThermoGoggles();
void SetThermoGoggles(bool on);

bool GetBouncyBullets();
void SetBouncyBullets(bool on);

bool GetInvisibility();
void SetInvisibility(bool on);

int  GetBoostLevel(int weaponIndex);   // 0..4
void SetBoostLevel(int weaponIndex, int lv); // clamps 0..4

int  FilterDamageToPlayer(int damage);
int  AmplifyPlayerOutgoingDamage(int baseDamage);

int  BoostLevelToReload(int boostLevel);
int  GetCheatReloadForWeapon(int weaponIndex, int fallbackReload);

} // namespace Cheats
