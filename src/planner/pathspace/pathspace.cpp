#include "planner/pathspace/pathspace.h"
#include "planner/pathspace/pathspace_input.h"
#include "algorithms/onetopic_reduction.h"
#include "elements/roadmap.h"
#include "planner/cspace/cspace.h"
#include "planner/cspace/cspace_factory.h"
#include "elements/swept_volume.h"
#include <Planning/RobotCSpace.h>
#include <ompl/base/PlannerData.h>

PathSpace::PathSpace(RobotWorld *world_, PathSpaceInput* input_):
  world(world_), input(input_)
{
  sv = NULL;
  roadmap = NULL;
  if(!input){
    std::cout << "Input does not exists" << std::endl;
    exit(0);
  }
}

RobotWorld* PathSpace::GetWorldPtr(){
  return world;
}
PathSpaceInput* PathSpace::GetPathSpaceInput(){
  return input;
}

void PathSpace::SetShortestPath(const ob::PathPtr p, CSpaceOMPL *cspace){
  path_ompl = new PathPiecewiseLinear(p, cspace);
}
PathPiecewiseLinear* PathSpace::getShortestPathOMPL(){
  return path_ompl;
}

const std::vector<Config>& PathSpace::GetVertices(){
  return vertices;
}
const std::vector<std::pair<Config,Config>>& PathSpace::GetEdges(){
  return edges;
}
//std::vector<std::vector<Config>> PathSpace::GetPaths(){
//  return paths;
//}
const RoadmapPtr PathSpace::GetRoadmap(){
  return roadmap;
}

void PathSpace::SetEdges(const std::vector<std::pair<Config,Config>>& edges_){
  edges = edges_;
}
void PathSpace::SetVertices(const std::vector<Config>& vertices_){
  vertices = vertices_;
}
//void PathSpace::SetPaths(const std::vector<std::vector<Config>>& paths_){
//  paths = paths_;
//}
void PathSpace::SetRoadmap(const RoadmapPtr roadmap_){
  roadmap = roadmap_;
}

SweptVolume& PathSpace::GetSweptVolume(Robot *robot){
  std::cout << "Needs reimplementation to OMPL pathptr" << std::endl;
  exit(0);
  //if(!sv){
  //  sv = new SweptVolume(robot, vantage_path, 0);
  //}
  return *sv;
}
std::ostream& operator<< (std::ostream& out, const PathSpace& space) 
{
  out << std::string(80, '-') << std::endl;
  out << "[PathSpace]" << std::endl;
  out << std::string(80, '-') << std::endl;
  std::cout << " atomic      : " << (space.isAtomic()?"yes":"no") << std::endl;
  std::cout << " init        : " << space.input->q_init << std::endl;
  std::cout << " goal        : " << space.input->q_goal << std::endl;
  std::cout << " type        : " << space.input->type << std::endl;
  std::cout << " #graph vertices          : " << space.vertices.size() << std::endl;
  std::cout << " #graph edges             : " << space.edges.size() << std::endl;
  //std::cout << " #paths size              : " << space.paths.size() << std::endl;
  //uint ii = space.input.robot_inner_idx;
  //uint io = space.input.robot_outer_idx;
  //std::cout << " robot inner : " << ii << " " << space.world->robots[ii]->name << std::endl;
  //std::cout << " robot outer : " << io << " " << space.world->robots[io]->name << std::endl;
  out << std::string(80, '-') << std::endl;
  return out;
}