#include "Header.h"
#include <algorithm>
#include <set>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

extern float g_cam_delta_x, g_cam_delta_y;
extern bool g_showBoundingBox;
PlayerState g_playerState;
LevelState  g_levelState;
PlayerInstance* g_player;
SceneManager g_sceneManager;

std::vector<SpriteInstance*> g_spriteInstances;
char g_dx, g_dy;
extern int WIN_W, WIN_H;
Sprite* g_spr0, *g_spr1, *g_spr2, *g_spr3, *g_sprBrick, *g_spr4;
Sprite* g_spr5;
Sprite* g_sprExit;
extern float g_cam_focus_x, g_cam_focus_y;

static const float GRAVITY = -480.0f;
static const float VX0 = 132.f, VY0 = 346.0f;
static float g_time_scale = 2.0f;

extern PerSceneCBData g_per_scene_cb_data;

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
  return g_player;
}

void do_ClearAndLoadLevel(const std::vector<std::vector<char> > m) {
  for (SpriteInstance* x : g_spriteInstances) {
    delete x;
  }

  g_spriteInstances.clear();
  g_player = nullptr;
  g_player = new PlayerInstance(-450, 300, 100, 100, 0, 0, 75, 100, g_spr2);
  g_levelState.Reset();

  const int H = int(m.size()), W = int(m[0].size());
  const int L = 100;

  std::set<FollowerInstance*> followers;

  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      char elt = m[y][x];
      float px = (-W * 0.5f + x) * L, py = (H * 0.5f - y) * L;
      switch (elt) {
      case 1: {
        g_spriteInstances.push_back(new WallInstance(px, py, L, L, g_sprBrick));
        break;
      }
      case 2: {
        g_spriteInstances.push_back(new ItemInstance(px, py, 60, 60, g_spr3));
        break;
      }
      case 3: {
        followers.insert(new FollowerInstance(px, py, 50, 50, 0, 0, 50, 50, g_spr4));
        g_levelState.num_total_sprites++;
        break;
      }
      case 4: {
        g_spriteInstances.push_back(new DestinationInstance(px, py, 100, 100, g_spr5));
        break;
      }
      case 9: {
        g_playerState.x = px; g_playerState.y = py;
        break;
      }
      case 8: {
        g_spriteInstances.push_back(new ExitInstance(px, py, L, L, g_sprExit));
        break;
      }
      }
    }
  }

  for (FollowerInstance* fi : followers) g_spriteInstances.push_back(fi);
  g_spriteInstances.push_back(g_player);
}

void LoadLevelFile(const char* filename) {
  std::ifstream f(filename);
  if (f.good() == false) return;
  std::string line;

  std::getline(f, line);
  std::stringstream wh(line); // Width and Height

  std::vector<std::vector<char> > lvldata;

  int W, H;
  wh >> W >> H;
  for (int i = 0; i < H; i++) {
    if (f.good() == false) break;
    std::getline(f, line);
    std::stringstream ls(line);
    std::vector<char> lvl_row;
    for (int j = 0; j < W; j++) {
      int temp;
      ls >> temp;
      lvl_row.push_back(char(temp));
    }
    lvldata.push_back(lvl_row);
  }

  f.close();

  do_ClearAndLoadLevel(lvldata);
}

