//
// BAGEL - Brilliantly Advanced General Electronic Structure Library
// Filename: dkhgrad.h
// Copyright (C) 2017 Toru Shiozaki
//
// Author: Nils Strand <nilsstrand2022@u.northwestern.edu>
// Maintainer: Shiozaki group
//
// This file is part of the BAGEL package.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//


#ifndef __SRC_GRAD_DKHGRAD_H
#define __SRC_GRAD_DKHGRAD_H

#include <src/molecule/molecule.h>
#include <src/util/math/diagvec.h>

namespace bagel {

class DKHgrad {
  protected:
    std::shared_ptr<const Molecule> mol_;

    // Effective density matrix for kinetic gradient
    std::shared_ptr<const Matrix> compute_tden(std::shared_ptr<const Matrix>, std::shared_ptr<const Matrix>,
                                                std::shared_ptr<const Matrix>, std::shared_ptr<const Matrix>,
                                                std::shared_ptr<const DiagVec>, std::shared_ptr<const Matrix>,
                                                std::shared_ptr<const Matrix>, std::shared_ptr<Matrix>,
                                                std::shared_ptr<Matrix>);
    // Effective density matrices for NAI/SmallNAI gradients
    std::array<std::shared_ptr<const Matrix>, 2> compute_vden(std::shared_ptr<const Matrix>, std::shared_ptr<const Matrix>,
                                                                std::shared_ptr<const Matrix>, std::shared_ptr<const Matrix>,
                                                                std::shared_ptr<const DiagVec>, std::shared_ptr<const Matrix>,
                                                                std::shared_ptr<const Matrix>);
    // Effective density matrix for overlap gradient
    std::shared_ptr<const Matrix> compute_sden(std::shared_ptr<const Matrix>, std::shared_ptr<const Matrix>,
                                                std::shared_ptr<const Matrix>, std::shared_ptr<const Matrix>,
                                                std::shared_ptr<const Matrix>, std::shared_ptr<const DiagVec>,
                                                std::shared_ptr<const Matrix>, std::shared_ptr<const Matrix>,
                                                std::shared_ptr<Matrix>, std::shared_ptr<Matrix>);

  public:
    DKHgrad() { }
    DKHgrad(std::shared_ptr<const Molecule> current) : mol_(current) { }

    // Compute array of densities needed for DKH gradient
    std::array<std::shared_ptr<const Matrix>, 4> compute(std::shared_ptr<const Matrix>, std::shared_ptr<const Matrix>);
};

}

#endif
