// -*- C++ -*-
/*! \file
 *  \brief Hyp utilities
 */

#include "chroma_config.h"
#include "chromabase.h"
#include "util/gauge/hyp_utils.h"
#include "util/gauge/sun_proj.h"
#if QDP_NC != 3
#include "util/gauge/expm12.h"
#endif

#if defined(BUILD_JIT_CLOVER_TERM)
void function_hyp_smear_link_exec(JitFunction& function, 
                                  const LatticeColorMatrix& u, 
                                  LatticeColorMatrix& u_hyp,
                                  const Real alpha1,
                                  const Real alpha2,
                                  const Real alpha3,
                                  const int BlkMax,
                                  const Real BlkAccu);

void function_hyp_smear_link_build(JitFunction& function,
                                   const LatticeColorMatrix& u, 
                                   LatticeColorMatrix& u_hyp,
                                   const Real alpha1,
                                   const Real alpha2,
                                   const Real alpha3,
                                   const int BlkMax,
                                   const Real BlkAccu);

#endif


namespace Chroma 
{ 

  //! Timings
  /*! \ingroup gauge */
  namespace HypLinkTimings { 
    static double smearing_secs = 0;
    double getSmearingTime() { 
      return smearing_secs;
    }

    static double force_secs = 0;
    double getForceTime() { 
      return force_secs;
    }

    static double functions_secs = 0;
    double getFunctionsTime() { 
      return functions_secs;
    }
  }

  //! Utilities
  /*! \ingroup gauge */
  namespace Hyping 
  {    
    //! Do the force recursion from level i+1, to level i
    void deriv_recurse(multi1d<LatticeColorMatrix>& F,
		       const multi1d<bool>& smear_in_this_dirP,
                       const Real alpha1,
                       const Real alpha2,
                       const Real alpha3,
                       const int hyp_qr_max_iter,
                       const Real hyp_qr_tol,
                       const multi1d<LatticeColorMatrix>& U)
    {
      START_CODE();
      
      QDPIO::cout << "HYP deriv" << std::endl;
      
      // Save the fat force
      multi1d<LatticeColorMatrix> F_plus(Nd);
      F_plus = F;
      
      
      // Loop over lattice dimensions
      for(int mu=0; mu<Nd; mu++) {
        // Apply Upper Hessenberg reduction to the link
        if(smear_in_this_dirP[mu]) {
          
        }
      }
      
      END_CODE();
    }
    
    /* A namespace to hide the thread dispatcher in */
    namespace HypUtils { 
      
      struct HypUpperHessArgs { 

        // Site link
        const LatticeColorMatrix &U;

        // The Upper Hessenberg matrix
        LatticeColorMatrix &UH;
      };
      
      struct HypQRArgs { 

        // The Upper Hessenberg matrix to QR
        LatticeColorMatrix &UH;

        // QR accuracy
        Real hyp_qr_tol;
        int hyp_qr_maxiter;
      };

      struct HypVandermondeArgs { 

        // The Upper triangular matrix, evals on diagonal
        LatticeColorMatrix &UT;
        
        // The Cayley-Hamilton coeffs
        multi1d<LatticeComplex>& f;
      };

      
      inline void hypUpperHessSiteLoop(int lo, int hi, int myId, 
                                       HypUpperHessArgs* arg)
      {
#ifndef QDP_IS_QDPJIT
        // We follow the computation described in arxiv:1606.01277
        //--------------------------------------------------------------
        // 1. Compute V = Omega * Q^(1/2) | Q = (Omega^dag * Omega)
        //    To compute the square root for all Nc we reduce the SU(Nc)
        //    matrix to UpperHessenberg form, then perform a QR 
        //    decomposition to further reduce it to UpperTriangular.
        //    The eigenvalues will then lie on the diagonal.
        
        const LatticeColorMatrix &U = arg->U;   // The original links
        LatticeColorMatrix &UH = arg->UH; // Upper Hessenberg reduction
      
        REAL tol = 1e-15;

        for(int site=lo; site < hi; site++) { 
      
          // Site local Upper Hessenberg reduction, intitialised to U.
          PColorMatrix<QDP::RComplex<REAL>, Nc> UH_site = arg->U.elem(site).elem();
          // Reflector at iteration `site`
          PColorMatrix<QDP::RComplex<REAL>, Nc> Pi;
          
          // Dump matrix to UH
#if 0
          {
            multi1d<int> coord = Layout::siteCoords(Layout::nodeNumber(), site);
            if(coord[0] == 0 && coord[1] == 0 && coord[2] == 0 && coord[3] == 0) {
              QMP_fprintf(stdout, "%s: site=%d coord=[%d,%d,%d,%d] Matrix to UH\n",
                          __func__, site, coord[0], coord[1], coord[2], coord[3]);
              
              for(int j = 0; j < Nc; j++ ) {
                for(int k = 0; k < Nc; k++ ) {
                  QDPIO::cout << "(" << UH_site.elem(j,k).real() << "," << UH_site.elem(j,k).imag() << ") ";
                }              
                QDPIO::cout << std::endl;
              }
            }
          }
#endif

          for(int i = 0; i < Nc - 2; i++) {
            
            REAL col_norm, col_norm_inv;
            // get (partial) dot product of ith column vector
            col_norm = 0.0;
            for(int j = i+1; j < Nc; j++) col_norm += toDouble(localNorm2(UH_site.elem(j,i)));
            col_norm = sqrt(col_norm);

            PColorVector<QDP::RComplex<REAL>, Nc> v; // vector of length (1 x N)
            for(int j=0; j<Nc; j++) {
              v.elem(j).real() = toDouble(0.0);
              v.elem(j).imag() = toDouble(0.0);
            }
            
            QDP::RComplex<REAL> rho(1.0, 0.0);
            REAL abs_elem = sqrt(toDouble(localNorm2(UH_site.elem(i+1,i))));
            
            if(toDouble(abs_elem) > tol) {
              rho = UH_site.elem(i+1,i);
              rho.real() *= toDouble(-1.0)/toDouble(abs_elem);
              rho.imag() *= toDouble(-1.0)/toDouble(abs_elem);
            }
            
            v.elem(i+1).real() = UH_site.elem(i+1,i).real() - toDouble(col_norm) * rho.real();
            v.elem(i+1).imag() = UH_site.elem(i+1,i).imag() - toDouble(col_norm) * rho.imag();

            // reuse col_norm
            col_norm = toDouble(localNorm2(v.elem(i+1)));
            
            // copy the rest of the column
            for(int j = i + 2; j < Nc; j++ ) {
              v.elem(j) = UH_site.elem(j,i);
              col_norm += toDouble(localNorm2(v.elem(j)));
            }
            col_norm_inv = 1.0/std::max(sqrt(col_norm), 1e-30);

            // Normalise the column
            for(int j = i + 1; j < Nc; j++) {
              v.elem(j).real() *= toDouble(col_norm_inv);
              v.elem(j).imag() *= toDouble(col_norm_inv);
            }
                       
            // Reset Pi to the identity.
            for(int j = 0; j < Nc; j++ ) {
              for(int k = 0; k < Nc; k++ ) {
                Pi.elem(j,k).real() = (j == k ? 1.0 : 0.0);
                Pi.elem(j,k).imag() = 0.0;
              }
            }
            
            // Construct the householder matrix P = I - 2 v * v^{\dag}
            for(int j = i + 1; j < Nc; j++ ) {
              for(int k = i + 1; k < Nc; k++ ) {
                QDP::RComplex<REAL> P_elem;
                P_elem.real()  = v.elem(j).real() * v.elem(k).real();
                P_elem.real() += v.elem(j).imag() * v.elem(k).imag();
                P_elem.imag()  = v.elem(j).imag() * v.elem(k).real();
                P_elem.imag() -= v.elem(j).real() * v.elem(k).imag();
                Pi.elem(j,k).real() -= 2.0 * P_elem.real();
                Pi.elem(j,k).imag() -= 2.0 * P_elem.imag();
              }
            }
            
            // Similarity transform
            UH_site = Pi * UH_site * Pi;
          }
          
          // Load back into the lattice sized objects
          for(int j=0; j < Nc; j++) { 
            for(int k=0; k < Nc; k++) { 
              UH.elem(site).elem().elem(j,k).real() = UH_site.elem(j,k).real();
              UH.elem(site).elem().elem(j,k).imag() = UH_site.elem(j,k).imag();
            }
          }
        }
#endif
      } // End Function      