void LoadLevel(int levelid) {

  const std::vector<std::vector<char> > stupidMap0 = {
    { 0,1,1,1,1,1,1,0,0,0,0,0,0 },
    { 0,1,9,0,0,0,1,0,1,1,1,1,0 },
    { 1,1,1,1,0,1,1,1,1,0,0,1,1 },
    { 1,0,0,0,0,0,0,3,0,3,0,3,1 },
    { 1,8,0,0,0,0,0,1,1,0,0,1,1 },
    { 1,2,2,0,4,1,0,1,1,1,0,0,1 },
    { 1,2,2,0,1,1,0,1,0,1,1,3,1 },
    { 1,1,1,1,1,1,1,1,0,1,1,1,1 },
  };

  const std::vector<std::vector<char> > stupidMap1 = {
    { 1,1,1,1,1,1,1,1,1,1,1,1,1 },
    { 1,0,0,0,0,0,0,0,0,0,0,0,1 },
    { 1,3,0,0,0,0,0,0,0,0,0,3,1 },
    { 1,1,1,0,0,0,0,0,0,0,1,1,1 },
    { 1,0,0,0,0,0,4,8,0,0,0,0,1 },
    { 1,0,0,0,1,1,1,1,1,0,0,0,1 },
    { 1,3,0,0,0,0,0,0,0,0,0,3,1 },
    { 1,1,1,0,0,0,0,0,0,0,1,1,1 },
    { 1,0,0,0,3,3,3,3,3,0,0,0,1 },
    { 1,0,0,0,1,1,1,1,1,3,0,0,1 },
    { 1,9,0,1,1,1,1,1,1,1,0,3,1 },
    { 1,1,1,1,1,1,1,1,1,1,1,1,1 },
  };

  if (levelid == 0)
    do_ClearAndLoadLevel(stupidMap0);
  else 
    do_ClearAndLoadLevel(stupidMap1);
}

void LoadDummyAssets() {
  g_spr0 = new Sprite(0);
  g_spr1 = new Sprite(1);
  g_spr2 = new Sprite(2);
  g_spr3 = new Sprite(3);
  g_sprBrick = new Sprite(4);
  g_spr4 = new Sprite(5);
  g_spr5 = new Sprite(6);
  g_sprExit = new Sprite(7);
  //SpriteInstance* i0 = new SpriteInstance(0,       0, 100, 100, g_spr0);
  //SpriteInstance* i1 = new SpriteInstance(-100.0f, 0, 100, 100, g_spr1);
  //SpriteInstance* iPlayer = new SpriteInstance(-200.0f, 0, 100, 100, g_spr2);
  

  //g_spriteInstances.push_back(i1);
  //g_spriteInstances.push_back(i0);

  

  const std::vector<std::vector<char> > stupidMap = {
    { 0,1,1,1,1,1,1,0,0,0,0,0,0 },
    { 0,1,9,0,0,0,1,0,1,1,1,1,0 },
    { 1,1,1,1,0,1,1,1,1,0,0,1,1 },
    { 1,0,0,0,0,0,0,3,0,3,0,3,1 },
    { 1,8,0,0,0,0,0,1,1,0,0,1,1 },
    { 1,2,2,0,4,1,0,1,1,1,0,0,1 },
    { 1,2,2,0,1,1,0,1,0,1,1,3,1 },
    { 1,1,1,1,1,1,1,1,0,1,1,1,1 },
  };
  const int H = int(stupidMap.size()), W = int(stupidMap[0].size());
  const int L = 100;

  std::set<FollowerInstance*> followers;

  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      char elt = stupidMap[y][x];
      float px = (-W * 0.5f + x) * L, py = (H * 0.5f - y) * L;
      switch (elt) {
      case 1: {
        g_spriteInstances.push_back(new WallInstance(px, py, L, L, g_sprBrick));
        break;
      }
      case 2: {
        g_spriteInstances.push_back(new ItemInstance(px, py, 60, 60, g_spr3));
        printf("item at %g,%g\n", px, py);
        break;
      }
      case 3: {
        followers.insert(new FollowerInstance(px, py, 50, 50, 0, 0, 50, 50, g_spr4));
        g_levelState.num_total_sprites++;
        break;
      }
      case 4: {
        g_spriteInstances.push_back(new DestinationInstance(px, py, 100, 100, g_spr5));
        break;
      }
      case 9: {
        g_playerState.x = px; g_playerState.y = py;
        break;
      }
      case 8: {
        g_spriteInstances.push_back(new ExitInstance(px, py, L, L, g_sprExit));
        break;
      }
      }
    }
  }

  for (FollowerInstance* fi : followers) g_spriteInstances.push_back(fi);
  g_spriteInstances.push_back(g_player);
}

