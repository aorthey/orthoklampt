#include "ompl_space.h"

CSpaceOMPL::CSpaceOMPL(Robot *robot_, CSpace *kspace_):
  robot(robot_), kspace(kspace_)
{
}

GeometricCSpaceOMPL::GeometricCSpaceOMPL(Robot *robot_, CSpace *kspace_):
  CSpaceOMPL(robot_, kspace_)
{
}

void GeometricCSpaceOMPL::initSpace()
{
  //###########################################################################
  // Create OMPL state space
  //   Create an SE(3) x R^n state space
  //###########################################################################
  if(!(robot->joints[0].type==RobotJoint::Floating))
  {
    std::cout << "[MotionPlanner] only supports robots with a configuration space equal to SE(3) x R^n" << std::endl;
    exit(0);
  }

  double N = robot->q.size()-6;
  std::cout << "[MotionPlanner] Robot CSpace: SE(3) x R^"<<N<<std::endl;

  ob::StateSpacePtr SE3(std::make_shared<ob::SE3StateSpace>());
  ob::StateSpacePtr Rn(std::make_shared<ob::RealVectorStateSpace>(N));

  this->space = SE3 + Rn;
  ob::SE3StateSpace *cspaceSE3 = this->space->as<ob::CompoundStateSpace>()->as<ob::SE3StateSpace>(0);
  ob::RealVectorStateSpace *cspaceRn = this->space->as<ob::CompoundStateSpace>()->as<ob::RealVectorStateSpace>(1);

  //ob::CompoundStateSpace *cspace = this->space->as<ob::CompoundStateSpace>();
  //cspace->setSubspaceWeight(0,1);
  //cspace->setSubspaceWeight(1,0);

  //###########################################################################
  // Set bounds
  //###########################################################################
  std::vector<double> minimum, maximum;
  minimum = robot->qMin;
  maximum = robot->qMax;

  assert(minimum.size() == 6+N);
  assert(maximum.size() == 6+N);

  vector<double> lowSE3;
  lowSE3.push_back(minimum.at(0));
  lowSE3.push_back(minimum.at(1));
  lowSE3.push_back(minimum.at(2));
  vector<double> highSE3;
  highSE3.push_back(maximum.at(0));
  highSE3.push_back(maximum.at(1));
  highSE3.push_back(maximum.at(2));

  ob::RealVectorBounds boundsSE3(3);
  boundsSE3.low = lowSE3;
  boundsSE3.high = highSE3;
  cspaceSE3->setBounds(boundsSE3);
  boundsSE3.check();

  vector<double> lowRn, highRn;
  for(int i = 0; i < N; i++){
    lowRn.push_back(minimum.at(i+6));
    highRn.push_back(maximum.at(i+6));
  }
  ob::RealVectorBounds boundsRn(N);

  //ompl does only accept dimensions with strictly positive measure, adding some epsilon space
  double epsilonSpacing=1e-8;
  for(int i = 0; i < N; i++){
    if(abs(lowRn.at(i)-highRn.at(i))<epsilonSpacing){
      highRn.at(i)+=epsilonSpacing;
    }
  }

  boundsRn.low = lowRn;
  boundsRn.high = highRn;
  boundsRn.check();
  cspaceRn->setBounds(boundsRn);

}
void GeometricCSpaceOMPL::initControlSpace()
{
  uint NdimControl = robot->q.size();

  this->control_space = std::make_shared<oc::RealVectorControlSpace>(space, NdimControl+1);
  ob::RealVectorBounds cbounds(NdimControl+1);
  cbounds.setLow(-1);
  cbounds.setHigh(1);

  uint effectiveControlDim = 0;
  for(int i = 0; i < NdimControl; i++){
    double qmin = robot->qMin(i);
    double qmax = robot->qMax(i);
    double d = sqrtf((qmin-qmax)*(qmin-qmax));
    if(d<1e-8){
      //remove zero-measure dimensions for control
      cbounds.setLow(i,0);
      cbounds.setHigh(i,0);
    }else{
      effectiveControlDim++;
      double dqmin = robot->velMin(i);
      double dqmax = robot->velMax(i);
      if(dqmin<-1) dqmin=-1;
      if(dqmax>1) dqmax=1;
      cbounds.setLow(i,dqmin);
      cbounds.setHigh(i,dqmax);
    }
  }

  cbounds.setLow(NdimControl,0.01);//propagation step size
  cbounds.setHigh(NdimControl,0.10);

  cbounds.check();
  control_space->setBounds(cbounds);
}