      inline void hypQRSiteLoop(int lo, int hi, int myId, 
                                HypQRArgs* arg)
      {
#ifndef QDP_IS_QDPJIT
      
        LatticeColorMatrix &UH = arg->UH; // Upper Hessenberg reduction
      
        Real tol = arg->hyp_qr_tol;
        int max_iter = arg->hyp_qr_maxiter;
        
        for(int site=lo; site < hi; site++) { 
        
          // Site local Upper Hessenberg reduction, intitialised to U.
          PColorMatrix<QDP::RComplex<REAL>, Nc> UH_site = arg->UH.elem(site).elem();
          PColorMatrix<QDP::RComplex<REAL>, Nc> Q;

          // Set Q to the identity.
          for(int j = 0; j < Nc; j++ ) {
            for(int k = 0; k < Nc; k++ ) {
              Q.elem(j,k).real() = (j == k ? 1.0 : 0.0);
              Q.elem(j,k).imag() = 0.0;
            }
          }
        
          // Dump matrix to QR
#if 0
          {
            multi1d<int> coord = Layout::siteCoords(Layout::nodeNumber(), site);
            if(coord[0] == 0 && coord[1] == 0 && coord[2] == 0 && coord[3] == 0) {
              QMP_fprintf(stdout, "%s: site=%d coord=[%d,%d,%d,%d] Matrix to QR\n",
                          __func__, site, coord[0], coord[1], coord[2], coord[3]);
              
              for(int j = 0; j < Nc; j++ ) {
                for(int k = 0; k < Nc; k++ ) {
                  QDPIO::cout << "(" << UH_site.elem(j,k).real() << "," << UH_site.elem(j,k).imag() << ") ";
                }              
                QDPIO::cout << std::endl;
              }
            }
          }
#endif
          
          int iter = 0;
        
          QDP::RComplex<REAL> temp, discriminant, sol1, sol2, eval;
          for (int i = Nc - 2; i >= 0; i--) {
            while (iter < max_iter) {
              if (sqrt(toDouble(localNorm2(UH_site.elem(i+1,i)))) < toDouble(tol) ) {
                UH_site.elem(i+1,i).real() = 0.0;
                UH_site.elem(i+1,i).imag() = 0.0;
                break;
              } else {
              
                // Compute the 2 eigenvalues via the quadratic formula
                //----------------------------------------------------
                // The discriminant
                temp = (UH_site.elem(i,i) - UH_site.elem(i+1,i+1)) * (UH_site.elem(i,i) - UH_site.elem(i+1,i+1));
                temp.real() /= 4.0;
                temp.imag() /= 4.0;
                

                discriminant = UH_site.elem(i+1,i) * UH_site.elem(i,i+1) + temp;
                // Compute polar decomposition of discriminant to take the
                // square root of a complex number.
                REAL arg_d = atan2(discriminant.imag(), discriminant.real());
                REAL mod_d = sqrt(toDouble(localNorm2(discriminant)));
                discriminant.real() = sqrt(mod_d) * cos(arg_d/2.0);
                discriminant.imag() = sqrt(mod_d) * sin(arg_d/2.0);
                
                // Reuse temp
                temp = (UH_site.elem(i,i) + UH_site.elem(i + 1,i + 1));
                temp.real() /= 2.0;
                temp.imag() /= 2.0;
                
                sol1 = temp - UH_site.elem(i + 1,i + 1) + discriminant;
                sol2 = temp - UH_site.elem(i + 1,i + 1) - discriminant;
                //----------------------------------------------------


                // Deduce the better eval to shift
                eval = UH_site.elem(i+1,i+1) + (toDouble(localNorm2(sol1)) < toDouble(localNorm2(sol2)) ? sol1 : sol2);
                
                if(sqrt(toDouble(localNorm2(eval))) < toDouble(tol) ) {
                  eval.real() = 1.0;
                  eval.imag() = 0.0;
                }
                
                // Shift the eigenvalue              
                for (int j = 0; j < Nc; j++) UH_site.elem(j,j) -= eval;
                
                // Do the QR iteration
                QDP::RComplex<REAL> T11, T12, T21, T22, U1, U2;
                REAL dV, inv_dV;
                
                // Allocate the rotation matrix elements, vector of length (1 x (Nc -1))
                PColorVector<QDP::RComplex<REAL>, Nc-1> R11; 
                PColorVector<QDP::RComplex<REAL>, Nc-1> R12; 
                PColorVector<QDP::RComplex<REAL>, Nc-1> R21; 
                PColorVector<QDP::RComplex<REAL>, Nc-1> R22; 
                
                for (int k = 0; k < Nc - 1; k++) {
                  
                  // If the sub-diagonal element is numerically
                  // small enough, floor it to 0
                  if ( sqrt(toDouble(localNorm2(UH_site.elem(k+1,k)))) < toDouble(1e-30) ) {
                    UH_site.elem(k + 1,k).real() = 0.0;
                    UH_site.elem(k + 1,k).imag() = 0.0;
                    continue;
                  }

                  U1 = UH_site.elem(k,k);
                  dV = sqrt(toDouble(localNorm2(UH_site.elem(k,k))) + toDouble(localNorm2(UH_site.elem(k + 1,k))));
                  dV = (U1.real() > 0) ? dV : -dV;
                  U1.real() += dV;
                  U2 = UH_site.elem(k + 1,k);
                  inv_dV = 1.0/dV;


                  T11 = conj(U1);
                  T11.real() *= toDouble(inv_dV);
                  T11.imag() *= toDouble(inv_dV);
                  R11.elem(k) = conj(T11);


                  T12 = conj(U2);
                  T12.real() *= toDouble(inv_dV);
                  T12.imag() *= toDouble(inv_dV);
                  R12.elem(k) = conj(T12);

                  T21 = conj(T12) * conj(U1) / U1;
                  R21.elem(k) = conj(T21);

                  T22 = T12 * U2 / U1;
                  R22.elem(k) = conj(T22);
                  
                  // Do the UH_kk and set UH_k+1k to zero
                  UH_site.elem(k,k) -= (T11 * UH_site.elem(k,k) + T12 * UH_site.elem(k + 1,k));
                  UH_site.elem(k + 1,k).real() = 0.0;
                  UH_site.elem(k + 1,k).imag() = 0.0;
                  

                  // Continue for the other columns
                  for (int j = k + 1; j < Nc; j++) {
                    QDP::RComplex<REAL> temp2 = UH_site.elem(k,j);
                    UH_site.elem(k,j) -= (T11 * temp2 + T12 * UH_site.elem(k + 1,j));
                    UH_site.elem(k + 1,j) -= (T21 * temp2 + T22 * UH_site.elem(k + 1,j));
                  }
                }

                // Rotate R and V, i.e. H->RQ. V->VQ
                // Loop over columns of upper Hessenberg
                for (int j = 0; j < Nc - 1; j++) {
                  if (sqrt(toDouble(localNorm2(R11.elem(j)))) > toDouble(tol) ) {
                    // Loop over the rows, up to the sub diagonal element k=j+1
                    for (int k = 0; k < j + 2; k++) {
                      QDP::RComplex<REAL> temp2 = UH_site.elem(k,j);
                      UH_site.elem(k,j) -= (R11.elem(j) * temp2 + R12.elem(j) * UH_site.elem(k,j + 1));
                      UH_site.elem(k,j + 1) -= (R21.elem(j) * temp2 + R22.elem(j) * UH_site.elem(k,j + 1));
                    }
                    
                    for (int k = 0; k < Nc; k++) {
                      QDP::RComplex<REAL> temp2 = Q.elem(k,j);
                      Q.elem(k,j) -= (R11.elem(j) * temp2 + R12.elem(j) * Q.elem(k,j + 1));
                      Q.elem(k,j + 1) -= (R21.elem(j) * temp2 + R22.elem(j) * Q.elem(k,j + 1));
                    }
                  }
                }                
                // Shift back
                for (int j = 0; j < Nc; j++) UH_site.elem(j,j) += eval;
              }
              iter++;
            }
          }
          
#if 0
          {
            multi1d<int> coord = Layout::siteCoords(Layout::nodeNumber(), site);
            if(coord[0] == 0 && coord[1] == 0 && coord[2] == 0 && coord[3] == 0) {
              QMP_fprintf(stdout, "%s: site=%d coord=[%d,%d,%d,%d] UH post\n",
                          __func__, site, coord[0], coord[1], coord[2], coord[3]);
              
              QDP::RComplex<REAL> sum(1.0,0.0);
              for(int j = 0; j < Nc; j++ ) {
                QDPIO::cout << "eval norm " << j << " = " << toDouble(localNorm2(UH_site.elem(j,j))) << std::endl;
                sum *= UH_site.elem(j,j);
              }
              QDPIO::cout << "eval prod = ("<< sum.real() << "," << sum.imag() << ")" << std::endl; 
              QDPIO::cout << std::endl;
            }
          }
#endif
          // Load back into the lattice sized objects
          for(int j=0; j < Nc; j++) { 
            for(int k=0; k < Nc; k++) { 
              UH.elem(site).elem().elem(j,k).real() = UH_site.elem(j,k).real();
              UH.elem(site).elem().elem(j,k).imag() = UH_site.elem(j,k).imag();
            }
          }
        }
#endif
      } // End Function      

