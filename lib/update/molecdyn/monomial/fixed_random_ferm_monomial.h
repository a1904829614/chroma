#ifndef FIXED_RANDOM_FERM_MONOMIAL_H
#define FIXED_RANDOM_FERM_MONOMIAL_H


#include "abs_monomial.h"
#include "chromabase.h"
#include "util/gauge/reunit.h"
#include "io/param_io.h"
#include "actions/ferm/fermacts/stout_fermstate_w.h"
#include "util/gauge/taproj.h"

using namespace QDP;

namespace Chroma { 

  namespace FixedRandomFermMonomial4DEnv {
     extern const std::string name;
     extern const bool registered;
   }

   struct FixedRandomFermMonomialParams {
     FixedRandomFermMonomialParams();

     FixedRandomFermMonomialParams(XMLReader& in, const std::string& path) {
       XMLReader paramtop(in, path);
       fermstate = readXMLGroup(paramtop, "FermState", "Name");
     }

     GroupXML_t fermstate;
   };


   class FixedRandomFermMonomial4D : public ExactFermMonomial<multi1d<LatticeColorMatrix>,multi1d<LatticeColorMatrix>,LatticeFermion>
  {
   public:

     typedef multi1d<LatticeColorMatrix> P;
     typedef multi1d<LatticeColorMatrix> Q;
     typedef LatticeFermion Phi;

     ~FixedRandomFermMonomial4D() {} // Destructor is automatic

     FixedRandomFermMonomial4D(const FixedRandomFermMonomialParams& p_);
     FixedRandomFermMonomial4D(const FixedRandomFermMonomial4D& m) : cfs(m.cfs) {
       X.resize(Nd);
       X = m.X;
     }

     void dsdq(P& F, const AbsFieldState<P,Q>& s);

     Double S(const AbsFieldState<P,Q>& s);

     //! Refresh pseudofermions
     void refreshInternalFields(const AbsFieldState<P,Q>& field_state) {
       // No Internal fields
     }
     
     //! Copy pseudofermions if any
     virtual void setInternalFields(const Monomial<P,Q>& m) {
       // No Internal Fields
     }

  private:
    multi1d<LatticeColorMatrix> X;
    Handle< CreateStoutFermState > cfs;

  };

};

#endif