void LaunchProjectile() {
  SpriteInstance* pSprInst = g_player;
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
    g_playerState.is_left_pressed = true;
    g_dx -= 1 * g_time_scale; break;
  case VK_RIGHT: case 'd':
    g_playerState.facing = 1;
    g_playerState.is_right_pressed = true;
    g_dx += 1 * g_time_scale; break;
  case 'Z': case 'z': case VK_UP:
    g_playerState.Jump(370.0f);
    g_playerState.is_jump_pressed = true;
    break;
  case VK_ESCAPE:
    PostQuitMessage(0);
    break;
  case VK_BACK:
    g_showBoundingBox = !g_showBoundingBox;
    break;
  case VK_SPACE: case 'x': case 'X':
    LaunchProjectile();
    break;
  case '0':
    //LoadLevel(0);
    g_sceneManager.FadeOutLoadLevelFadeIn(0);
    break;
  }
}

void OnKeyUp(WPARAM wParam, LPARAM lParam) {
  switch (wParam) {
  //case VK_DOWN: case 's':
  //  g_dy += 1 * g_time_scale; break;
  //case VK_UP: 
  //case 'w':
  //  g_dy -= 1 * g_time_scale; break;
  case VK_LEFT: case 'a':
    g_playerState.is_left_pressed = false;
    g_dx += 1 * g_time_scale; break;
  case VK_RIGHT: case 'd':
    g_playerState.is_right_pressed = false;
    g_dx -= 1 * g_time_scale; break;
  case VK_UP: case 'z': case'Z':
    g_playerState.is_jump_pressed = false;
    break;
  }
}

