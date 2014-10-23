//
// BAGEL - Parallel electron correlation program.
// Filename: pdfdist_ints.h
// Copyright (C) 2014 Toru Shiozaki
//
// Author: Hai-Anh Le <anh@u.northwestern.edu>
// Maintainer: Shiozaki group
//
// This file is part of the BAGEL package.
//
// The BAGEL package is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 3, or (at your option)
// any later version.
//
// The BAGEL package is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the BAGEL package; see COPYING.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//

#ifndef __SRC_PERIODIC_PDFDIST_INTS_H
#define __SRC_PERIODIC_PDFDIST_INTS_H

#include <src/df/df.h>
#include <src/integral/os/overlapbatch.h>

namespace bagel {

class PDFDist_ints : public DFDist {
  friend class PDFIntTask_aux;
  friend class PDFIntTask_coeff;
  protected:
    /// lattice vectors in direct space
    std::vector<std::array<double, 3>> lattice_vectors_;
    /// 3-index integrals (r sL'|iL) for each L' (sum over all lattice vectors L)
    void pcompute_3index_00(const std::vector<std::shared_ptr<const Shell>>& ashell,   /*aux   */
                            const std::vector<std::shared_ptr<const Shell>>& b0shell); /*cell 0*/

    void pcompute_3index_0g(const std::vector<std::shared_ptr<const Shell>>& ashell,   /*aux   */
                            const std::vector<std::shared_ptr<const Shell>>& b0shell,  /*cell 0*/
                            const std::vector<std::shared_ptr<const Shell>>& bgshell); /*cell g*/


    /// 2-index integrals (i|j_L)^{-1} (sum over L)
    void pcompute_2index(const std::vector<std::shared_ptr<const Shell>>& ashell, const double throverlap, const bool compute_inverse);

    /// normalised 1-index integrals (i|.)
    std::shared_ptr<VectorB> data1_;
    void compute_aux_charge(const std::vector<std::shared_ptr<const Shell>>& ashell);
    /// P_{ij} = <i|.><.|j>
    std::shared_ptr<const Matrix> projector_;

    /// charged part of coeff
    std::shared_ptr<btas::Tensor3<double>> coeffC_;
    void compute_charged_coeff(const std::vector<std::shared_ptr<const Shell>>& b0shell,
                               const std::vector<std::shared_ptr<const Shell>>& bgshell);

  public:
    PDFDist_ints(std::vector<std::array<double, 3>> lattice_vectors,
                 const int nbas, const int naux, const std::vector<std::shared_ptr<const Atom>>& atoms_c0,
                 std::vector<std::shared_ptr<const Atom>>& atoms_cg, const std::vector<std::shared_ptr<const Atom>>& aux_atoms,
                 const double thr, const bool inverse, const std::shared_ptr<Matrix> data2 = nullptr);

    std::vector<std::array<double, 3>> lattice_vectors() { return lattice_vectors_; }
    int ncell() { return lattice_vectors_.size(); }

    std::shared_ptr<const VectorB> data1() const { return data1_; }
    std::shared_ptr<const btas::Tensor3<double>> coeffC() const { return coeffC_; }

    /// compute J_{rs} operator (r0 and sL'), given density matrices in AO basis
    std::shared_ptr<Matrix> pcompute_Jop(const std::shared_ptr<const Matrix> den) const;
    std::shared_ptr<Matrix> pcompute_Jop_from_coeff(std::shared_ptr<const VectorB> coeff) const;
    std::shared_ptr<VectorB> pcompute_coeff(const std::shared_ptr<const Matrix> den) const;
};

}

#endif
