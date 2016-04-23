//#ifndef WIN32
//#define WIN32 1
//#endif

#include "ESAT/window.h"
#include "ESAT/draw.h"
#include "ESAT/input.h"
 
#include <iostream>
#include "time.h"
#include "Ship.h"

#define STB_PERLIN_IMPLEMENTATION
#include "Terrain.h"
 
#include "ESAT_extra/chipmunk/chipmunk.h"
 
#include "config.h"

ESAT::SpriteHandle bg;

struct {
  int width;
  int height;
} WindowOptions = {kWinWidth, kWinHeight};

typedef struct {
    Ship* ship;
    Terrain* terrain;
  } Colliders;
  
int g_score = 0;
float g_time = 0.0f;

cpSpace* g_space = nullptr;
Ship* g_ship = nullptr;
Terrain* g_terrain = nullptr;
Colliders* g_colliders = nullptr;


void drawInfo(Ship* ship) {

  std::string score_str ("SCORE  "+std::to_string(g_score));
  std::string time_str  ("TIME   "+std::to_string(g_time));
  std::string fuel_str  ("FUEL   "+std::to_string(ship->fuel_));
  std::string alt_str   ("ALTITUDE            "+std::to_string(kWinHeight - ship->pos_.y).substr(0,5));
  std::string hspeed_str("HORIZONTAL SPEED    "+std::to_string(fabs(ship->velocity_.x)).substr(0,5));
  std::string vspeed_str("VERTICAL SPEED      "+std::to_string(fabs(ship->velocity_.y)).substr(0,5));
  
  hspeed_str += (ship->velocity_.x > 0.0f) ? "   >" : "   <";
  vspeed_str += (ship->velocity_.y > 0.0f) ? "   -" : "   +";
  
  ESAT::DrawText(100.0f, 50.0f, score_str.c_str());
  ESAT::DrawText(100.0f, 75.0f, time_str.c_str());
  ESAT::DrawText(100.0f, 100.0f, fuel_str.c_str());
  
  ESAT::DrawText(800.0f, 50.0f, alt_str.c_str());
  ESAT::DrawText(800.0f, 75.0f, hspeed_str.c_str());
  ESAT::DrawText(800.0f, 100.0f, vspeed_str.c_str());
}


cpBool OnCollisionEnter(cpArbiter *arb, cpSpace *space, void *data) {
  //Magic goes here
  cpShape *a = NULL;
  cpShape *b = NULL;
  cpBody *c = NULL;
  cpBody *d = NULL;
  
  cpArbiterGetShapes(arb, &a, &b);
  cpArbiterGetBodies(arb, &c, &d);

  Colliders* collider = reinterpret_cast<Colliders*>(data);
  
  Ship* ship = collider->ship;
  Terrain* terrain = collider->terrain;
  
  //Order of colliders is undeterministic
  if (!ship->landed_ && (ship->physics_body_ == c || ship->physics_body_ == d)) {
    cpBody* s = ship->physics_body_;
    cpVect velocity = cpBodyGetVelocity(s);
    cpVect rotation = cpBodyGetRotation(s);
    ship->landed_ = true;
    
    float velocity_threshold = 0.9f;
    float rotation_threshold = 0.9f;
    
    bool speed_ok = fabs(velocity.x)<velocity_threshold && fabs(velocity.y)<velocity_threshold;
    bool rotation_ok = fabs(1 - rotation.x)<rotation_threshold && fabs(rotation.y)<rotation_threshold;
    
    //DEBUG MESSAGES
    if (speed_ok) printf("Speed OK\n");
    else printf("Ship is going too fast\n");
    
    if (rotation_ok) printf("Rotation OK\n");
    else printf("Ship is upside down\n");
    
    if (speed_ok && rotation_ok) {
      cpCollisionType ta = cpShapeGetCollisionType(a);
      cpCollisionType tb = cpShapeGetCollisionType(b);
      
      if (ta==LANDING_TYPE || tb==LANDING_TYPE) {
        printf("LANDING SPOT\n");
        
        Terrain::LandingSpot* landing = nullptr;
          
        //Check if terrain's box is a LandingSpot
        for (int i=0; i < terrain->landings_.size() && landing==nullptr; i++) {
          
          for (int j=0; j < terrain->landings_[i].shapes.size() && landing==nullptr; j++) {
          
            if (terrain->landings_[i].shapes[j] == a || terrain->landings_[i].shapes[j] == b) {
              landing = &terrain->landings_[i];
              printf("LANDING SHAPE FOUND\n");
              g_score += landing->points;
            } 
          } 
        }
        
      } else {
        printf("CRASHED SOMEWHERE\n");
        ship->crashed_ = true;
      }
    } else {
      ship->crashed_ = true;
    }
  }
  
  return cpTrue; //If false, collision is interrupted
}


void startGame() {
  srand(time(NULL));
  
  g_space = cpSpaceNew();
  g_ship = new Ship();
  g_terrain = new Terrain();
  g_colliders = (Colliders*)malloc(sizeof(Colliders));
  
//  space = cpSpaceNew();
  cpSpaceSetGravity(g_space, cpv(0, 0.0098f));
  
  g_ship->space_ = g_space;
  MathLib::Vec2 offset = {0.0f, 0.0f};
  g_ship->assignRegularPolygon(20, 10, offset, 0, g_ship->cvertices_, &g_ship->num_cvertices_);
  offset.y = 5.0f;
  g_ship->assignRegularPolygon(3, 10, offset, 225, g_ship->tvertices_, &g_ship->num_tvertices_);
  g_ship->setPhysics();
  
  g_terrain->space_ = g_space;
  g_terrain->generate();
  g_terrain->createWalls();
  
  g_colliders->ship = g_ship;
  g_colliders->terrain = g_terrain;
  
  //Set up default collision handler
  cpCollisionHandler* handler = cpSpaceAddDefaultCollisionHandler(g_space);
  handler->beginFunc = OnCollisionEnter;
  handler->userData = g_colliders;
}


int ESAT::main(int argc, char **argv) {
  
  startGame();
  
  ESAT::WindowInit(WindowOptions.width, WindowOptions.height);
  
  bg = ESAT::SpriteFromFile("assets/img/sky.jpg");
  
  //Init font
  ESAT::DrawSetTextFont("assets/font/medieval.ttf");
  ESAT::DrawSetTextSize(20);
  
  double last_time = ESAT::Time();
  
  
  
  while(ESAT::WindowIsOpened() && !ESAT::IsSpecialKeyDown(ESAT::kSpecialKey_Escape)) {

    /****************SIMULATION****************/
    double tick = ESAT::Time();
    double delta = (tick - last_time) * 0.1f;
    last_time = ESAT::Time();
    
    cpSpaceStep(g_space, delta);
    
    
    
    g_ship->pos_.x = cpBodyGetPosition(g_ship->physics_body_).x;
    g_ship->pos_.y = cpBodyGetPosition(g_ship->physics_body_).y;
    /*****************************************/
    
    if ((g_ship->exploding_ || g_ship->landed_)  && ESAT::IsSpecialKeyPressed(ESAT::kSpecialKey_Space)) {
      startGame();
    }
    
    ESAT::DrawBegin();
    ESAT::DrawClear(255,255,255);
  
    ESAT::DrawSprite(bg, 0, 0);
    
    g_ship->update();
    
    drawInfo(g_ship);
    
    g_terrain->draw();
    g_ship->draw();
    
    
    ESAT::DrawEnd();
    ESAT::WindowFrame();
  }
  ESAT::WindowDestroy();
  return 0;
}