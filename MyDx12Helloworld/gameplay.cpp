#include "Header.h"
#include <algorithm>

extern bool g_showBoundingBox;
PlayerState g_playerState;

std::vector<SpriteInstance*> g_spriteInstances;
char g_dx, g_dy;
extern int WIN_W, WIN_H;
Sprite* g_spr0, *g_spr1, *g_spr2, *g_spr3, *g_sprBrick;

static const float GRAVITY = -480.0f;
static const float VX0 = 132.f, VY0 = 346.0f;

// returns direction 0=left 1=right 2=up 3=down -1=no collision
int SpriteInstance::Collide(SpriteInstance* other, float* new_x, float* new_y) {
  const float x_lb = x - w / 2 - other->w / 2;
  const float x_ub = x + w / 2 + other->w / 2;
  const float y_lb = y - h / 2 - other->h / 2;
  const float y_ub = y + h / 2 + other->h / 2;
  
  int ret = -1;

  // broad phase
  if (other->x > x_lb &&
    other->x < x_ub &&
    other->y > y_lb &&
    other->y < y_ub)
  {
    // X Resolution
    float pushLeft = other->x - x_lb;
    float pushRight = x_ub - other->x;
    float pushUp = y_ub - other->y;
    float pushDown = other->y - y_lb;

    float* ptrs[] = { &pushLeft, &pushRight, &pushUp, &pushDown };
    for (int i = 0; i < 4; i++) {
      float* ptr = ptrs[i];
      if (*ptr < 0) *ptr = 1e20;
    }

    const float eps = .0f;

    std::vector<std::pair<float, int> > temp = {
      { pushLeft, 0 },
      { pushRight, 1 },
      { pushUp, 2 },
      { pushDown, 3 }
    };
    std::sort(temp.begin(), temp.end());
    ret = temp[0].second;
    switch (temp[0].second) {
    case 0:
      // Push left
      if (new_x)
        *new_x = x_lb - eps;
      break;
    case 1:
      // Push right
      if (new_x)
        *new_x = x_ub + eps;
      break;
    case 2:
      // Push up
      if (new_y)
        *new_y = y_ub + eps;
      break;
    case 3:
      // Push down
      if(new_y)
        *new_y = y_lb - eps;
      break;
    }
  }
  return ret;
}

PlayerInstance* GetPlayer() { 
  return dynamic_cast<PlayerInstance*>(g_spriteInstances[0]);
}

void PopulateDummy() {
  g_spr0 = new Sprite(0);
  g_spr1 = new Sprite(1);
  g_spr2 = new Sprite(2);
  g_spr3 = new Sprite(3);
  g_sprBrick = new Sprite(4);
  SpriteInstance* i0 = new SpriteInstance(0,       0, 100, 100, g_spr0);
  SpriteInstance* i1 = new SpriteInstance(-100.0f, 0, 100, 100, g_spr1);
  //SpriteInstance* iPlayer = new SpriteInstance(-200.0f, 0, 100, 100, g_spr2);
  PlayerInstance* iPlayer = new PlayerInstance(-200.0f, 0, 100, 100, 0, 0, 75, 100, g_spr2);

  g_spriteInstances.push_back(iPlayer);
  //g_spriteInstances.push_back(i1);
  //g_spriteInstances.push_back(i0);

  const std::vector<std::vector<char> > stupidMap = {
    { 1,1,1,1,1,1,1,1 },
    { 1,0,0,0,0,0,0,1 },
    { 1,0,0,0,0,0,0,1 },
    { 1,0,0,0,0,1,0,1 },
    { 1,0,0,0,1,1,0,1 },
    { 1,1,1,1,1,1,1,1 },
  };
  const int H = int(stupidMap.size()), W = int(stupidMap[0].size());
  const int L = 100;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      char elt = stupidMap[y][x];
      if (elt) {
        float px = (-W * 0.5f + x) * L, py = (H * 0.5f - y) * L;
        g_spriteInstances.push_back(new WallInstance(px, py, L, L, g_sprBrick));
      }
    }
  }
}

void LaunchProjectile() {
  SpriteInstance* pSprInst = g_spriteInstances[0];
  float x = pSprInst->x - 25.0f;
  float y = pSprInst->y + 25.0f;

  ProjectileInstance* p = new ProjectileInstance(x, y, 48, 48, g_spr3);
  p->vx = VX0; p->vy = VY0;
  p->angular_velocity = 0.1f;

  if (g_playerState.facing == -1) {
    p->vx = -VX0;
    p->x += 50.0f;
    p->angular_velocity = -0.1f;
  }

  g_spriteInstances.push_back(p);
  printf("|g_spriteInstances|=%lu\n", g_spriteInstances.size());
}

void OnKeyDown(WPARAM wParam, LPARAM lParam) {
  if (lParam & 0x40000000) return;
  switch (wParam) {
  //case VK_DOWN: case 's':
  //  g_dy -= 1; break;
  //case VK_UP: case 'w':
  //  g_dy += 1; break;
  case VK_LEFT: case 'a':
    g_playerState.facing = -1;
    g_dx -= 1; break;
  case VK_RIGHT: case 'd':
    g_playerState.facing = 1;
    g_dx += 1; break;
  case 'Z':
    g_playerState.Jump(370.0f);
    break;
  case VK_ESCAPE:
    PostQuitMessage(0);
    break;
  case VK_CAPITAL:
    g_showBoundingBox = !g_showBoundingBox;
    break;
  case VK_SPACE: case 'X':
    LaunchProjectile();
    break;
  }
}

