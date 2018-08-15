#pragma once
#include <vector>
#include <queue>
#include <set>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>
#include <DirectXMath.h>

struct PerSceneCBData {
  DirectX::XMMATRIX view;
  DirectX::XMMATRIX projection;
};

class Sprite {
public:
  float u0, v0, u1, v1; // location in the sprite sheet
  int textureId;
  Sprite(int _tid) : u0(0), v0(0), u1(1), v1(1), textureId(_tid) { }
};

class PlayerState {
public:
  PlayerState() {
    x = y = vx = vy = 0;
  }
  float x, y, vx, vy;
  int facing; // 1 or -1
  void UpdateGravity(float secs);
  void Jump(float vy);
  bool is_left_pressed;
  bool is_right_pressed;
  bool is_jump_pressed;

  float follower_complete_cooldown; // seconds
};

class SpriteInstance {
public:
  float x, y; // in Game World
  float w, h; // in pixels
  DirectX::XMMATRIX orientation;
  Sprite* pSprite;
  SpriteInstance(float _x, float _y, float _w, float _h, Sprite* _spr) {
    x = _x; y = _y; w = _w; h = _h; pSprite = _spr;
    //orientation = DirectX::XMMatrixRotationZ(0.0f);
    orientation = DirectX::XMMatrixIdentity();
  }
  virtual void Update(float secs) { }
  virtual int Collide(SpriteInstance* other, float* new_x, float* new_y);
};

class ActorInstance : public SpriteInstance {
public:
  bool is_player;
  ActorInstance(float _x, float _y, float _w, float _h, float collx, float colly, float collw, float collh, Sprite* _spr) :
    SpriteInstance(_x, _y, _w, _h, _spr) {
    coll_rect_center_x = collx;
    coll_rect_center_y = colly;
    coll_rect_w = collw;
    coll_rect_h = collh;
  }
  float coll_rect_w, coll_rect_h;
  float coll_rect_center_x, coll_rect_center_y; // Relative to location's X Y
  SpriteInstance GetCollisionShape();
};

class PlayerInstance;

class FollowerInstance : public ActorInstance {
public:
  PlayerInstance * subject;
  bool is_done;
  FollowerInstance(float _x, float _y, float _w, float _h, float collx, float colly, float collw, float collh, Sprite* _spr) :
    ActorInstance(_x, _y, _w, _h, collx, colly, collw, collh, _spr) {
    subject = NULL;
    is_done = false;
  }
  std::queue<std::pair<float, float> > historical_pos;
  int history_len;

  virtual void Update(float secs);
  void StartFollowing(PlayerInstance* who);
  void CompleteFollowing();
};

class PlayerInstance : public ActorInstance {
public:
  PlayerInstance(float _x, float _y, float _w, float _h, float collx, float colly, float collw, float collh, Sprite* _spr) :
    ActorInstance(_x, _y, _w, _h, collx, colly, collw, collh, _spr) { }
  void AddFollower(FollowerInstance* f);
  std::set<FollowerInstance*> followers;
};

class WallInstance : public SpriteInstance {
public:
  virtual void Update(float secs) { }
  WallInstance(float _x, float _y, float _w, float _h, Sprite* _spr) :
    SpriteInstance(_x, _y, _w, _h, _spr) { }
};

class ProjectileInstance : public SpriteInstance {
public:
  float vx, vy, angular_velocity;
  int collision_count;
  const static int COLLISIONS_MAX = 10;
  ProjectileInstance(float _x, float _y, float _w, float _h, Sprite* _spr) :
    SpriteInstance(_x, _y, _w, _h, _spr) {
    collision_count = 0;
  }
  bool Demised() { return collision_count >= COLLISIONS_MAX; }
  virtual void Update(float secs);
};

class ItemInstance : public SpriteInstance {
public:
  ItemInstance(float _x, float _y, float _w, float _h, Sprite* _spr) :
    SpriteInstance(_x, _y, _w, _h, _spr) {

  }
  virtual void Update(float secs); // Will rotate around Z axis
};

// This is where followers go
class DestinationInstance : public SpriteInstance {
public:
  DestinationInstance(float _x, float _y, float _w, float _h, Sprite* _spr) :
    SpriteInstance(_x, _y, _w, _h, _spr) {

  }
};

void PopulateDummy();
void OnKeyDown(WPARAM wParam, LPARAM lParam);
void OnKeyUp(WPARAM wParam, LPARAM lParam);
void GameplayUpdate();