      inline void hypVandermondeSiteLoop(int lo, int hi, int myId, 
                                         HypVandermondeArgs* arg)
      {
#ifndef QDP_IS_QDPJIT
      
        LatticeColorMatrix &UT = arg->UT; // Upper Triangular matrix
        multi1d<LatticeComplex>& f = arg->f; // The Cayley-Hamilton coeffs      

        for(int site=lo; site < hi; site++) { 
          
          // Site local Upper Triangular reduction
          PColorMatrix<QDP::RComplex<REAL>, Nc> UT_site = arg->UT.elem(site).elem();
          // Site local Vandermonde matrix
          PColorMatrix<QDP::RComplex<REAL>, Nc> V;    // Preserve for checks
          PColorMatrix<QDP::RComplex<REAL>, Nc> Vcpy; 
          PColorVector<QDP::RComplex<REAL>, Nc> G;    // vector of length (1 x N)
          
          // Populate Vandermonde matrix and G vector
          for(int i=0; i < Nc; i++) {
            QDP::RComplex<REAL> temp(1.0,0.0);
            for(int j=0; j < Nc; j++) {
              V.elem(i,j) = temp;
              Vcpy.elem(i,j) = V.elem(i,j);
              temp *= UT_site.elem(i,i);
            }

            // Compute polar decomposition of eigenvalue to take the
            // inverse square root of a complex number.
            REAL arg_Gi = atan2(UT_site.elem(i,i).imag(), UT_site.elem(i,i).real());
            REAL mod_Gi = sqrt(toDouble(localNorm2(UT_site.elem(i,i))));
            G.elem(i).real() =  (1.0/sqrt(mod_Gi)) * cos(arg_Gi/2.0);
            G.elem(i).imag() = -(1.0/sqrt(mod_Gi)) * sin(arg_Gi/2.0);            

#if 0
            // Dump G
            multi1d<int> coord = Layout::siteCoords(Layout::nodeNumber(), site);
            if(coord[0] == 0 && coord[1] == 0 && coord[2] == 0 && coord[3] == 0) {
              QMP_fprintf(stdout, "%s: site=%d coord=[%d,%d,%d,%d] G\n",
                          __func__, site, coord[0], coord[1], coord[2], coord[3]);
              
              for(int j = 0; j < Nc; j++ ) {
                QDPIO::cout << " G["<<j<<"] = (" << G.elem(j).real() << ","<< G.elem(j).imag() <<")"<<std::endl;
              }    
            }
#endif
          }      
            
#if 0
          // Dump Vandermonde          
          multi1d<int> coord = Layout::siteCoords(Layout::nodeNumber(), site);
          if(coord[0] == 0 && coord[1] == 0 && coord[2] == 0 && coord[3] == 0) {
            QMP_fprintf(stdout, "%s: site=%d coord=[%d,%d,%d,%d] VM\n",
                        __func__, site, coord[0], coord[1], coord[2], coord[3]);
            
            for(int j = 0; j < Nc; j++ ) {
              for(int k = 0; k < Nc; k++ ) {
                QDPIO::cout << "(" << V.elem(j,k).real() << "," << V.elem(j,k).imag() << ") ";
              }              
              QDPIO::cout << std::endl;
            }    
          }      
#endif
          
          // Invert Vandermonde via LU decomposition
          // DMH: There are more direct methods for doing this,
          //      that may also be more stable. Stability
          //      of this step is paramount.
          PColorMatrix<QDP::RComplex<REAL>, Nc> Vinv;
          
          double tol = 1e-15;
          int i = 0, j = 0, k = 0, i_max = 0;
          int pivots[Nc+1];
          Real max_u = 0.0, abs_u = 0.0;
          QDP::RComplex<REAL> temp(0.0,0.0);
          
          for (i = 0; i <= Nc; i++) pivots[i] = i; //Permutation matrix
          for (i = 0; i  < Nc; i++) {
            max_u = 0.0;
            i_max = i;

            for (k = i; k < Nc; k++) {
              abs_u = sqrt(toDouble(localNorm2(V.elem(k,i))));
              if (toDouble(abs_u) > toDouble(max_u)) {
                max_u = abs_u;
                i_max = k;
              }
            }
            if (toDouble(max_u) < toDouble(tol) ) {
              QDP_error_exit("Failure to invert Vandermonde matrix due to degeneracy\n"
                             "max_u %.6e < tol %.6e", max_u, tol);
            }
            
            if (i_max != i) {
              //pivoting pivots
              j = pivots[i];
              pivots[i] = pivots[i_max];
              pivots[i_max] = j;

              //pivoting rows of u
              for(int r=0; r<Nc; r++) {
                temp = V.elem(i,r);
                V.elem(i, r) = V.elem(i_max, r);
                V.elem(i_max,r) = temp;
              }

              //counting pivots starting from N
              pivots[Nc]++;
            }

            for (j = i + 1; j < Nc; j++) {
              V.elem(j,i) = V.elem(j,i)/V.elem(i,i);

              for (k = i + 1; k < Nc; k++)
                V.elem(j,k) -= V.elem(j,i) * V.elem(i,k);
            }
          }
          
          // Compute inverse
          for (int j = 0; j < Nc; j++) {
            for (int i = 0; i < Nc; i++) {
              Vinv.elem(i,j).real() = pivots[i] == j ? 1.0 : 0.0;
              Vinv.elem(i,j).imag() = 0.0;
              
              for (int k = 0; k < i; k++)
                Vinv.elem(i,j) -= V.elem(i,k) * Vinv.elem(k,j);
            }

            for (int i = Nc - 1; i >= 0; i--) {
              for (int k = i + 1; k < Nc; k++)
                Vinv.elem(i,j) -= V.elem(i,k) * Vinv.elem(k,j);
              
              Vinv.elem(i,j) = Vinv.elem(i,j)/V.elem(i,i);
            }
          }
          
          // Inverse is now defined

          // Check that the inverse is good
#if 0
          //multi1d<int> coord = Layout::siteCoords(Layout::nodeNumber(), site);
          if (abs(toDouble(real(trace(Vcpy * Vinv))) - 4.0) > 1e-6) {
            QMP_fprintf(stdout, "%s: site=%d coord=[%d,%d,%d,%d] UH post\n",
                        __func__, site, coord[0], coord[1], coord[2], coord[3]);
            
            QMP_fprintf(stdout, "Tr(Vinv * V) = (%.6e, %.6e)\n", toDouble(real(trace(Vcpy * Vinv))), toDouble(imag(trace(Vcpy * Vinv))));
            
            for(int j = 0; j < Nc; j++ ) {
              QMP_fprintf(stdout, "(%.6e, %.6e) (%.6e, %.6e) (%.6e, %.6e) (%.6e, %.6e)\n", 
                          (Vcpy * Vinv).elem(j,0).real(), (Vcpy * Vinv).elem(j,0).imag(),
                          (Vcpy * Vinv).elem(j,1).real(), (Vcpy * Vinv).elem(j,1).imag(),
                          (Vcpy * Vinv).elem(j,2).real(), (Vcpy * Vinv).elem(j,2).imag(),
                          (Vcpy * Vinv).elem(j,3).real(), (Vcpy * Vinv).elem(j,3).imag());
            }
            QDP_error_exit("Failure to invert Vandermonde matrix due to trace incorrectness\n"
                           "abs(toDouble(real(trace(Vcpy * Vinv))) - 4.0) = %e > 1e-6", abs(toDouble(real(trace(Vcpy * Vinv))) - 4.0));
          }
#endif          

#if 0
          {
            // Dump Vandermonde          
            multi1d<int> coord = Layout::siteCoords(Layout::nodeNumber(), site);
            if(coord[0] == 0 && coord[1] == 0 && coord[2] == 0 && coord[3] == 0) {
              QMP_fprintf(stdout, "%s: site=%d coord=[%d,%d,%d,%d] VM_inv\n",
                          __func__, site, coord[0], coord[1], coord[2], coord[3]);
            
              for(int j = 0; j < Nc; j++ ) {
                for(int k = 0; k < Nc; k++ ) {
                  QDPIO::cout << "(" << Vinv.elem(j,k).real() << "," << Vinv.elem(j,k).imag() << ") ";
                }              
                QDPIO::cout << " G["<<j<<"] = (" << G.elem(j).real() << ","<< G.elem(j).imag() <<")"<<std::endl;
              }    
            }
          }      
#endif
          

          
          // Compute f_i
          G = Vinv * G;
          
          // Load back into the lattice sized objects
          for(int j=0; j < Nc; j++) { 
            f[j].elem(site).elem().elem().real() = G.elem(j).real();
            f[j].elem(site).elem().elem().imag() = G.elem(j).imag();
          }
        }
#endif
      } // End Function      

    }// End Namespace
    