void OnKeyUp(WPARAM wParam, LPARAM lParam) {
  switch (wParam) {
  case VK_DOWN: case 's':
    g_dy += 1; break;
  case VK_UP: case 'w':
    g_dy -= 1; break;
  case VK_LEFT: case 'a':
    g_dx += 1; break;
  case VK_RIGHT: case 'd':
    g_dx -= 1; break;
  }
}

void ProjectileInstance::Update(float secs) {
  vy += GRAVITY * secs;
  x += vx * secs; y += vy * secs;
  orientation *= DirectX::XMMatrixRotationZ(angular_velocity);
  if (y < -WIN_H / 2) {
    vy = -vy * 0.6f;
    y = -WIN_H / 2;
  }
}

// https://gamedev.stackexchange.com/questions/26759/best-way-to-get-elapsed-time-in-miliseconds-in-windows?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
long long MillisecondsNow() {
  static LARGE_INTEGER s_frequency;
  static BOOL s_use_qpc = QueryPerformanceFrequency(&s_frequency);
  if (s_use_qpc) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (1000LL * now.QuadPart) / s_frequency.QuadPart;
  }
  else {
    return GetTickCount();
  }
}

void GameplayUpdate() {
  const float dt = 1.0f / 60;
  const float pxPerSec = 144;

  g_playerState.x += g_dx * pxPerSec * dt;
  g_playerState.y += g_dy * pxPerSec * dt;
  g_playerState.UpdateGravity(dt);

  PlayerInstance* pPlayer = GetPlayer();
  SpriteInstance playerCollider = pPlayer->GetCollisionShape();
  pPlayer->x = g_playerState.x;
  pPlayer->y = g_playerState.y;

  // Collision Detection and Resolve
  // Collide player with all walls
  {
    const float w = pPlayer->w, h = pPlayer->h;
    for (SpriteInstance* p : g_spriteInstances) {
      if (dynamic_cast<WallInstance*>(p)) {
        float new_x, new_y;
        int dir = p->Collide(&playerCollider, &new_x, &new_y);
        if (dir == 1 || dir == 0) {
          g_playerState.x = new_x;
        }
        if (dir == 2 || dir == 3) {
          g_playerState.y = new_y;
        }
        if (dir == 2) {
          if (g_playerState.vy < 0)
            g_playerState.vy = 0;
        }
      }
    }

    // Collide projectile with walls
    {
      for (SpriteInstance* sp : g_spriteInstances) {
        ProjectileInstance* pi = dynamic_cast<ProjectileInstance*>(sp);
        if (pi) {
          for (SpriteInstance* p : g_spriteInstances) {
            if (dynamic_cast<WallInstance*>(p)) {
              float new_x, new_y;
              int dir = p->Collide(pi, &new_x, &new_y);
              if (dir == 0 || dir == 1) {
                pi->vx = -pi->vx;
                pi->x = new_x;
                pi->collision_count++;
                pi->angular_velocity *= -1;
              }
              if (dir == 2 || dir == 3) {
                pi->y = new_y;
                pi->collision_count++;
                if (dir == 2) {
                  pi->vy = -pi->vy * 0.6f;
                }
                else pi->vy = 0;
              }
            }
          }
        }
      }
    }
  }


  if (g_dx != 0 || g_dy != 0) {
    
    float scaleY = sin(MillisecondsNow() / 250.0f * 2 * 3.1415926f);
    DirectX::XMVECTOR s;

    float yDiff = scaleY * 0.03f;

    s.m128_f32[0] = 1.0f;
    s.m128_f32[1] = scaleY + 1.0f;
    s.m128_f32[2] = 1.0f;
    s.m128_f32[3] = 1.0f;
    //pSprInst->orientation = DirectX::XMMatrixScalingFromVector(s);
    pPlayer->orientation = DirectX::XMMatrixTranslation(0.0f, yDiff, 0.0f);
  }
  else {
    pPlayer->orientation = DirectX::XMMatrixIdentity();
  }

  // facing left or right?
  if (g_playerState.facing == -1) pPlayer->orientation *= DirectX::XMMatrixRotationY(3.14159f);

  for (UINT i = 0; i < g_spriteInstances.size(); i++) {
    g_spriteInstances[i]->Update(dt);
  }

  // Delete projectiles
  std::vector<SpriteInstance*> next_sprites;
  for (SpriteInstance* si : g_spriteInstances) {
    ProjectileInstance* pi = dynamic_cast<ProjectileInstance*>(si);
    if (pi && pi->Demised()) {
      delete pi;
      continue;
    }

    next_sprites.push_back(si);
  }
  g_spriteInstances = next_sprites;
}

// ==============================

void PlayerState::UpdateGravity(float secs) {
  vy += GRAVITY * secs;
  x += vx * secs; y += vy * secs;
}

void PlayerState::Jump(float vy) {
  this->vy = vy;
}

// ==============================
SpriteInstance PlayerInstance::GetCollisionShape() {
  float dx = x + coll_rect_center_x;
  float dy = y + coll_rect_center_y;
  return SpriteInstance(dx, dy, coll_rect_w, coll_rect_h, NULL);
}