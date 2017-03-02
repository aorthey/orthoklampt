#include "planner.h"
#include "util.h"
#include "cspace_sentinel.h"
#include "cspace_epsilon_neighborhood.h"

MotionPlanner::MotionPlanner(RobotWorld *world, WorldSimulation *sim):
  _world(world),_sim(sim)
{
  _irobot = 0;
  _icontroller = 0;
}
const MultiPath& MotionPlanner::GetPath()
{
  return _path;
}
const SerializedTree& MotionPlanner::GetTree()
{
  return _stree;
}

void MotionPlanner::SerializeTreeNeighborhoodPrune(SerializedTree &_stree, double epsilon)
{
  //for(int i = 0; i < _stree.size(); i++){
  //  for(int j = i+1; j < _stree.size(); j++){

  //  }
  //}
//v.erase(std::remove_if(
//          v.begin(), v.end(),
//              [](const int& x) { return x > 10; }), 
//    v.end());
}


void MotionPlanner::SerializeTreeAddCostToGoal(SerializedTree &stree, CSpace *base, Config &goal)
{
  for(uint i = 0; i < stree.size(); i++){
    Config worldgoal;
    worldgoal.resize(6);
    for(int j = 0; j < 6; j++){
      worldgoal(j) = goal(j);
    }
    stree.at(i).cost_to_goal = base->Distance(stree.at(i).position, goal);
  }
}
void MotionPlanner::SerializeTree( const KinodynamicTree::Node* node, SerializedTree &stree){
  SerializedTreeNode snode;

  State s = *node;
  Vector position;
  position.resize(6);
  for(int i = 0; i < 6; i++){
    position(i) = s(i);
  }
  snode.position = position;

  std::vector<Vector3> directions;

  std::vector<KinodynamicTree::Node* > children;
  node->enumChildren(children);
  for(uint i = 0; i < children.size(); i++){
    Vector3 childdir;
    State c = *children.at(i);
    for(int j = 0; j < 3; j++){
      childdir[j] = c(j)-position(j);
    }
    directions.push_back(childdir);
    SerializeTree(children.at(i),stree);
  }
  snode.directions = directions;

  //DEBUG
  //std::cout << snode.first << std::endl;
  //std::cout << "Childrens: " << children.size() << std::endl;
  //for(int i = 0; i < directions.size(); i++){
  //  std::cout << snode.second.at(i) << std::endl;
  //}
  stree.push_back(snode);

}
void MotionPlanner::SerializeTree( const KinodynamicTree& tree, SerializedTree& stree){
  SerializeTree(tree.root, stree);
}
std::string MotionPlanner::getName(){
  return "Motion Planner";
}

void MotionPlanner::CheckFeasibility( Robot *robot, SingleRobotCSpace &cspace, Config &q){
  if(!cspace.IsFeasible(q)) {
    cout<<"configuration is infeasible, violated constraints:"<<endl;
    std::cout << q << std::endl;
    vector<bool> infeasible;
    cspace.CheckObstacles(q,infeasible);
    for(size_t i=0;i<infeasible.size();i++){
      //if(!infeasible[i]) cout<<"  OK"<<cspace.ObstacleName(i)<<endl;
      if(infeasible[i]){
        int icq = i - (int)robot->joints.size();
        cout<<"-->"<<cspace.ObstacleName(i) << " | pen-depth: ";
        cout << cspace.collisionQueries[icq].PenetrationDepth() << " | dist: ";
        cout << cspace.collisionQueries[icq].Distance(1e-3,1e-3) << std::endl;
      }
    }
    std::cout << cspace.collisionQueries.size() << std::endl;
    exit(0);
  }
}