    void upper_hessenberg(const LatticeColorMatrix &U, 
                          LatticeColorMatrix &UH)
      
    {
      START_CODE();
      QDP::StopWatch swatch;
      swatch.reset();
      swatch.start();
      
      int num_sites = Layout::sitesOnNode();
      HypUtils::HypUpperHessArgs args={U, UH};
      
      
#if !defined(BUILD_JIT_CLOVER_TERM)

#ifdef QDP_IS_QDPJIT
#warning "Hyping disabled, as building with QDP-JIT but the JIT Clover term is disabled."
#else
#warning "Using QDP++ hyping"
      dispatch_to_threads(num_sites, args, HypUtils::hypUpperHessSiteLoop);
#endif
      
#else
#warning "Using QDP-JIT hyping"
      static JitFunction function;
      if (function.empty()) {
        //function_upper_hess_build(function, U, UH, P);
        //function_upper_hess_exec(function, U, UH, P);
      }
#endif

      swatch.stop();
      HypLinkTimings::functions_secs += swatch.getTimeInSeconds();
      END_CODE();
    }
    
  
    void qr_from_upper_hess(LatticeColorMatrix &UH,
                            const Real tol,
                            const int max_iter)
      
    {
      START_CODE();
      QDP::StopWatch swatch;
      swatch.reset();
      swatch.start();
      
      int num_sites = Layout::sitesOnNode();
      HypUtils::HypQRArgs args={UH, tol, max_iter};
      
    
#if !defined(BUILD_JIT_CLOVER_TERM)
    
#ifdef QDP_IS_QDPJIT
#warning "Hyping disabled, as building with QDP-JIT but the JIT Clover term is disabled."
#else
#warning "Using QDP++ hyping"
      dispatch_to_threads(num_sites, args, HypUtils::hypQRSiteLoop);
#endif
      
#else
#warning "Using QDP-JIT hyping"
      static JitFunction function;
      if (function.empty()) {
        //function_upper_hess_build(function, U, UH, P);
        //function_upper_hess_exec(function, U, UH, P);
      }
#endif
      
      swatch.stop();
      HypLinkTimings::functions_secs += swatch.getTimeInSeconds();
      END_CODE();
    }

