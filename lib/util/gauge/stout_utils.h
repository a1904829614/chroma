// -*- C++ -*-
// $Id: stout_utils.h,v 1.2 2006-12-10 01:56:26 edwards Exp $
/*! \file
 *  \brief Stout utilities
 */

#ifndef STOUT_UTILS_H
#define STOUT_UTILS_H

#include "chromabase.h"

namespace Chroma 
{

  /*!
   * Stouting
   *
   * \ingroup gauge
   *
   * @{
   */

  /*! \ingroup gauge */
  namespace StoutLinkTimings { 
    double getForceTime();
    double getSmearingTime();
    double getFunctionsTime();
  }

  /*! \ingroup gauge */
  namespace Stouting 
  {
    //! Given field U, construct the staples into C, form Q and Q^2 and compute  c0 and c1
    void getQsandCs(const multi1d<LatticeColorMatrix>& u, 
		    LatticeColorMatrix& Q, 
		    LatticeColorMatrix& QQ,
		    LatticeColorMatrix& C, 
		    int mu,
		    const multi1d<bool>& smear_in_this_dirP,
		    const multi2d<Real>& rho);

    //! Given c0 and c1 compute the f-s and b-s
    /*! Only compute b-s if do_bs is set to true (default) */
    void getFsAndBs(const LatticeColorMatrix& Q,
		    const LatticeColorMatrix& QQ,
		    multi1d<LatticeComplex>& f,
		    multi1d<LatticeComplex>& b1,
		    multi1d<LatticeComplex>& b2,
		    bool do_bs=true);
    
    //! Do the smearing from level i to level i+1
    void smear_links(const multi1d<LatticeColorMatrix>& current,
		     multi1d<LatticeColorMatrix>& next, 
		     const multi1d<bool>& smear_in_this_dirP,
		     const multi2d<Real>& rho);
    
    //! Do the force recursion from level i+1, to level i
    void deriv_recurse(multi1d<LatticeColorMatrix>&  F,
		       const multi1d<bool>& smear_in_this_dirP,
		       const multi2d<Real>& rho,
		       const multi1d<LatticeColorMatrix>& u);

  }

  /*! @} */   // end of group gauge
}

#endif
