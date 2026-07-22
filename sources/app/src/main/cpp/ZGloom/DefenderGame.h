#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gloommap.h"

// Platform-independent recreation of Gloom's hidden Defender monitor game.
// The original artwork is stored after the visible 64x64 monitor texture.
// This class renders the 44x36 game image directly into the live wall texture,
// so every platform sees it through the normal perspective wall renderer.
class DefenderGame
{
public:
    struct InputState
    {
        int moveX = 0; // -1 left, +1 right
        int moveY = 0; // -1 up,   +1 down
        bool fire = false;
    };

    DefenderGame();

    // textureColumns is the owning texture file. baseColumn is the first
    // column of the selected 64x64 texture (global texture index modulo file).
    bool Begin(std::vector<Column>* textureColumns, std::size_t baseColumn, int episode);
    void Update(const InputState& input);
    void Reset();

    bool IsActive() const { return active; }
    bool IsComplete() const { return complete; }
    bool DidWin() const { return won; }
    int GetPlayerLives() const { return playerLives; }
    int GetTargetsRemaining() const { return targetsRemaining > 0 ? targetsRemaining : 0; }

private:
    enum class ObjectType
    {
        Lander,
        Bullet,
        Fragment,
        ImpactFragment
    };

    struct DefObject
    {
        ObjectType type = ObjectType::Fragment;
        int32_t x = 0;  // 16.16, wraps at 256
        int32_t y = 0;  // 16.16
        int32_t vx = 0;
        int32_t vy = 0;
        int shape = 0;
        int delay = 0;
        bool collidable = false;
        bool remove = false;
    };

    struct ShapeDef
    {
        int sourceX;
        int sourceY;
        int width;
        int height;
    };

    static constexpr int kScreenWidth = 44;
    static constexpr int kScreenHeight = 36;
    static constexpr int kDestinationX = 10;
    static constexpr int kDestinationY = 19;
    static constexpr int kArtworkX = 64;
    static constexpr int kWorldWidth = 256;
    static constexpr int kMaxObjects = 128;

    std::vector<Column>* columns = nullptr;
    std::size_t baseColumn = 0;
    std::vector<DefObject> objects;

    bool active = false;
    bool complete = false;
    bool won = false;
    bool fireHeld = false;
    bool playerDead = false;
    bool resultStarted = false;

    int episode = 1;
    int targetTotal = 20;
    int targetsToSpawn = 20;
    int targetsRemaining = 20;
    int landerDelay = 25;
    int landerCounter = 1;
    int playerLives = 3;
    int playerShape = 0;
    int deadCounter = 0;
    int resultCounter = 0;

    int32_t playerX = 0;
    int32_t playerY = 18 << 16;
    int32_t playerVX = 0;

    static const ShapeDef kShapes[14];

    bool ValidateTexture() const;
    void InitialiseRound();
    void StartResult(bool playerWon);
    void SpawnLander();
    void SpawnBullet();
    void DestroyLander(std::size_t index);
    void DestroyPlayer();
    void SpawnFragments(int32_t x, int32_t y, int count, int firstShape, bool gravity);
    void UpdatePlayer(const InputState& input);
    void UpdateObjects();
    void ResolveCollisions();
    void RemoveDeadObjects();

    void DrawFrame();
    void DrawMountains(int offset);
    void DrawSprite(int destX, int destY, int shape);
    void SetDestinationPixel(int x, int y, uint8_t colour);
    uint8_t GetArtworkPixel(int x, int y) const;

    static int32_t WrapWorldX(int32_t value);
    static int WrappedDelta(int lhs, int rhs);
    static int RandomSigned16();
};