    void solve_vandermonde(LatticeColorMatrix &UT,
                           multi1d<LatticeComplex>& f)
      
    {
      START_CODE();
      QDP::StopWatch swatch;
      swatch.reset();
      swatch.start();
      
      int num_sites = Layout::sitesOnNode();
      HypUtils::HypVandermondeArgs args={UT, f};
      
      
#if !defined(BUILD_JIT_CLOVER_TERM)
    
#ifdef QDP_IS_QDPJIT
#warning "Hyping disabled, as building with QDP-JIT but the JIT Clover term is disabled."
#else
#warning "Using QDP++ hyping"
      dispatch_to_threads(num_sites, args, HypUtils::hypVandermondeSiteLoop);
#endif
      
#else
#warning "Using QDP-JIT hyping"
      static JitFunction function;
      if (function.empty()) {
        //function_upper_hess_build(function, U, UH, P);
        //function_upper_hess_exec(function, U, UH, P);
      }
#endif
      
      swatch.stop();
      HypLinkTimings::functions_secs += swatch.getTimeInSeconds();
      END_CODE();
    }


    /*! \ingroup gauge */
    void hyp_lv1_links(const multi1d<LatticeColorMatrix>& u, 
                       multi1d<LatticeColorMatrix>& u_lv1,
                       multi1d<LatticeColorMatrix>& Omega,
                       multi1d<LatticeColorMatrix>& QPowHalf,
                       const multi1d<bool>& smear_in_this_dirP,
                       const Real alpha1,
                       const Real alpha2,
                       const Real alpha3,
                       const int BlkMax,
                       const Real BlkAccu)
    {
      START_CODE();
      LatticeColorMatrix u_nu_tmp;
      LatticeColorMatrix u_tmp;
      LatticeColorMatrix Q;
      multi1d<LatticeComplex> f(Nc);
      
      Real ftmp1 = 1.0 - alpha3;
      Real ftmp2 = alpha3 / 2.0;
      int ii = 0;
      for(int mu = 0; mu < Nd; mu++) {
        for(int nu = 0; nu < Nd; nu++) {
          
          if(nu == mu) continue;
          
          //QDPIO::cout << "HYP lvl1: ii=" << ii << " mu="<<mu<<" nu="<<nu<<std::endl;
          
          // Forward staple
          // u_tmp(x) = u(x,nu)*u(x+nu,mu)*u_dag(x+mu,nu)
          u_tmp = u[nu] * shift(u[mu],FORWARD,nu) * adj(shift(u[nu],FORWARD,mu));
          
          // Backward staple
          // u_tmp(x) += u_dag(x-nu,nu)*u(x-nu,mu)*u(x-nu+mu,nu)
          u_nu_tmp = shift(u[nu],FORWARD,mu);
          u_tmp += shift(adj(u[nu]) * u[mu] * u_nu_tmp, BACKWARD, nu);
          
          // Unprojected level 1 link
          Omega[ii] = ftmp1*u[mu];
          Omega[ii] = Omega[ii] + ftmp2*u_tmp;
          
          // Compute Upper Hessenberg, then QR.
          Q = adj(Omega[ii]) * Omega[ii];
          upper_hessenberg(Q, u_tmp);
          qr_from_upper_hess(u_tmp, BlkAccu, BlkMax);
          
          // Extract f coeffs from the vandermonde inversion.
          // Evals are located on the diagonal of u_tmp
          solve_vandermonde(u_tmp, f);
          
          // Construct Q^{1/2}
          QPowHalf[ii] = f[0];
          u_tmp = Q;
          for(int n=1; n<Nc; n++) {
            QPowHalf[ii] += f[n] * u_tmp;
            if(n < Nc-1) u_tmp = u_tmp * Q;
          }
          
          // Dump matrix
#if 0
          {
            int site;
            multi1d<int> coord = Layout::siteCoords(Layout::nodeNumber(), site);
            if(coord[0] == 0 && coord[1] == 0 && coord[2] == 0 && coord[3] == 0) {
              QMP_fprintf(stdout, "%s: site=%d coord=[%d,%d,%d,%d] Matrix QPowHalf[ii], lvl1\n",
                          __func__, site, coord[0], coord[1], coord[2], coord[3]);
              
              for(int j = 0; j < Nc; j++ ) {
                for(int k = 0; k < Nc; k++ ) {
                  QDPIO::cout << "(" << QPowHalf[ii].elem(site).elem().elem(j,k).real() << "," << QPowHalf[ii].elem(site).elem().elem(j,k).imag() << ") ";
                }              
                QDPIO::cout << " f["<<j<<"] = ("<<f[j].elem(site).elem().elem().real()<<","<<f[j].elem(site).elem().elem().imag() << ")" << std::endl;
              }
            }
          }
#endif
          
          // Constuct V = Omega * Q^{1/2} = Omega * (Omega^dag * Omega)^{1/2}
          u_lv1[ii] = Omega[ii] * QPowHalf[ii];

#if 0
          {
            int site;
            multi1d<int> coord = Layout::siteCoords(Layout::nodeNumber(), site);
            if(coord[0] == 0 && coord[1] == 0 && coord[2] == 0 && coord[3] == 0) {
              QMP_fprintf(stdout, "%s: site=%d coord=[%d,%d,%d,%d] Matrix u_lv1[%d], lvl1\n",
                          __func__, site, coord[0], coord[1], coord[2], coord[3], ii);
              
              for(int j = 0; j < Nc; j++ ) {
                for(int k = 0; k < Nc; k++ ) {
                  QDPIO::cout << "(" << u_lv1[ii].elem(site).elem().elem(j,k).real() << "," << u_lv1[ii].elem(site).elem().elem(j,k).imag() << ") ";
                }              
                QDPIO::cout << std::endl;
              }
            }
          }
#endif
          
          ii++;
        }
      }

      END_CODE();
    }