ob::State* GeometricCSpaceOMPL::ConfigToOMPLStatePtr(const Config &q){
  ob::ScopedState<> qompl = this->ConfigToOMPLState(q);

  ob::SE3StateSpace::StateType *qomplSE3 = qompl->as<ob::CompoundState>()->as<ob::SE3StateSpace::StateType>(0);
  ob::SO3StateSpace::StateType *qomplSO3 = &qomplSE3->rotation();
  ob::RealVectorStateSpace::StateType *qomplRn = qompl->as<ob::CompoundState>()->as<ob::RealVectorStateSpace::StateType>(1);
  //double* qomplRn = static_cast<ob::RealVectorStateSpace::StateType*>(qomplRnSpace)->values;

  ob::State* out = space->allocState();
  ob::SE3StateSpace::StateType *outSE3 = out->as<ob::CompoundState>()->as<ob::SE3StateSpace::StateType>(0);
  ob::SO3StateSpace::StateType *outSO3 = &outSE3->rotation();
  ob::RealVectorStateSpace::StateType *outRn = out->as<ob::CompoundState>()->as<ob::RealVectorStateSpace::StateType>(1);

  outSE3 = qomplSE3;
  outSO3 = qomplSO3;
  outRn = qomplRn;

  return out;
}
ob::ScopedState<> GeometricCSpaceOMPL::ConfigToOMPLState(const Config &q){
  ob::ScopedState<> qompl(space);

  ob::SE3StateSpace::StateType *qomplSE3 = qompl->as<ob::CompoundState>()->as<ob::SE3StateSpace::StateType>(0);
  ob::SO3StateSpace::StateType *qomplSO3 = &qomplSE3->rotation();
  ob::RealVectorStateSpace::StateType *qomplRnSpace = qompl->as<ob::CompoundState>()->as<ob::RealVectorStateSpace::StateType>(1);
  double* qomplRn = static_cast<ob::RealVectorStateSpace::StateType*>(qomplRnSpace)->values;

  qomplSE3->setXYZ(q[0],q[1],q[2]);
  qomplSO3->setIdentity();

  //q SE3: X Y Z yaw pitch roll
  //double yaw = q[3];
  //double pitch = q[4];
  //double roll = q[5];

  Math3D::EulerAngleRotation Reuler(q(5),q(4),q(3));
  Matrix3 R;
  Reuler.getMatrixXYZ(R);

  QuaternionRotation qr;
  qr.setMatrix(R);

  double qx,qy,qz,qw;
  qr.get(qw,qx,qy,qz);

  qomplSO3->x = qx;
  qomplSO3->y = qy;
  qomplSO3->z = qz;
  qomplSO3->w = qw;

  //Math3D::Matrix3 qrM;
  //qr.getMatrix(qrM);

  for(int i = 0; i < q.size()-6; i++){
    qomplRn[i]=q(6+i);
  }
  return qompl;
}
Config GeometricCSpaceOMPL::OMPLStateToConfig(const ob::SE3StateSpace::StateType *qomplSE3, const ob::RealVectorStateSpace::StateType *qomplRnState){
  const ob::SO3StateSpace::StateType *qomplSO3 = &qomplSE3->rotation();

  //std::vector<double> reals;
  //s->copyToReals(reals, qomplRnState);
  uint N =  space->getDimension() - 6;

  Config q;
  q.resize(6+N);

  q(0) = qomplSE3->getX();
  q(1) = qomplSE3->getY();
  q(2) = qomplSE3->getZ();

  double qx = qomplSO3->x;
  double qy = qomplSO3->y;
  double qz = qomplSO3->z;
  double qw = qomplSO3->w;

  Math3D::QuaternionRotation qr(qw, qx, qy, qz);
  Math3D::Matrix3 qrM;
  qr.getMatrix(qrM);
  Math3D::EulerAngleRotation R;
  R.setMatrixXYZ(qrM);

  q(3) = R[2];
  q(4) = R[1];
  q(5) = R[0];

  if(q(3)<-M_PI) q(3)+=2*M_PI;
  if(q(3)>M_PI) q(3)-=2*M_PI;

  if(q(4)<-M_PI/2) q(4)+=M_PI;
  if(q(4)>M_PI/2) q(4)-=M_PI;

  if(q(5)<-M_PI) q(5)+=2*M_PI;
  if(q(5)>M_PI) q(5)-=2*M_PI;

  for(int i = 0; i < N; i++){
    q(i+6) = qomplRnState->values[i];
  }

  return q;
}
Config GeometricCSpaceOMPL::OMPLStateToConfig(const ob::State *qompl){
  const ob::SE3StateSpace::StateType *qomplSE3 = qompl->as<ob::CompoundState>()->as<ob::SE3StateSpace::StateType>(0);
  const ob::RealVectorStateSpace::StateType *qomplRnState = qompl->as<ob::CompoundState>()->as<ob::RealVectorStateSpace::StateType>(1);

  return OMPLStateToConfig(qomplSE3, qomplRnState);

}

