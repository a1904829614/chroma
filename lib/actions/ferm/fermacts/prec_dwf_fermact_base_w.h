// -*- C++ -*-
// $Id: prec_dwf_fermact_base_w.h,v 1.4 2004-01-23 10:35:36 bjoo Exp $
/*! \file
 *  \brief Base class for even-odd preconditioned domain-wall-like fermion actions
 */

#ifndef __prec_dwf_fermact_base_w_h__
#define __prec_dwf_fermact_base_w_h__

#include "fermact.h"

using namespace QDP;

//! Base class for unpreconditioned domain-wall-like fermion actions
/*! \ingroup fermact
 *
 * Unprecondition domain-wall fermion action. The conventions used here
 * are specified in Phys.Rev.D63:094505,2001 (hep-lat/0005002).
 */

class EvenOddPrecDWFermActBase : public EvenOddPrecWilsonTypeFermAct<LatticeDWFermion>
{
public:
  //! Return the quark mass
  virtual Real quark_mass() const = 0;

  //! Produce a linear operator for this action but with quark mass 1
  virtual const EvenOddPrecLinearOperator<LatticeDWFermion>* linOpPV(Handle<const ConnectState> state) const = 0;\

  //! Define quark propagator routine for 4D fermions
  void qprop(LatticeFermion& psi, 
	     Handle<const ConnectState> state, 
	     const LatticeFermion& chi, 
	     enum InvType invType,
	     const Real& RsdCG, 
	     int MaxCG, int& ncg_had) const;
};

#endif