    /*! \ingroup gauge */
    void hyp_lv2_links(const multi1d<LatticeColorMatrix>& u, 
                       multi1d<LatticeColorMatrix>& u_lv1,
                       multi1d<LatticeColorMatrix>& u_lv2,
                       multi1d<LatticeColorMatrix>& Omega,
                       multi1d<LatticeColorMatrix>& QPowHalf,
                       const multi1d<bool>& smear_in_this_dirP,
                       const Real alpha1,
                       const Real alpha2,
                       const Real alpha3,
                       const int BlkMax,
                       const Real BlkAccu)
    {
      START_CODE();
      LatticeColorMatrix Q;
      LatticeColorMatrix u_tmp;
      LatticeColorMatrix u_lv1_tmp;
      multi1d<LatticeComplex> f(Nc);

      Real ftmp1 = 1.0 - alpha2;
      Real ftmp2 = alpha2 / 4.0;
      
      int ii = 0;
      int rho, sigma, jj, kk;
      for(int mu = 0; mu < Nd; ++mu) {
        for(int nu = 0; nu < Nd; ++nu) {
          if(nu == mu) continue;
          
          u_tmp = 0;
          for(rho = 0; rho < Nd; ++rho) {
            if(rho == mu || rho == nu) continue;
            
            // Identify 4-th orthogonal direction: sigma.
            // Sigma is unique if nu != mu and rho != mu and rho != nu
            for(jj = 0; jj < Nd; ++jj) {
              if(jj != mu && jj != nu && jj != rho) sigma = jj;
            }
                        
            jj = (Nd-1)*mu + sigma;
            if(sigma > mu ) jj--;
            kk = (Nd-1)*rho + sigma;
            if(sigma > rho ) kk--;
            
            //QDPIO::cout << "HYP lvl2: ii=" << ii << " mu="<<mu<<" nu="<<nu<<" rho="<<rho<<" sig="<<sigma<<" jj="<<jj<<" kk="<<kk<<std::endl;
            
            // Forward staple
            // u_tmp(x) += u_lv1(x,kk)*u_lv1(x+rho,jj)*u_lv1_dag(x+mu,kk)

#if 0
            {
              int site;
              multi1d<int> coord = Layout::siteCoords(Layout::nodeNumber(), site);
              if(coord[0] == 0 && coord[1] == 0 && coord[2] == 0 && coord[3] == 0) {
                QMP_fprintf(stdout, "%s: site=%d coord=[%d,%d,%d,%d] Matrix u_lv1[kk], lvl2\n",
                            __func__, site, coord[0], coord[1], coord[2], coord[3]);
                
                for(int j = 0; j < Nc; j++ ) {
                  for(int k = 0; k < Nc; k++ ) {
                    QDPIO::cout << "(" << u_lv1[kk].elem(site).elem().elem(j,k).real() << "," << u_lv1[kk].elem(site).elem().elem(j,k).imag() << ") ";
                  }              
                  QDPIO::cout << std::endl;
                }
              }
            }
#endif

            u_tmp += u_lv1[kk] * shift(u_lv1[jj],FORWARD, rho) * adj(shift(u_lv1[kk], FORWARD, mu));
#if 0
            {
              int site;
              multi1d<int> coord = Layout::siteCoords(Layout::nodeNumber(), site);
              if(coord[0] == 0 && coord[1] == 0 && coord[2] == 0 && coord[3] == 0) {
                QMP_fprintf(stdout, "%s: site=%d coord=[%d,%d,%d,%d] Matrix u_tmp_1, lvl2\n",
                            __func__, site, coord[0], coord[1], coord[2], coord[3]);
                
                for(int j = 0; j < Nc; j++ ) {
                  for(int k = 0; k < Nc; k++ ) {
                    QDPIO::cout << "(" << u_tmp.elem(site).elem().elem(j,k).real() << "," << u_tmp.elem(site).elem().elem(j,k).imag() << ") ";
                  }              
                  QDPIO::cout << std::endl;
                }
              }
            }
#endif
                 
            // Backward staple
            // u_tmp(x) += u_lv1_dag(x-rho,kk)*u_lv1(x-rho,jj)*u_lv1(x-rho+mu,kk)
            u_lv1_tmp = shift(u_lv1[kk],FORWARD,mu);
            u_tmp += shift(adj(u_lv1[kk]) * u_lv1[jj] * u_lv1_tmp , BACKWARD,rho);

#if 0
            {
              int site;
              multi1d<int> coord = Layout::siteCoords(Layout::nodeNumber(), site);
              if(coord[0] == 0 && coord[1] == 0 && coord[2] == 0 && coord[3] == 0) {
                QMP_fprintf(stdout, "%s: site=%d coord=[%d,%d,%d,%d] Matrix u_tmp_2, lvl2\n",
                            __func__, site, coord[0], coord[1], coord[2], coord[3]);
                
                for(int j = 0; j < Nc; j++ ) {
                  for(int k = 0; k < Nc; k++ ) {
                    QDPIO::cout << "(" << u_tmp.elem(site).elem().elem(j,k).real() << "," << u_tmp.elem(site).elem().elem(j,k).imag() << ") ";
                  }              
                  QDPIO::cout << std::endl;
                }
              }
            }
#endif
          }          


          // Unprojected level 2 link
          Omega[ii] = ftmp1*u[mu];
          Omega[ii] = Omega[ii] + ftmp2*u_tmp;
          
          // Compute Upper Hessenberg, then QR.
          Q = adj(Omega[ii]) * Omega[ii];
          upper_hessenberg(Q, u_tmp);
          qr_from_upper_hess(u_tmp, BlkAccu, BlkMax);
          
          // Extract f coeffs from the vandermonde inversion.
          // Evals are located on the diagonal of u_tmp
          solve_vandermonde(u_tmp, f);
          
          // Construct Q^{1/2}
          QPowHalf[ii] = f[0];
          u_tmp = Q;
          for(int n=1; n<Nc; n++) {
            QPowHalf[ii] += f[n] * u_tmp;
            if(n < Nc-1) u_tmp = u_tmp * Q;
          }
          
          // Constuct V = Omega * Q^{1/2} = Omega * (Omega^dag * Omega)^{1/2}
          u_lv2[ii] = Omega[ii] * QPowHalf[ii];
          ii++;
        }
      }
      END_CODE();
    }