Config GeometricCSpaceOMPL::OMPLStateToConfig(const ob::ScopedState<> &qompl){

  const ob::SE3StateSpace::StateType *qomplSE3 = qompl->as<ob::CompoundState>()->as<ob::SE3StateSpace::StateType>(0);
  const ob::RealVectorStateSpace::StateType *qomplRnState = qompl->as<ob::CompoundState>()->as<ob::RealVectorStateSpace::StateType>(1);
  return OMPLStateToConfig(qomplSE3, qomplRnState);
}

const oc::StatePropagatorPtr GeometricCSpaceOMPL::StatePropagatorPtr(oc::SpaceInformationPtr si)
{
  return std::make_shared<PrincipalFibreBundleIntegrator>(si, this);
}
const ob::StateValidityCheckerPtr GeometricCSpaceOMPL::StateValidityCheckerPtr(oc::SpaceInformationPtr si)
{
  return std::make_shared<PrincipalFibreBundleOMPLValidityChecker>(si, kspace, this);
}

//#############################################################################
//#############################################################################
//#############################################################################
KinodynamicCSpaceOMPL::KinodynamicCSpaceOMPL(Robot *robot_, CSpace *kspace_):
  CSpaceOMPL(robot_, kspace_)
{

}

void KinodynamicCSpaceOMPL::initSpace()
{
  //###########################################################################
  // Create OMPL state space
  //   Create an SE(3) x R^n space + a R^{6+N} space
  //###########################################################################
  if(!(robot->joints[0].type==RobotJoint::Floating))
  {
    std::cout << "[MotionPlanner] only supports robots with a configuration space equal to SE(3) x R^n" << std::endl;
    exit(0);
  }

  double N = robot->q.size()-6;
  std::cout << "[MotionPlanner] Robot CSpace: M = SE(3) x R^" << N << std::endl;
  std::cout << "[MotionPlanner] Tangent Bundle: TM = M x R^{6+" << N << "}" << std::endl;

  ob::StateSpacePtr SE3(std::make_shared<ob::SE3StateSpace>());
  ob::StateSpacePtr Rn(std::make_shared<ob::RealVectorStateSpace>(N));
  ob::StateSpacePtr TM(std::make_shared<ob::RealVectorStateSpace>(6+N));

  space = SE3 + Rn + TM;
  ob::SE3StateSpace *cspaceSE3 = space->as<ob::CompoundStateSpace>()->as<ob::SE3StateSpace>(0);
  ob::RealVectorStateSpace *cspaceRn = space->as<ob::CompoundStateSpace>()->as<ob::RealVectorStateSpace>(1);
  ob::RealVectorStateSpace *cspaceTM = space->as<ob::CompoundStateSpace>()->as<ob::RealVectorStateSpace>(2);

  ob::CompoundStateSpace *cspace = space->as<ob::CompoundStateSpace>();

  //###########################################################################
  // Set position bounds
  //###########################################################################
  std::vector<double> minimum, maximum;
  minimum = robot->qMin;
  maximum = robot->qMax;

  assert(minimum.size() == 6+N);
  assert(maximum.size() == 6+N);

  vector<double> lowSE3;
  lowSE3.push_back(minimum.at(0));
  lowSE3.push_back(minimum.at(1));
  lowSE3.push_back(minimum.at(2));
  vector<double> highSE3;
  highSE3.push_back(maximum.at(0));
  highSE3.push_back(maximum.at(1));
  highSE3.push_back(maximum.at(2));

  ob::RealVectorBounds boundsSE3(3);
  boundsSE3.low = lowSE3;
  boundsSE3.high = highSE3;
  boundsSE3.check();
  cspaceSE3->setBounds(boundsSE3);

  vector<double> lowRn, highRn;
  for(int i = 0; i < N; i++){
    lowRn.push_back(minimum.at(i+6));
    highRn.push_back(maximum.at(i+6));
  }
  ob::RealVectorBounds boundsRn(N);

  //ompl does only accept dimensions with strictly positive measure, adding some epsilon space
  double epsilonSpacing=1e-8;
  for(int i = 0; i < N; i++){
    if(abs(lowRn.at(i)-highRn.at(i))<epsilonSpacing){
      highRn.at(i)+=epsilonSpacing;
    }
  }
  boundsRn.low = lowRn;
  boundsRn.high = highRn;
  boundsRn.check();
  cspaceRn->setBounds(boundsRn);
  //###########################################################################
  // Set velocity bounds
  //###########################################################################
  std::vector<double> vMin, vMax;
  vMin = robot->velMin;
  vMax = robot->velMax;

  assert(vMin.size() == 6+N);
  assert(vMax.size() == 6+N);


  vector<double> lowTM, highTM;
  for(int i = 0; i < 6+N; i++){
    lowTM.push_back(vMin.at(i));
    highTM.push_back(vMax.at(i));
  }

  ob::RealVectorBounds boundsTM(6+N);
  boundsTM.low = lowTM;
  boundsTM.high = highTM;
  boundsTM.check();
  boundsTM.setLow(-100);
  boundsTM.setHigh(100);
  cspaceTM->setBounds(boundsTM);

  //std::cout << "velocity bounds" << std::endl;
  //for(int i = 0; i < N+6; i++){
  //  std::cout << i << " <" << boundsTM.low.at(i) << ","<< boundsTM.high.at(i) << ">" << std::endl;
  //}

}
void KinodynamicCSpaceOMPL::initControlSpace(){
  uint NdimControl = robot->q.size();
  this->control_space = std::make_shared<oc::RealVectorControlSpace>(space, NdimControl+1);

  Vector torques = robot->torqueMax;
  //std::cout << "TORQUES robot:" << std::endl;
  //std::cout << robot->q.size() << std::endl;
  //std::cout << torques.size() << std::endl;
  //std::cout << torques << std::endl;

  ob::RealVectorBounds cbounds(NdimControl+1);
  cbounds.setLow(-1);
  cbounds.setHigh(1);

  uint effectiveControlDim = 0;
  for(int i = 0; i < NdimControl; i++){
    double qmin = robot->qMin(i);
    double qmax = robot->qMax(i);
    double d = sqrtf((qmin-qmax)*(qmin-qmax));
    if(d<1e-8){
      //remove zero-measure dimensions for control
      cbounds.setLow(i,0);
      cbounds.setHigh(i,0);
    }else{
      effectiveControlDim++;
      double tmin = -torques(i);
      double tmax = torques(i);
      //if(tmin<-1) tmin=-1;
      //if(tmax>1) tmax=1;
      cbounds.setLow(i,tmin);
      cbounds.setHigh(i,tmax);
    }
  }

  cbounds.setLow(NdimControl,input.timestep_min);//propagation step size
  cbounds.setHigh(NdimControl,input.timestep_max);

  //TODO: remove hardcoded se(3) vector fields
  for(int i = 0; i < 6; i++){
    cbounds.setLow(i,0);
    cbounds.setHigh(i,0);
  }
  //cbounds.setLow(0,0);
  //cbounds.setHigh(0,1);
  //cbounds.setLow(1,0);
  //cbounds.setHigh(1,1);
  cbounds.setLow(3,-0.1);
  cbounds.setHigh(3,0.1);
  //cbounds.setLow(4,-0.01);
  //cbounds.setHigh(4,0.01);
  //cbounds.setLow(5,-1);
  //cbounds.setHigh(5,1);

  cbounds.check();
  control_space->setBounds(cbounds);
  //std::cout << "torque bounds" << std::endl;
  //for(int i = 0; i < NdimControl+1; i++){
  //  std::cout << i << " <" << cbounds.low.at(i) << ","<< cbounds.high.at(i) << ">" << std::endl;
  //}
}

