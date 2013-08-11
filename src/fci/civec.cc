//
// BAGEL - Parallel electron correlation program.
// Filename: civec.cc
// Copyright (C) 2012 Toru Shiozaki
//
// Author: Toru Shiozaki <shiozaki@northwestern.edu>
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


#include <src/fci/civec.h>
#include <src/math/algo.h>
#include <src/parallel/mpi_interface.h>

using namespace std;
using namespace bagel;


template<>
double Civector<double>::spin_expectation() const {
  shared_ptr<Civec> S2 = spin();
  double out = ddot(*S2);

  return out;
}


// S^2 = S_z^2 + S_z + S_-S_+
template<>
shared_ptr<Civector<double>> Civector<double>::spin() const {
  auto out = make_shared<Civec>(det_);

  // First the easy part, S_z^2 + S_z
  const double sz = 0.5*static_cast<double>(det_->nspin());
  *out = *this;
  *out *= sz*sz + sz + det_->neleb();

  const int norb = det_->norb();
  const int lena = det_->lena();
  const int lenb = det_->lenb();

  auto intermediate = make_shared<Civec>(det_);

  for (int i = 0; i < norb; ++i) {
    for (int j = 0; j < norb; ++j) {
      intermediate->zero();

      for ( auto& iter : det_->phia(i,j) ) {
        const double* source = this->element_ptr(0, iter.source);
        double* target = intermediate->element_ptr(0, iter.target);
        double sign = static_cast<double>(iter.sign);

        daxpy_(lenb, sign, source, 1, target, 1);
      }

      for (int ia = 0; ia < lena; ++ia) {
        double* target_base = out->element_ptr(0, ia);
        double* source_base = intermediate->element_ptr(0, ia);
        for ( auto& iter : det_->phib(j,i) ) {
          target_base[iter.target] -= static_cast<double>(iter.sign) * source_base[iter.source];
        }
      }
    }
  }

  return out;
}

// S_- = \sum_i i_beta^\dagger i_alpha
template<>
shared_ptr<Civector<double>> Civector<double>::spin_lower(shared_ptr<const Determinants> target_det) const {
  if (target_det == nullptr)
    target_det = make_shared<Determinants>(det_->norb(), det_->nelea()-1, det_->neleb()+1, det_->compress(), true);
  assert( (target_det->nelea() == det_->nelea()-1) && (target_det->neleb() == det_->neleb()+1) );
  auto out = make_shared<Civec>(target_det);

  shared_ptr<const Determinants> source_det = det_;
  const int norb = source_det->norb();

  const int source_lena = source_det->lena();
  const int source_lenb = source_det->lenb();

  double* source_data = cc_ptr_;
  // This is a safe but probably slow implementation
  for (int aiter = 0; aiter < source_lena; ++aiter) {
    auto alphastring = source_det->stringa(aiter);
    for (int biter = 0; biter < source_lenb; ++biter, ++source_data) {
      auto betastring = source_det->stringb(biter);
      for (int i = 0; i < norb; ++i) {
        bitset<nbit__> abit = alphastring;
        bitset<nbit__> bbit = betastring;
        if (abit[i]) {
          abit.reset(i);
          if (!bbit[i]) {
            bbit.set(i);

            const int atarget = target_det->lexical<0>(abit);
            const int btarget = target_det->lexical<1>(bbit);
            // Now the computation begins

            const int aphase = source_det->sign<0>(alphastring, i);
            const int bphase = -1*source_det->sign<1>(betastring, i);

            out->element(btarget, atarget) += static_cast<double>(aphase*bphase) * (*source_data);
          }
        }
      }
    }
  }

  return out;
}

// S_+ = \sum_i i_alpha^\dagger i_beta
template<>
shared_ptr<Civector<double>> Civector<double>::spin_raise(shared_ptr<const Determinants> target_det) const {
  if (target_det == nullptr)
    target_det = make_shared<Determinants>(det_->norb(), det_->nelea()+1, det_->neleb()-1, det_->compress(), true);
  assert( (target_det->nelea() == det_->nelea()+1) && (target_det->neleb() == det_->neleb()-1) );
  auto out = make_shared<Civec>(target_det);

  shared_ptr<const Determinants> source_det = det_;
  const int norb = source_det->norb();

  const int source_lena = source_det->lena();
  const int source_lenb = source_det->lenb();

  double* source_data = cc_ptr_;
  // This is a safe but probably slow implementation
  for (int aiter = 0; aiter < source_lena; ++aiter) {
    auto alphastring = source_det->stringa(aiter);
    for (int biter = 0; biter < source_lenb; ++biter, ++source_data) {
      auto betastring = source_det->stringb(biter);
      for (int i = 0; i < norb; ++i) {
        bitset<nbit__> abit = alphastring;
        bitset<nbit__> bbit = betastring;
        if (bbit[i]) {
          bbit.reset(i);
          if (!abit[i]) {
            abit.set(i);

            const int atarget = target_det->lexical<0>(abit);
            const int btarget = target_det->lexical<1>(bbit);

            const int aphase = source_det->sign<0>(alphastring, i);
            const int bphase = source_det->sign<1>(betastring, i);

            out->element(btarget, atarget) += static_cast<double>(aphase*bphase) * (*source_data);
          }
        }
      }
    }
  }

  return out;
}


template<>
void Civector<double>::spin_decontaminate(const double thresh) {
  const int nspin = det_->nspin();
  const int max_spin = det_->nelea() + det_->neleb();

  const double expectation = static_cast<double>(nspin * (nspin + 2)) * 0.25;

  shared_ptr<Civec> S2 = spin();

  int k = nspin + 2;
  while( fabs(ddot(*S2) - expectation) > thresh ) {
    if ( k > max_spin ) throw runtime_error("Spin decontamination failed.");

    const double factor = -4.0/(static_cast<double>(k*(k+2)));
    daxpy(factor, *S2);

    const double norm = this->norm();
    const double rescale = (norm*norm > 1.0e-60) ? 1.0/norm : 0.0;
    scale(rescale);

    S2 = spin();

    k += 2;
  }
}