    /*! \ingroup gauge */
    void hyp_lv3_links(const multi1d<LatticeColorMatrix>& u, 
                       multi1d<LatticeColorMatrix>& u_lv2,
                       multi1d<LatticeColorMatrix>& u_hyp,
                       multi1d<LatticeColorMatrix>& Omega,
                       multi1d<LatticeColorMatrix>& QPowHalf,
                       const multi1d<bool>& smear_in_this_dirP,
                       const Real alpha1,
                       const Real alpha2,
                       const Real alpha3,
                       const int BlkMax,
                       const Real BlkAccu)
    {
      START_CODE();

      LatticeColorMatrix Q;
      LatticeColorMatrix u_tmp;
      LatticeColorMatrix u_lv2_tmp;
      multi1d<LatticeComplex> f(Nc);

      int jj, kk;
      
      Real ftmp1 = 1.0 - alpha1;
      Real ftmp2 = alpha1 / 6.0;
      for(int mu = 0; mu < Nd; ++mu) {
        
          u_tmp = 0;
          for(int nu = 0; nu < Nd; ++nu) {
            if(nu == mu) continue;
            
            jj = (Nd-1)*mu + nu;
            if(nu > mu ) jj--;
            kk = (Nd-1)*nu + mu;
            if(mu > nu ) kk--;

            //QDPIO::cout << "HYP lvl3: jj=" << jj << " kk=" << kk << std::endl;
            
            // Forward staple
            // u_tmp(x) += u_lv2(x,kk)*u_lv2(x+nu,jj)*u_lv2_dag(x+mu,kk)
            u_tmp += u_lv2[kk] * shift(u_lv2[jj],FORWARD,nu) * adj(shift(u_lv2[kk],FORWARD,mu));
            
            // Backward staple
            // u_tmp(x) += u_lv2_dag(x-nu,kk)*u_lv2(x-nu,jj)*u_lv2(x-nu+mu,kk)
            u_lv2_tmp = shift(u_lv2[kk],FORWARD,mu);
            u_tmp += shift(adj(u_lv2[kk]) * u_lv2[jj] * u_lv2_tmp , BACKWARD,nu);
          }
          
          // Unprojected hyp-smeared link
          Omega[mu] = ftmp1*u[mu];
          Omega[mu] = Omega[mu] + ftmp2*u_tmp;
          
          // Compute Upper Hessenberg, then QR.
          Q = adj(Omega[mu]) * Omega[mu];
          upper_hessenberg(Q, u_tmp);
          qr_from_upper_hess(u_tmp, BlkAccu, BlkMax);
          
          // Extract f coeffs from the vandermonde inversion.
          // Evals are located on the diagonal of u_tmp
          solve_vandermonde(u_tmp, f);
          
          // Construct Q^{1/2}
          QPowHalf[mu] = f[0];
          u_tmp = Q;
          for(int n=1; n<Nc; n++) {
            QPowHalf[mu] += f[n] * u_tmp;
            if(n < Nc-1) u_tmp = u_tmp * Q;
          }
          
          // Constuct V = Omega * Q^{1/2} = Omega * (Omega^dag * Omega)^{1/2}
          u_hyp[mu] = Omega[mu] * QPowHalf[mu];
      }
      END_CODE();
    }

    
    /*! \ingroup gauge */
    void smear_links(const multi1d<LatticeColorMatrix>& u, 
                     multi1d<LatticeColorMatrix>& u_hyp,
                     multi1d<LatticeColorMatrix>& Omega1,
                     multi1d<LatticeColorMatrix>& Omega2,
                     multi1d<LatticeColorMatrix>& Omega3,
                     multi1d<LatticeColorMatrix>& QPowHalf1,
                     multi1d<LatticeColorMatrix>& QPowHalf2,
                     multi1d<LatticeColorMatrix>& QPowHalf3,
                     const multi1d<bool>& smear_in_this_dirP,
                     const Real alpha1,
                     const Real alpha2,
                     const Real alpha3,
                     const int BlkMax,
                     const Real BlkAccu)
    {
      multi1d<LatticeColorMatrix> u_lv1(Nd*(Nd-1));
      multi1d<LatticeColorMatrix> u_lv2(Nd*(Nd-1));
      
      if (Nd > 4) QDP_error_exit("Hyp-smearing only implemented for Nd<=4",Nd);
      
      START_CODE();

      hyp_lv1_links(u, u_lv1, Omega1, QPowHalf1,        smear_in_this_dirP, alpha1, alpha2, alpha3, BlkMax, BlkAccu);
      QDPIO::cout << " Level 1 complete " << std::endl;
      
      hyp_lv2_links(u, u_lv1, u_lv2, Omega2, QPowHalf2, smear_in_this_dirP, alpha1, alpha2, alpha3, BlkMax, BlkAccu);
      QDPIO::cout << " Level 2 complete " << std::endl;

      hyp_lv3_links(u, u_lv2, u_hyp, Omega3, QPowHalf3, smear_in_this_dirP, alpha1, alpha2, alpha3, BlkMax, BlkAccu);      
      QDPIO::cout << " Level 3 complete " << std::endl;
      
      // Dump Hyp smeared link / check it is in SU(Nc) manifold
#if 0
      int site;
      multi1d<int> coord = Layout::siteCoords(Layout::nodeNumber(), site);
      if(coord[0] == 0 && coord[1] == 0 && coord[2] == 0 && coord[3] == 0) {
        QMP_fprintf(stdout, "%s: site=%d coord=[%d,%d,%d,%d] HYP mat\n",
                    __func__, site, coord[0], coord[1], coord[2], coord[3]);
        
        for(int j = 0; j < Nc; j++ ) {
          for(int k = 0; k < Nc; k++ ) {
            QDPIO::cout << "(" << u_hyp[0].elem(site).elem().elem(j,k).real() << "," << u_hyp[0].elem(site).elem().elem(j,k).imag() << ") ";
          }              
          QDPIO::cout << std::endl;
        }    
      }            
#endif

    Real numSites = Real(QDP::Layout::vol());
    Double norm = Double(1)/(Nc*numSites*Nd);
    Double tr;
    for(int j = 0; j < Nd; j++ ) tr += sum(real(trace(adj(u_hyp[j]) * u_hyp[j])), all) * norm;
    
    QDPIO::cout << "Real Trace = " << tr <<std::endl;
    
    END_CODE();
    }
  } // End Namespace Hyping
} // End Namespace Chroma

    

