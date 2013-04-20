//
// BAGEL - Parallel electron correlation program.
// Filename: relfci.cc
// Copyright (C) 2013 Matthew Kelley
//
// Author: Matthew Kelley matthewkelley2017@u.northwestern.edu
// Maintainer: Shiozaki group
//
// This file is part of the BAGEL package.
//
// The BAGEL package is free software; you can redistribute it and\/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
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

#include <string>
#include <vector>
#include <bitset>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <src/rel/relfci.h>
#include <src/fci/space.h>
#include <src/rysint/eribatch.h>
#include <src/util/combination.hpp>
#include <src/util/constants.h>
#include <src/util/davidson.h>

using namespace std;
using namespace bagel;

RelFCI::RelFCI(const multimap<string, string>& idata, const shared_ptr<const Geometry> geom,
             const shared_ptr<const RelReference> re) : relref_(re) {
  gaunt_ = read_input<bool>(idata, "gaunt", true);
  breit_ = read_input<bool>(idata, "breit", gaunt_);
  geom_ = geom->relativistic(gaunt_);
  common_init(idata);
}


void RelFCI::common_init(const multimap<string, string>& idata) {
  cout << "  *** Relativistic Full CI ***" << endl << endl;

  // reading input keywords
  max_iter_ = read_input<int>(idata, "maxiter", 100);
  max_iter_ = read_input<int>(idata, "maxiter_scf", max_iter_);
  diis_start_ = read_input<int>(idata, "diis_start", 1);
  thresh_scf_ = read_input<double>(idata, "thresh", 1.0e-8);
  thresh_scf_ = read_input<double>(idata, "thresh_scf", thresh_scf_);
  ncharge_ = read_input<int>(idata, "charge", 0);
  nele_ = geom_->nele()-ncharge_;
  nneg_ = geom_->nbasis()*2;

  if (breit_ && !gaunt_) throw runtime_error("Breit cannot be turned on if Gaunt is off");
}


shared_ptr<const ZMatrix> RelFCI::time_reversal_operator() {

  const int n = geom_->nbasis();
  std::complex<double> one  (1.0, 0.0);
  std::complex<double> coeffi  (0.0, 1.0);
  
  shared_ptr<ZMatrix> kramers(new ZMatrix(4*n, 4*n));

  shared_ptr<ZMatrix> unit(new ZMatrix(n, n));
  unit->unit();

  kramers->add_block(one, 0, n, n, n, (*unit * (-1.0)).data());
  kramers->add_block(one, n, 0, n, n, unit);
  kramers->add_block(one, 2*n, 3*n, n, n, (*unit * (-1.0)).data());
  kramers->add_block(one, 3*n, 2*n, n, n, unit);

  shared_ptr<RelOverlap> overlap(new RelOverlap(geom_, false));

  return shared_ptr<const ZMatrix>(new ZMatrix(*kramers * *overlap * coeffi));
}

void RelFCI::print_eig(const unique_ptr<double[]>& eig, const int n) {
  for (int i = 0; i != n; ++i) cout << setprecision(10) << setw(15) << eig[i] << "    " <<  i << endl;
}

void RelFCI::compute() {
  cout << "Nothing here yet... " << endl;
  unique_ptr<double[]> eig(new double[relref_->coeff()->ndim()]);
  unique_ptr<double[]> eig2(new double[relref_->coeff()->ndim()]);

  coeff_ = relref_->coeff();
  shared_ptr<const ZMatrix> coeff = coeff_;
  nocc_ = relref_->nocc();
  nvirt_ = relref_->nvirt();
  shared_ptr<const ZMatrix> coeff_occ = coeff->slice(0, nocc_);
  shared_ptr<const ZMatrix> coeff_virt = coeff->slice(nocc_, nocc_+nvirt_);

  shared_ptr<ZMatrix> time_reversal(shared_ptr<ZMatrix>(new ZMatrix(*time_reversal_operator())));
//time_reversal->hermite();
//ZMatrix tmp1(*coeff_ % *time_reversal * *coeff_);
  ZMatrix tmp1(*coeff_occ % *time_reversal * *coeff_occ);
  ZMatrix tmp2(*coeff_virt % *time_reversal * *coeff_virt);
  tmp1.diagonalize(eig.get());
  tmp2.diagonalize(eig2.get());
  cout << " OCCUPIED " << endl;
  print_eig(eig, tmp1.mdim());
  cout << " VIRTUAL " << endl;
  print_eig(eig2, tmp2.mdim());

  cout << "Just Kidding!" << endl;
}