void ProjectileInstance::Update(float secs) {
  const float dt = secs * g_time_scale;
  vy += GRAVITY * dt;
  x += vx * dt; y += vy * dt;
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

  g_sceneManager.Update(dt);
  PlayerInstance* pPlayer = GetPlayer();

  if (g_sceneManager.state == ScenePlaying) {

    const float pxPerSec = 144;

    g_playerState.x += g_dx * pxPerSec * dt;
    g_playerState.y += g_dy * pxPerSec * dt;
    g_playerState.UpdateGravity(dt);

    pPlayer->x = g_playerState.x;
    pPlayer->y = g_playerState.y;
    SpriteInstance playerCollider = pPlayer->GetCollisionShape();

    std::set<ItemInstance*> victims;
    victims.clear();

    // Collision Detection and Resolve
    // Collide player with all walls
    {
      const float w = pPlayer->w, h = pPlayer->h;
      for (SpriteInstance* p : g_spriteInstances) {

        ItemInstance* ii = NULL;
        FollowerInstance* fi = NULL;
        ExitInstance* ei = NULL;

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
        else if ((ii = dynamic_cast<ItemInstance*>(p)) != NULL) {
          if (int blah = ii->Collide(&playerCollider, NULL, NULL) != -1) {
            victims.insert(ii);
            printf("Erasing a sprite @ (%g,%g); player is at (%g,%g) push=%d!\n", ii->x, ii->y, pPlayer->x, pPlayer->y, blah);
          }
        }
        else if ((fi = dynamic_cast<FollowerInstance*>(p)) != NULL) {
          if (int blah = fi->Collide(&playerCollider, NULL, NULL) != -1) {
            if (fi->is_done == false)
              pPlayer->AddFollower(fi);
          }
        }
        else if ((ei = dynamic_cast<ExitInstance*>(p)) != NULL) {
          if (g_levelState.IsAllSpritesSaved()) {
            if (p->Collide(&playerCollider, nullptr, nullptr) != -1) {
              g_levelState.Reset();
              g_sceneManager.FadeOutLoadLevelFadeIn(g_sceneManager.NextLevel());
            }
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

      // Collide followers with destinations
      if (g_playerState.follower_complete_cooldown <= 0)
      {
        std::set<FollowerInstance*> followers;
        std::set<DestinationInstance*> dests;
        for (SpriteInstance* sp : g_spriteInstances) {
          FollowerInstance* fi = dynamic_cast<FollowerInstance*>(sp);
          if (fi) {
            followers.insert(fi);
          }
          DestinationInstance* di = dynamic_cast<DestinationInstance*>(sp);
          if (di) {
            dests.insert(di);
          }
        }
        for (FollowerInstance* fi : followers) {
          if (fi->is_done == false) {
            for (DestinationInstance* di : dests) {
              float new_x, new_y;
              if (int blah = di->Collide(fi, &new_x, &new_y) != -1) {
                fi->CompleteFollowing();
                g_playerState.follower_complete_cooldown = 0.2;
                goto DONE_FOLLOWER_COMPLETE;
              }
            }
          }
        }
      }
    DONE_FOLLOWER_COMPLETE:

      // Camera Effects
      {
        if (g_playerState.is_left_pressed) g_cam_delta_x += 0.1f;
        if (g_playerState.is_right_pressed) g_cam_delta_x -= 0.1f;
        if (g_playerState.is_jump_pressed) g_cam_delta_y += 0.13f;

        g_cam_delta_x *= 0.95f;
        g_cam_delta_y *= 0.95f;
      }
    }

    for (UINT i = 0; i < g_spriteInstances.size(); i++) {
      g_spriteInstances[i]->Update(dt);
    }

    // Delete projectiles
    std::vector<SpriteInstance*> next_sprites;
    for (SpriteInstance* si : g_spriteInstances) {
      ProjectileInstance* pi = dynamic_cast<ProjectileInstance*>(si);
      if (pi && pi->Demised()) {
        delete pi; continue;
      }

      ItemInstance* ii = dynamic_cast<ItemInstance*>(si);
      if (victims.find(ii) != victims.end()) {
        delete ii;
        continue;
      }
      next_sprites.push_back(si);
    }

    g_spriteInstances = next_sprites;


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

  } // End state is playing


  // Camera follows even when not playing
  {
    const float px = g_playerState.x, py = g_playerState.y,
      cx = g_cam_focus_x, cy = g_cam_focus_y;
    const float L = 0.05f;
    g_cam_focus_x = L * px + (1.0f - L) * cx;
    g_cam_focus_y = L * py + (1.0f - L) * cy;
  }

  // View matrix
  DirectX::XMVECTOR eye, target, up;
  eye.m128_f32[0] = g_cam_delta_x + g_cam_focus_x / 100.0f;
  eye.m128_f32[1] = g_cam_delta_y + g_cam_focus_y / 100.0f;
  eye.m128_f32[2] = -10;

  if (g_sceneManager.state == SceneFadingIn) {
    const float c = g_sceneManager.completion;
    eye.m128_f32[2] = -10 - 100 * (1-(c*c));
  }
  else if (g_sceneManager.state == SceneFadingOutThenIn) {
    const float c = g_sceneManager.completion;
    eye.m128_f32[2] = -10 - 100 * (1-(1-c)*(1-c));
  }

  up.m128_f32[0] = 0;
  up.m128_f32[1] = 1;
  up.m128_f32[2] = 0;
  target.m128_f32[0] = g_cam_focus_x / 100.0f;
  target.m128_f32[1] = g_cam_focus_y / 100.0f;
  target.m128_f32[2] = 0;
  g_per_scene_cb_data.view = DirectX::XMMatrixLookAtLH(eye, target, up);
}

// ==============================

void PlayerState::UpdateGravity(float secs) {
  const float dt = secs * g_time_scale;
  vy += GRAVITY * dt;
  x += vx * dt; y += vy * dt;

  follower_complete_cooldown -= dt;
  if (follower_complete_cooldown < 0)
    follower_complete_cooldown = 0;
}

void PlayerState::Jump(float vy) {
  this->vy = vy;
}

// ==============================
SpriteInstance ActorInstance::GetCollisionShape() {
  float dx = x + coll_rect_center_x;
  float dy = y + coll_rect_center_y;
  return SpriteInstance(dx, dy, coll_rect_w, coll_rect_h, NULL);
}

// ==============================
void PlayerInstance::AddFollower(FollowerInstance* f) {
  if (f->subject != NULL) return;

  f->StartFollowing(this);

  followers.insert(f);
  printf("Added a follower\n");
}

// ==============================
void FollowerInstance::Update(float secs) {
  const float dt = secs * g_time_scale;
  if (subject != NULL) {
    std::pair<float, float> p = std::make_pair(subject->x, subject->y);
    historical_pos.push(p);
    while (historical_pos.size() >= history_len) {
      p = historical_pos.front();
      historical_pos.pop();

      this->x = p.first;
      this->y = p.second;
    }
  }
  else {
    if (historical_pos.size() > 0) {
      std::pair<float, float> p = historical_pos.front();
      historical_pos.pop();

      this->x = p.first;
      this->y = p.second;
    }
  }
}

// Start following and pre-fill the historical position buffer
void FollowerInstance::StartFollowing(PlayerInstance* who) {
  subject = who;

  int mult = 30;
  mult = mult - (who->followers.size()) * 2;
  if (mult < 10) mult = 10;

  history_len = mult * (1 + who->followers.size());

  while (historical_pos.empty() == false) historical_pos.pop();
  float x0 = this->x, x1 = who->x, y0 = this->y, y1 = who->y;
  for (int i = 0; i < history_len; i++) {
    float t = (i + 1) * 1.0 / history_len;
    float xx = x0 * (1.0f - t) + x1 * t,
      yy = y0 * (1.0f - t) + y1 * t;
    historical_pos.push(std::make_pair(xx, yy));
  }
}

void FollowerInstance::CompleteFollowing() {
  subject->followers.erase(this);
  subject = NULL;
  is_done = true;

  const int leftover_frames = rand() % 3 + 3;
  std::vector<std::pair<float, float> > temp;
  for (int i = 0; i < leftover_frames && historical_pos.empty() == false; i++) {
    temp.push_back(historical_pos.front());
    historical_pos.pop();
  }
  while (historical_pos.empty() == false) historical_pos.pop();
  for (const std::pair<float, float>& x : temp) historical_pos.push(x);

  g_levelState.OnSpriteSaved();
}

// ==============================
void ItemInstance::Update(float secs) {
  orientation = orientation * DirectX::XMMatrixRotationZ(0.1f);
}

void LevelState::OnSpriteSaved() {
  num_saved_sprites++;
  if (num_saved_sprites == num_total_sprites) {
    // Reveal exit
    for (SpriteInstance* inst : g_spriteInstances) {
      ExitInstance* ei = dynamic_cast<ExitInstance*>(inst);
      if (ei != NULL) {
        ei->enabled = true;
      }
    }
  }
}

float SceneManager::GetZoomFactor() {
  return completion;
}

void SceneManager::FadeIn() {
  state = SceneFadingIn;
  completion = 0.0f;
}

void SceneManager::Update(float secs) {
  switch (state) {
    case SceneFadingIn:
      completion += secs / DURATION;
      if (completion > 1.0f) {
        completion = 1.0f; state = ScenePlaying;
      }
      break;
    case SceneFadingOutThenIn:
      completion += secs / DURATION;
      if (completion > 1.0f) {
        completion = 0.0f; state = SceneFadingIn;
        LoadLevel(level_to_load);
      }
      break;
    default: break;
  }
}

void SceneManager::FadeOutLoadLevelFadeIn(int x) {
  level_to_load = x;
  state = SceneFadingOutThenIn;
  completion = 0.0f;
}

int SceneManager::GetCurrLevel() { return curr_level; }

int SceneManager::NextLevel() {
  curr_level = (curr_level + 1) % NUM_LEVELS;
  return curr_level;
}