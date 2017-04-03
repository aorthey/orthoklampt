#include <Library/KrisLibrary/math/vector.h>
typedef Vector Config;

// Continuous Piecewise-Linear Path on Euclidean Space without velocity information
//
// [0,L] -> R^n, piecewise linear
// [0,1] -> R^n, piecewise linear (normalized)
class PathPiecewiseLinearEuclidean
{

  protected:
    std::vector<Config> keyframes;
    uint Ndim;
    uint Nkeyframes;
    double length;
    std::vector<double> interLength;//interLength(i) length towards next milestone point from q(i)
    PathPiecewiseLinearEuclidean(){};

  public:
    static PathPiecewiseLinearEuclidean from_keyframes(const std::vector<Config> &keyframes);
    static PathPiecewiseLinearEuclidean from_keyframes(const std::vector<Vector3> &keyframes);


    // make it [0,1] -> R^n
    void Normalize(){
      double newLength =0.0;
      for(int i = 0; i < interLength.size(); i++){
        interLength.at(i) /= length;
        newLength+=interLength.at(i);

      }
      assert( fabs(newLength-1.0) < 1e-10);
      length = newLength;
    }
    std::vector<double> GetLengthVector() const{
      return interLength;
    }

    double GetLength() const{
      return length;
    }

    Vector3 EvalVec3(const double t) const{
      Config q = Eval(t);
      Vector3 v; v[0] = q(0); v[1] = q(1); v[2] = q(2);
      return v;
    }
    Vector3 EvalVec3Milestone(const int k) const{
      Config q;
      if(k<0) q= keyframes.front();
      else if(k>Nkeyframes) q= keyframes.back();
      else q = keyframes.at(k);
      Vector3 v; v[0] = q(0); v[1] = q(1); v[2] = q(2);
      return v;
    }
    Config EvalMilestone(const int k) const{
      if(k<0) return keyframes.front();
      if(k>Nkeyframes) return keyframes.back();
      return keyframes.at(k);
    }
    Config Eval(const double t) const{
      if(t<=0) return keyframes.front();
      if(t>=length) return keyframes.back();
      uint ictr = 0;
      double Tcum = 0;
      assert(interLength.size()==keyframes.size()-1);
      for(int i = 0; i < interLength.size(); i++){
        double Tnext = interLength.at(i);
        if((Tcum+Tnext)>=t){
          //t \in [Lcum, Lcum+Lnext]
          double tloc = (t-Tcum)/Tnext; //tloc \in [0,1]
          Config q1 = keyframes.at(i);
          Config q2 = keyframes.at(i+1);
          return q1 + tloc*(q2-q1);
        }
        Tcum+=Tnext;
      }
      //rounding errors could lead to the fact that the cumulative length is not
      //exactly 1. If t is sufficiently close, we just return the last keyframe.
      if(length-Tcum > 1e-10){
        std::cout << "length of path is significantly different from 1.0" << std::endl;
        std::cout << "length    : " << Tcum << "/" << 1.0 << std::endl;
        std::cout << "difference: " << 1.0-Tcum << std::endl;
        throw;
      }

      if(t>=Tcum){
        return keyframes.back();
      }

      std::cout << "Eval could not find point for value " << t << std::endl;
      throw;
    }
    virtual void interpolate(){
      length = 0;
      interLength.clear();
      for(int i = 0; i < Nkeyframes-1; i++){
        Config q1 = keyframes.at(i);
        Config q2 = keyframes.at(i+1);
        double d = (q1-q2).norm();
        length += d;
        interLength.push_back(d);
      }
    }
  
    virtual void info(){
      std::cout << std::string(80, '-') << std::endl;
      std::cout << "Path Length   : " << length << std::endl;
      std::cout << "Path Keyframes: " << Nkeyframes << std::endl;
      std::cout << "Path Dimensionality: " << Ndim << std::endl;
      std::cout << std::string(80, '-') << std::endl;
      double dstep = length/10.0;
      for(double d = 0; d < length; d+=dstep){
        std::cout << Eval(d) << std::endl;  
      }
      std::cout << Eval(length) << std::endl;  
    }

};

inline PathPiecewiseLinearEuclidean PathPiecewiseLinearEuclidean::from_keyframes(const std::vector<Config> &keyframes){
  PathPiecewiseLinearEuclidean path = PathPiecewiseLinearEuclidean();
  path.keyframes=keyframes;
  path.Ndim = keyframes.at(0).size();
  path.Nkeyframes = keyframes.size();
  path.interpolate();
  return path;
}
inline PathPiecewiseLinearEuclidean PathPiecewiseLinearEuclidean::from_keyframes(const std::vector<Vector3> &keyframes){
  PathPiecewiseLinearEuclidean path = PathPiecewiseLinearEuclidean();
  for(int i = 0; i < keyframes.size(); i++){
    //Vector3 x = keyframes.at(i);
    Config q;q.resize(3);q(0)=keyframes.at(i)[0];q(1)=keyframes.at(i)[1];q(2)=keyframes.at(i)[2];
    path.keyframes.push_back(q);
  }
  //path.keyframes=keyframes;
  path.Ndim = 3;
  path.Nkeyframes = keyframes.size();
  path.interpolate();
  return path;
}