bool MotionPlanner::PlanPath(){

}
bool MotionPlanner::solve(Config &p_init, Config &p_goal, double timelimit, bool shortcutting)
{

  this->_p_init = p_init;
  this->_p_goal = p_goal;
  this->_timelimit = timelimit;
  this->_shortcutting = shortcutting;
  Robot *robot = _world->robots[_irobot];
  robot->UpdateConfig(_p_init);
  util::SetSimulatedRobot(robot,*_sim,_p_init);
  this->_world->InitCollisions();

  std::cout << std::string(80, '-') << std::endl;

  std::cout << "Motion Planner:" << this->getName() << std::endl;
  std::cout << "p_init =" << p_init << std::endl;
  std::cout << "p_goal =" << p_goal << std::endl;

  std::cout << std::string(80, '-') << std::endl;

  //###########################################################################
  // Planner Settings
  //###########################################################################

//** Real   collisionEpsilon threshold for edge feasibility checks
//** Vector  distanceWeights for non-euclidean distance metric
//** AABB3D  worldBounds base position sampling range for free-floating robots
//** Real  contactEpsilon convergence threshold for contact solving
//** int   contactIKMaxIters max iters for contact solving
//** PropertyMap   properties other properties 
  WorldPlannerSettings settings;
  settings.InitializeDefault(*_world);
  Vector3 bmin(-3,-3,1);
  Vector3 bmax(+3,+3,1);
  //AABB3D worldBounds(bmin,bmax);
  settings.robotSettings[0].worldBounds = AABB3D(bmin,bmax);
  Vector weights;weights.resize(7);
  weights.setZero();
  weights(0) = 1;
  weights(1) = 1;
  weights(2) = 1;
  weights(3) = 0.2;
  weights(4) = 0.2;
  weights(5) = 0.2;
  //std::cout << weights << std::endl;
  //settings.robotSettings[0].distanceWeights = weights;
  //exit(0);
  //settings.robotSettings[0].contactEpsilon = 1e-2;
  //settings.robotSettings[0].contactIKMaxIters = 100;

  SingleRobotCSpace cspace = SingleRobotCSpace(*_world,_irobot,&settings);

  //Check Kinematic Feasibility at init/goal positions
  CheckFeasibility( robot, cspace, _p_init);
  CheckFeasibility( robot, cspace, _p_goal);

  //###########################################################################
  // Dynamic Planning
  //###########################################################################

  //for(int i = 0; i < 10; i++){
  //  std::cout << "[" << i << "/" << 10 << "] " << std::endl;
  //  Config x;
  //  cspace.Sample(x);
  //  std::cout << x << std::endl;
  //}
  //exit(0);
  KinodynamicCSpaceSentinelAdaptor kcspace(&cspace);

  CSpaceGoalSetEpsilonNeighborhood goalSet(&kcspace, _p_goal, 0.1);

  LazyRRTKinodynamicPlanner krrt(&kcspace);
  //RRTKinodynamicPlanner krrt(&kcspace);
  krrt.goalSeekProbability=0.2;
  krrt.goalSet = &goalSet;
  krrt.Init(_p_init);

  //RRTKinodynamicPlanner2 krrt(&kcspace);
  //krrt.Init(_p_init,_p_goal);

  //BidirectionalRRTKP
  //BidirectionalRRTKP krrt(&kcspace);
  //UnidirectionalRRTKP krrt(&kcspace);
  //
  PropertyMap pmap;
  kcspace.Properties(pmap);
  std::cout << pmap << std::endl;

  bool res = krrt.Plan(1e8);

  _stree.clear();
  SerializeTree(krrt.tree, _stree);
  SerializeTreeNeighborhoodPrune(_stree, 0.1);
  SerializeTreeAddCostToGoal(_stree, &kcspace, _p_goal);

  //SerializeTreeCost(krrt.tree, _stree, &goalSet);

  if(res)
  {
    KinodynamicMilestonePath path;
    krrt.CreatePath(path);

    double dstep = 0.01;
    Config cur;
    vector<Config> keyframes;
    for(double d = 0; d <= 1; d+=dstep)
    {
      path.Eval(d,cur);
      std::cout << d << cur << std::endl;
      keyframes.push_back(cur);
    }
    _path.SetMilestones(keyframes);
    //std::cout << "time optimizing" << std::endl;
    //double xtol=0.01;
    //double ttol=0.01;
    //bool res=GenerateAndTimeOptimizeMultiPath(*robot,_path,xtol,ttol);


    std::string date = util::GetCurrentTimeString();
    string out = "../data/paths/path_"+date+".xml";
    _path.Save(out);
    std::cout << "saved path to "<<out << std::endl;

  }
  return res;


  //###########################################################################
  // Kinematic Planning
  //###########################################################################
  ////MotionPlannerFactory factory;
  //////factory.perturbationRadius = 0.1;
  //////factory.type = "rrt";
  //////factory.type = "prm";
  //////factory.type = "sbl";
  //////factory.type = "sblprt";
  //////factory.type = "rrt*";
  //////factory.type = "lazyrrg*";
  //////factory.type = "fmm"; //warning: slows down system 
  //////factory.type = "sbl";
  ////factory.type = "rrt";
  ////factory.shortcut = this->_shortcutting;

  ////SmartPointer<MotionPlannerInterface> planner = factory.Create(&kcspace,_p_init,_p_goal);

  ////HaltingCondition cond;
  ////cond.foundSolution=true;
  ////cond.timeLimit = this->_timelimit;

  ////std::cout << "Start Planning" << std::endl;
  ////string res = planner->Plan(_milestone_path,cond);
  ////if(_milestone_path.edges.empty())
  ////{
  //// printf("Planning failed\n");
  //// this->_isSolved = false;
  ////}else{
  //// printf("Planning succeeded, path has length %g\n",_milestone_path.Length());
  //// this->_isSolved = true;
  ////}

  ////###########################################################################
  //// Time Optimize Path, Convert to MultiPath and Save Path
  ////###########################################################################
  //if(this->_isSolved){
  //  double dstep = 0.1;
  //  Config cur;
  //  vector<Config> keyframes;
  //  for(double d = 0; d <= 1; d+=dstep)
  //  {
  //    _milestone_path.Eval(d,cur);
  //    keyframes.push_back(cur);
  //  }
  //  _path.SetMilestones(keyframes);
  //  double xtol=0.01;
  //  double ttol=0.01;
  //  std::cout << "time optimizing" << std::endl;
  //  bool res=GenerateAndTimeOptimizeMultiPath(*robot,_path,xtol,ttol);


  //  std::string date = util::GetCurrentTimeString();
  //  string out = "../data/paths/path_"+date+".xml";
  //  _path.Save(out);
  //  std::cout << "saved path to "<<out << std::endl;
  //}else{
  //  std::cout << "Planner did not find a solution" << std::endl;
  //}
}

void MotionPlanner::SendCommandStringController(string cmd, string arg)
{
  if(!_sim->robotControllers[_icontroller]->SendCommand(cmd,arg)) {
    std::cout << std::string(80, '-') << std::endl;
    std::cout << "ERROR in controller commander" << std::endl;
    std::cout << cmd << " command  does not work with the robot's controller" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    throw "Controller command not supported!";
  }
}
bool MotionPlanner::SendToController()
{
  if(!_isSolved){ return false; }

  double dstep = 0.1;
  Config q;
  Config dq;

  for(double d = 0; d <= 1; d+=dstep)
  {
    _path.Evaluate(d, q, dq);
    stringstream qstr;
    qstr<<q<<dq;
    string cmd( (d<=0)?("set_qv"):("append_qv") );
    SendCommandStringController(cmd,qstr.str());
  }

  std::cout << "Sending Path to Controller" << std::endl;
  Robot *robot = _world->robots[_irobot];
  util::SetSimulatedRobot(robot,*_sim,_p_init);
  robot->UpdateConfig(_p_goal);
  std::cout << "Done Path to Controller" << std::endl;
  return true;
}