ob::State* KinodynamicCSpaceOMPL::ConfigToOMPLStatePtr(const Config &q){
  ob::ScopedState<> qompl = this->ConfigToOMPLState(q);

  ob::SE3StateSpace::StateType *qomplSE3 = qompl->as<ob::CompoundState>()->as<ob::SE3StateSpace::StateType>(0);
  ob::SO3StateSpace::StateType *qomplSO3 = &qomplSE3->rotation();
  ob::RealVectorStateSpace::StateType *qomplRn = qompl->as<ob::CompoundState>()->as<ob::RealVectorStateSpace::StateType>(1);
  ob::RealVectorStateSpace::StateType *qomplTM = qompl->as<ob::CompoundState>()->as<ob::RealVectorStateSpace::StateType>(2);

  ob::State* out = space->allocState();
  ob::SE3StateSpace::StateType *outSE3 = out->as<ob::CompoundState>()->as<ob::SE3StateSpace::StateType>(0);
  ob::SO3StateSpace::StateType *outSO3 = &outSE3->rotation();
  ob::RealVectorStateSpace::StateType *outRn = out->as<ob::CompoundState>()->as<ob::RealVectorStateSpace::StateType>(1);
  ob::RealVectorStateSpace::StateType *outTM = out->as<ob::CompoundState>()->as<ob::RealVectorStateSpace::StateType>(2);

  outSE3 = qomplSE3;
  outSO3 = qomplSO3;
  outRn = qomplRn;
  outTM = qomplTM;

  return out;
}
ob::ScopedState<> KinodynamicCSpaceOMPL::ConfigToOMPLState(const Config &q){
  ob::ScopedState<> qompl(space);

  ob::SE3StateSpace::StateType *qomplSE3 = qompl->as<ob::CompoundState>()->as<ob::SE3StateSpace::StateType>(0);
  ob::SO3StateSpace::StateType *qomplSO3 = &qomplSE3->rotation();
  ob::RealVectorStateSpace::StateType *qomplRnSpace = qompl->as<ob::CompoundState>()->as<ob::RealVectorStateSpace::StateType>(1);
  ob::RealVectorStateSpace::StateType *qomplTMSpace = qompl->as<ob::CompoundState>()->as<ob::RealVectorStateSpace::StateType>(2);

  qomplSE3->setXYZ(q[0],q[1],q[2]);
  qomplSO3->setIdentity();

  //q SE3: X Y Z yaw pitch roll
  //double yaw = q[3];
  //double pitch = q[4];
  //double roll = q[5];

  Math3D::EulerAngleRotation Reuler(q(5),q(4),q(3));
  Matrix3 R;
  Reuler.getMatrixXYZ(R);

  QuaternionRotation qr;
  qr.setMatrix(R);

  double qx,qy,qz,qw;
  qr.get(qw,qx,qy,qz);

  qomplSO3->x = qx;
  qomplSO3->y = qy;
  qomplSO3->z = qz;
  qomplSO3->w = qw;

  //Math3D::Matrix3 qrM;
  //qr.getMatrix(qrM);

  double* qomplRn = static_cast<ob::RealVectorStateSpace::StateType*>(qomplRnSpace)->values;
  //q.size = 6 + N + (N+6)


  if(!(q.size() == space->getDimension())){
    if(q.size() == int(0.5*space->getDimension())){
      uint N = q.size()-6;
      assert(12+2*N == space->getDimension());
      for(int i = 0; i < N; i++){
        qomplRn[i]=q(6+i);
      }
      double* qomplTM = static_cast<ob::RealVectorStateSpace::StateType*>(qomplTMSpace)->values;
      for(int i = 0; i < (N+6); i++){
        qomplTM[i]=0.0;
      }
    }else{
      exit(0);
    }
  }else{
    uint N = int(0.5*q.size())-6;
    assert(12+2*N == space->getDimension());
    for(int i = 0; i < N; i++){
      qomplRn[i]=q(6+i);
    }
    double* qomplTM = static_cast<ob::RealVectorStateSpace::StateType*>(qomplTMSpace)->values;
    for(int i = 0; i < (N+6); i++){
      qomplTM[i]=q(N+6+i);
    }
  }


  return qompl;
}
Config KinodynamicCSpaceOMPL::OMPLStateToConfig(const ob::SE3StateSpace::StateType *qomplSE3, const ob::RealVectorStateSpace::StateType *qomplRnState, const ob::RealVectorStateSpace::StateType *qomplTMState){
  const ob::SO3StateSpace::StateType *qomplSO3 = &qomplSE3->rotation();

  //std::vector<double> reals;
  //s->copyToReals(reals, qomplRnState);
  uint N =  int(0.5*space->getDimension()) - 6;
  assert(12+2*N == space->getDimension());

  Config q;
  q.resize(12+2*N);

  q(0) = qomplSE3->getX();
  q(1) = qomplSE3->getY();
  q(2) = qomplSE3->getZ();

  double qx = qomplSO3->x;
  double qy = qomplSO3->y;
  double qz = qomplSO3->z;
  double qw = qomplSO3->w;

  Math3D::QuaternionRotation qr(qw, qx, qy, qz);
  Math3D::Matrix3 qrM;
  qr.getMatrix(qrM);
  Math3D::EulerAngleRotation R;
  R.setMatrixXYZ(qrM);

  q(3) = R[2];
  q(4) = R[1];
  q(5) = R[0];

  if(q(3)<-M_PI) q(3)+=2*M_PI;
  if(q(3)>M_PI) q(3)-=2*M_PI;

  if(q(4)<-M_PI/2) q(4)+=M_PI;
  if(q(4)>M_PI/2) q(4)-=M_PI;

  if(q(5)<-M_PI) q(5)+=2*M_PI;
  if(q(5)>M_PI) q(5)-=2*M_PI;

  for(int i = 0; i < N; i++){
    q(i+6) = qomplRnState->values[i];
  }
  for(int i = 0; i < (6+N); i++){
    q(i+6+N) = qomplTMState->values[i];
  }

  return q;
}
Config KinodynamicCSpaceOMPL::OMPLStateToConfig(const ob::State *qompl){
  const ob::SE3StateSpace::StateType *qomplSE3 = qompl->as<ob::CompoundState>()->as<ob::SE3StateSpace::StateType>(0);
  const ob::RealVectorStateSpace::StateType *qomplRnState = qompl->as<ob::CompoundState>()->as<ob::RealVectorStateSpace::StateType>(1);
  const ob::RealVectorStateSpace::StateType *qomplTMState = qompl->as<ob::CompoundState>()->as<ob::RealVectorStateSpace::StateType>(2);

  return OMPLStateToConfig(qomplSE3, qomplRnState, qomplTMState);

}

Config KinodynamicCSpaceOMPL::OMPLStateToConfig(const ob::ScopedState<> &qompl){

  const ob::SE3StateSpace::StateType *qomplSE3 = qompl->as<ob::CompoundState>()->as<ob::SE3StateSpace::StateType>(0);
  const ob::RealVectorStateSpace::StateType *qomplRnState = qompl->as<ob::CompoundState>()->as<ob::RealVectorStateSpace::StateType>(1);
  const ob::RealVectorStateSpace::StateType *qomplTMState = qompl->as<ob::CompoundState>()->as<ob::RealVectorStateSpace::StateType>(2);
  return OMPLStateToConfig(qomplSE3, qomplRnState, qomplTMState);
}


const oc::StatePropagatorPtr KinodynamicCSpaceOMPL::StatePropagatorPtr(oc::SpaceInformationPtr si)
{
  return std::make_shared<TangentBundleIntegrator>(si, robot, this);
}
const ob::StateValidityCheckerPtr KinodynamicCSpaceOMPL::StateValidityCheckerPtr(oc::SpaceInformationPtr si)
{
  return std::make_shared<TangentBundleOMPLValidityChecker>(si, kspace, this);
}

