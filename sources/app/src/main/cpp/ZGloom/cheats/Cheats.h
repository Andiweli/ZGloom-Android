#pragma once

namespace Cheats
{
    void Load();
    bool Save(); // returns true on success

    bool GetGodMode();
    void SetGodMode(bool on);

    bool GetOneHitKill();
    void SetOneHitKill(bool on);

    int  GetStartWeapon();      // 0..4 fixed, 5 = DEFAULT
    void SetStartWeapon(int idx);

    bool GetThermoGoggles();
    void SetThermoGoggles(bool on);

    bool GetBouncyBullets();
    void SetBouncyBullets(bool on);

    bool GetInvisibility();
    void SetInvisibility(bool on);

    // Compatibility helpers expected by gamelogic/menuscreen
    int  AmplifyPlayerOutgoingDamage(int baseDamage);
    int  FilterDamageToPlayer(int incomingDamage);
    int  GetCheatReloadForWeapon(int weaponIdx, int defaultReload);
}
