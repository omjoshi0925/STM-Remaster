#include "Level.hpp"
#include <cstdio>
#include <cmath>
using namespace bdae;
int fails=0;
static void ck(bool c,const char*w,const std::string&d=""){std::printf("  [%s] %s%s%s\n",c?"PASS":"FAIL",w,d.empty()?"":"  -> ",d.c_str()); if(!c)++fails;}
int main(int argc,char**argv){
  const std::string root = (argc>1? argv[1] : "Assets"), dir="levelnew_01";
  IrrScene sc; std::string e;
  if(!sc.load(root+"/"+dir+"/levelnew_01_0_Room1.irr",e)){std::printf("FATAL %s\n",e.c_str());return 1;}
  std::printf("Room1.irr parsed: %zu nodes\n",sc.nodes.size());
  ck(sc.nodes.size()==60,"Room1 has 60 scene nodes");
  ck(sc.firstOfType("Geometry")!=nullptr,"Room1 declares Geometry");
  ck(sc.firstOfType("Collisions")!=nullptr,"Room1 declares Collisions");
  ck(sc.firstOfType("NavMesh")!=nullptr,"Room1 declares NavMesh");
  const IrrNode*g=sc.firstOfType("Geometry");
  std::printf("  Geometry meshFile = %s\n",g->meshFile.c_str());

  LevelRoom room;
  if(!room.load(root,dir,{"levelnew_01.irr","levelnew_01_0_Room1.irr"},e)){std::printf("FATAL %s\n",e.c_str());return 1;}
  std::printf("Room1 built: visual %zu verts/%zu tris  collision %zu verts/%zu tris  nav %zu verts/%zu tris\n",
    room.visualVertexCount(),room.visualTriangleCount(),
    room.collision.vertices.size(),room.collision.indices.size()/3,
    room.navmesh.vertices.size(),room.navmesh.indices.size()/3);
  std::printf("  visual bbox (%.0f %.0f %.0f)..(%.0f %.0f %.0f)\n",
    room.visualBatches.empty()?0:room.visualBatches[0].bboxMin.x,room.visualBatches.empty()?0:room.visualBatches[0].bboxMin.y,room.visualBatches.empty()?0:room.visualBatches[0].bboxMin.z,
    room.visualBatches.empty()?0:room.visualBatches[0].bboxMax.x,room.visualBatches.empty()?0:room.visualBatches[0].bboxMax.y,room.visualBatches.empty()?0:room.visualBatches[0].bboxMax.z);
  ck(!room.visualBatches.empty(),"visual geometry loaded");
  ck(!room.collision.empty(),"collision geometry loaded");
  ck(!room.navmesh.empty(),"navmesh loaded");
  std::printf("  enemies in room: %zu   markers: %zu\n",room.enemies.size(),room.markers.size());
  for(auto&en:room.enemies) std::printf("    %-24s (%.0f %.0f %.0f)\n",en.type.c_str(),en.pos.x,en.pos.y,en.pos.z);

  // spawn: search the whole level for the SpiderMan node
  const char* rooms[]={"levelnew_01_0_Room1.irr","levelnew_01_1_Room2.irr","levelnew_01_2_Room3.irr"};
  for(auto r:rooms){ IrrScene s2; std::string e2;
    if(!s2.load(root+"/"+dir+"/"+r,e2)) continue;
    for(auto*n:s2.allOfType("SpiderMan"))
      std::printf("  SpiderMan spawn in %s: name=%s pos=(%.1f %.1f %.1f)\n",r,n->name.c_str(),
         n->absolute.m[12],n->absolute.m[13],n->absolute.m[14]);
    for(auto*n:s2.allOfType("SpawnPoint"))
      std::printf("  SpawnPoint in %s: name=%s pos=(%.1f %.1f %.1f)\n",r,n->name.c_str(),
         n->absolute.m[12],n->absolute.m[13],n->absolute.m[14]);
  }

  // ground raycast sanity: sample navmesh centre
  float cx=(room.navmesh.bboxMin.x+room.navmesh.bboxMax.x)*0.5f;
  float cy=(room.navmesh.bboxMin.y+room.navmesh.bboxMax.y)*0.5f;
  int hits=0,tries=0;
  for(int i=0;i<11;i++)for(int j=0;j<11;j++){
    float x=room.navmesh.bboxMin.x+(room.navmesh.bboxMax.x-room.navmesh.bboxMin.x)*i/10.0f;
    float y=room.navmesh.bboxMin.y+(room.navmesh.bboxMax.y-room.navmesh.bboxMin.y)*j/10.0f;
    ++tries; if(groundBelow(room.navmesh,x,y,1e9f).hit)++hits;
  }
  GroundQuery c=groundBelow(room.navmesh,cx,cy,1e9f);
  std::printf("  navmesh grid coverage %d/%d, centre hit=%d z=%.2f normal=(%.2f %.2f %.2f)\n",
     hits,tries,(int)c.hit,c.z,c.normal.x,c.normal.y,c.normal.z);
  ck(hits>0,"navmesh ground query returns hits over the room footprint",
     std::to_string(hits)+"/"+std::to_string(tries)+" sample points");
  std::printf("\n%s (%d failures)\n",fails?"LEVELTEST FAILED":"LEVELTEST PASSED",fails);
  return fails?1:0;
}
