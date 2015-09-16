//
// BAGEL - Parallel electron correlation program.
// Filename: pseudospin.cc
// Copyright (C) 2015 Toru Shiozaki
//
// Author: Ryan D. Reynolds <rreynolds2018@u.northwestern.edu>
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

// A function to compute the coefficients of Extended Stevens Operators, for the pseudospin Hamiltonian
// Notation follows I. D. Ryabov, Appl. Magn. Reson. (2009) 35, 481-494.
// Some equations also come from I. D. Ryabov, J. Magn. Reson. (1999) 140, 141-145.

#include <src/prop/pseudospin/pseudospin.h>
#include <src/mat1e/rel/spinint.h>

using namespace std;
using namespace bagel;


Pseudospin::Pseudospin(const int _nspin) : nspin_(_nspin), nspin1_(_nspin + 1) {

  // S_x, S_y, and S_z operators in pseudospin basis
  for (int i = 0; i != 3; ++i)
    spin_xyz_[i] = make_shared<ZMatrix>(nspin1_, nspin1_);
  spin_plus_ = make_shared<ZMatrix>(nspin1_, nspin1_);
  spin_minus_ = make_shared<ZMatrix>(nspin1_, nspin1_);

  const double sval = nspin_ / 2.0;
  const double ssp1 = sval * (sval + 1.0);

  for (int i = 0; i != nspin1_; ++i) {
    const double ml1 = sval - i;
    spin_xyz_[2]->element(i,i) = ml1;
    if (i < nspin_) {
      const double ml1m = ml1 - 1.0;
      spin_plus_->element(i,i+1) = std::sqrt(ssp1 - ml1*(ml1m));
    }
    if (i > 0) {
      const double ml1p = ml1 + 1.0;
      spin_minus_->element(i,i-1) = std::sqrt(ssp1 - ml1*(ml1p));
    }
  }

  spin_xyz_[0]->add_block( 0.5, 0, 0, nspin1_, nspin1_, spin_plus_);
  spin_xyz_[0]->add_block( 0.5, 0, 0, nspin1_, nspin1_, spin_minus_);
  spin_xyz_[1]->add_block( complex<double>( 0.0, -0.5), 0, 0, nspin1_, nspin1_, spin_plus_);
  spin_xyz_[1]->add_block( complex<double>( 0.0,  0.5), 0, 0, nspin1_, nspin1_, spin_minus_);
}


vector<Spin_Operator> Pseudospin::build_2ndorder_zfs_operators() const {
  vector<Spin_Operator> out;
  const array<string,3> dim = {{ "x", "y", "z" }};
  for (int i = 0; i != 3; ++i) {
    for (int j = 0; j != 3; ++j) {
      auto mat = make_shared<ZMatrix>(*spin_xyz(i) * *spin_xyz(j));
      const string label = "D" + dim[i] + dim[j];
      Spin_Operator tmp(mat, label);
      out.push_back(tmp);
    }
  }
  return out;
}


/*
  // If true, we use the calculated spin eigenvalues rather than the expected nonrel. limit
  const bool numerical = zfci.idata()->get<bool>("aniso_numerical", false);
  if (numerical)
    assert(std::abs(zeig[0] + zeig[nspin_]) < 1.0e-6);

  // S_x, S_y, and S_z operators in pseudospin basis
  array<shared_ptr<ZMatrix>,3> pspinmat;
  for (int i = 0; i != 3; ++i)
    pspinmat[i] = make_shared<ZMatrix>(nspin1_, nspin1_);

  auto spin_plus = make_shared<ZMatrix>(nspin1_, nspin1_);
  auto spin_minus = make_shared<ZMatrix>(nspin1_, nspin1_);
  const double sval = numerical ? zeig[0] : nspin_ / 2.0;
//  const double sval = nspin_ / 2.0;
  const double ssp1 = sval * (sval + 1.0);

  for (int i = 0; i != nspin1_; ++i) {
    const double ml1 = numerical ? zeig[i] : sval - i;
//    const double ml1 = sval - i;
    pspinmat[2]->element(i,i) = ml1;
    if (i < nspin_) {
      const double ml1m = numerical ? zeig[i+1] : ml1 - 1.0;
      spin_plus->element(i,i+1) = std::sqrt(ssp1 - ml1*(ml1m));
//      spin_plus->element(i,i+1) = std::sqrt(ssp1 - ml1*(ml1-1.0));
    }
    if (i > 0) {
      const double ml1p = numerical ? zeig[i-1] : ml1 + 1.0;
      spin_minus->element(i,i-1) = std::sqrt(ssp1 - ml1*(ml1p));
//      spin_minus->element(i,i-1) = std::sqrt(ssp1 - ml1*(ml1+1.0));
    }
*/


void Pseudospin::compute_numerical_hamiltonian(const ZHarrison& zfci, shared_ptr<const RelCoeff_Block> active_coeff) {

  /**  Part 1: Compute numerical pseudospin Hamiltonian by diagonalizing S_z matrix  **/
  // First, we create spin matrices in the atomic orbital basis
  RelSpinInt aospin(zfci.geom());

  const int norb = zfci.norb();

  // S value of spin manifold to be mapped
  cout << endl << endl;
  cout << "    Modeling Pseudospin Hamiltonian for S = " << nspin_ / 2 << (nspin_ % 2 == 0 ? "" : " 1/2") << endl;

  // By default, just use the ground states
  vector<int> aniso_state;
  aniso_state.resize(nspin1_);
  ref_energy_.resize(nspin1_);
  for (int i = 0; i != nspin1_; ++i)
    aniso_state[i] = i;

  // aniso_state can be used to request mapping excited states instead
  const shared_ptr<const PTree> exstates = zfci.idata()->get_child_optional("aniso_state");
  if (exstates) {
    aniso_state = {};
    for (auto& i : *exstates)
      aniso_state.push_back(lexical_cast<int>(i->data()) - 1);
    if (aniso_state.size() != nspin1_)
      throw runtime_error("Aniso:  Wrong number of states requested for this S value (should be " + to_string(nspin1_) + ")");
    for (int i = 0; i != nspin1_; ++i)
      if (aniso_state[i] < 0 || aniso_state[i] >= zfci.nstate())
        throw runtime_error("Aniso:  Invalid state requested (should be between 1 and " + to_string(zfci.nstate()) + ")");
    cout << "    For the following states:  ";
    for (int i = 0; i != nspin1_; ++i)
      cout << aniso_state[i] << "  ";
    cout << endl;
  } else {
    cout << "    For the ground spin-manifold" << endl;
  }
  cout << endl;

  for (int i = 0; i != nspin1_; ++i)
    ref_energy_[i] = zfci.energy()[aniso_state[i]];

  // Compute spin matrices in the basis of ZFCI Hamiltonian eigenstates
  for (int i = 0; i != 3; ++i) {
    spinop_h_[i] = make_shared<ZMatrix>(nspin1_, nspin1_);
  }
  for (int i = 0; i != nspin1_; ++i) {
    for (int j = 0; j != nspin1_; ++j) {
//      shared_ptr<Kramers<2,ZRDM<1>>> temprdm = (*rdm1)(aniso_state[i], aniso_state[j]);
      shared_ptr<Kramers<2,ZRDM<1>>> temprdm = zfci.rdm1(aniso_state[i], aniso_state[j]);
      if (!temprdm->exist({1,0})) {
        cout << " * Need to generate an off-diagonal rdm of zeroes." << endl;
        temprdm->add({1,0}, temprdm->at({0,0})->clone());
      }
      shared_ptr<const ZRDM<1>> tmp = expand_kramers<1,complex<double>>(temprdm, norb);

      auto rdmmat = make_shared<ZMatrix>(norb * 2, norb * 2);
      copy_n(tmp->data(), tmp->size(), rdmmat->data());

      /*******/
      // Test i j symmetry
      if (i > j && zfci.idata()->get<bool>("aniso_test_symm", false)) {
        temprdm = zfci.rdm1(aniso_state[j], aniso_state[i]);
        if (!temprdm->exist({1,0})) {
          cout << " * Need to generate an off-diagonal rdm of zeroes." << endl;
          temprdm->add({1,0}, temprdm->at({0,0})->clone());
        }
        tmp = expand_kramers<1,complex<double>>(temprdm, norb);
        rdmmat->zero();
        copy_n(tmp->data(), tmp->size(), rdmmat->data());
        rdmmat = rdmmat->transpose_conjg();
      }
      /*******/

      ZMatrix modensity (2 * norb, 2 * norb);
      modensity.copy_block(0, 0, 2 * norb, 2 * norb, rdmmat);

      ZMatrix aodensity = (*active_coeff * modensity ^ *active_coeff);

      for (int k = 0; k != 3; ++k)
        spinop_h_[k]->element(i,j) = aodensity.dot_product(*aospin(k));
    }
  }

  for (int i = 0; i != 3; ++i)
    spinop_h_[i]->print("Spin matrix, for component " + to_string(i) + " over ZFCI states");

  // Diagonalize S_z to get pseudospin eigenstates as combinations of ZFCI Hamiltonian eigenstates
  const int usedim = zfci.idata()->get<int>("aniso_spindim", 2);
  auto transform = spinop_h_[usedim]->copy();
  VectorB zeig(nspin1_);
  transform->diagonalize(zeig);

  { // Reorder eigenvectors so positive M_s come first
    shared_ptr<ZMatrix> tempm = transform->clone();
    VectorB tempv = *zeig.clone();
    for (int i = 0; i != nspin1_; ++i) {
      tempv[i] = zeig[nspin_ - i];
      tempm->copy_block(0, i, nspin1_, 1, transform->slice(nspin_ - i, nspin_ - i + 1));
    }
    transform = tempm;
    zeig = tempv;
  }

  for (int i = 0; i != nspin1_; ++i)
    cout << "    Spin-z eigenvalue " << i+1 << " = " << zeig[i] << endl;

  // We will subtract out average energy so the pseudospin Hamiltonian is traceless
  complex<double> energy_avg = 0.0;
  for (int i = 0; i != nspin1_; ++i)
    energy_avg += ref_energy_[i];
  energy_avg /= nspin1_;

  // Now build up the numerical pseudospin Hamiltonian!
  spinham_h_ = make_shared<ZMatrix>(nspin1_, nspin1_);
  for (int i = 0; i != nspin1_; ++i) {
    spinham_h_->element(i,i) = ref_energy_[i] - energy_avg;
  }

  spinham_s_ = make_shared<ZMatrix>(*transform % *spinham_h_ * *transform);

  for (int i = 0; i != 3; ++i)
    spinop_s_[i] = make_shared<ZMatrix>(*transform % *spinop_h_[i] * *transform);

  cout << endl;

#if 1
  auto hamox = make_shared<ZMatrix>(3,3);
  hamox->element(0,0) = complex<double>( 0.00000223,  0.00000000);
  hamox->element(0,1) = complex<double>(-0.00000547,  0.00000000);
  hamox->element(0,2) = complex<double>(-0.00000306,  0.00000000);
  hamox->element(1,0) = complex<double>(-0.00000547,  0.00000000);
  hamox->element(1,1) = complex<double>(-0.00000446,  0.00000000);
  hamox->element(1,2) = complex<double>(-0.00000547,  0.00000000);
  hamox->element(2,0) = complex<double>(-0.00000306,  0.00000000);
  hamox->element(2,1) = complex<double>(-0.00000547,  0.00000000);
  hamox->element(2,2) = complex<double>( 0.00000223,  0.00000000);
//  hamox->print("Oxygen");
//  cout << "  Trace = " << hamox->trace() << endl;
//  assert(hamox->is_hermitian());

  auto hamre = make_shared<ZMatrix>(4,4);
  hamre->element(0,0) = complex<double>( 0.00002259,  0.00000000);
  hamre->element(0,1) = complex<double>(-0.00000803,  0.00001416);
  hamre->element(0,2) = complex<double>(-0.00004150,  0.00000711);
  hamre->element(0,3) = complex<double>( 0.00000000,  0.00000000);
  hamre->element(1,0) = complex<double>(-0.00000803, -0.00001416);
  hamre->element(1,1) = complex<double>(-0.00002259,  0.00000000);
  hamre->element(1,2) = complex<double>( 0.00000000,  0.00000000);
  hamre->element(1,3) = complex<double>( 0.00003908,  0.00001566);
  hamre->element(2,0) = complex<double>(-0.00004150, -0.00000711);
  hamre->element(2,1) = complex<double>( 0.00000000,  0.00000000);
  hamre->element(2,2) = complex<double>(-0.00002259,  0.00000000);
  hamre->element(2,3) = complex<double>(-0.00001425,  0.00000786);
  hamre->element(3,0) = complex<double>( 0.00000000,  0.00000000);
  hamre->element(3,1) = complex<double>( 0.00003908, -0.00001566);
  hamre->element(3,2) = complex<double>(-0.00001425, -0.00000786);
  hamre->element(3,3) = complex<double>( 0.00002259,  0.00000000);
//  hamre->print("ReCl6");
//  cout << "  Trace = " << hamre->trace() << endl;
//  assert(hamre->is_hermitian());


  // Altered to have the symmetry I expect to see
  auto hamre2 = make_shared<ZMatrix>(4,4);
  hamre2->element(0,0) = complex<double>( 0.00002259,  0.00000000);
  hamre2->element(0,1) = complex<double>(-0.00000803,  0.00001416);
  hamre2->element(0,2) = complex<double>(-0.00004150,  0.00000711);
  hamre2->element(0,3) = complex<double>( 0.00000000,  0.00000000);
  hamre2->element(1,0) = complex<double>(-0.00000803, -0.00001416);
  hamre2->element(1,1) = complex<double>(-0.00002259,  0.00000000);
  hamre2->element(1,2) = complex<double>( 0.00000000,  0.00000000);
  hamre2->element(1,3) = complex<double>(-0.00004150,  0.00000711);
  hamre2->element(2,0) = complex<double>(-0.00004150, -0.00000711);
  hamre2->element(2,1) = complex<double>( 0.00000000,  0.00000000);
  hamre2->element(2,2) = complex<double>(-0.00002259,  0.00000000);
  hamre2->element(2,3) = complex<double>( 0.00000803, -0.00001416);
  hamre2->element(3,0) = complex<double>( 0.00000000,  0.00000000);
  hamre2->element(3,1) = complex<double>(-0.00004150, -0.00000711);
  hamre2->element(3,2) = complex<double>( 0.00000803,  0.00001416);
  hamre2->element(3,3) = complex<double>( 0.00002259,  0.00000000);
//  hamre2->print("ReCl6");
//  cout << "  Trace = " << hamre2->trace() << endl;
//  assert(hamre2->is_hermitian());

/*
  auto hamremi = make_shared<ZMatrix>(4,4);
  hamremi->element(0,0) = complex<double>( 0.20300000,  0.00000000);
  hamremi->element(0,1) = complex<double>(-2.28200000,  0.00000000);
  hamremi->element(0,2) = complex<double>(-0.88900000,  0.00000000);
  hamremi->element(0,3) = complex<double>( 0.00000000,  0.00000000);
  hamremi->element(1,0) = complex<double>(-2.28200000,  0.00000000);
  hamremi->element(1,1) = complex<double>(29.54900000,  0.00000000);
  hamremi->element(1,2) = complex<double>( 0.00000000,  0.00000000);
  hamremi->element(1,3) = complex<double>(-0.88900000,  0.00000000);
  hamremi->element(2,0) = complex<double>(-0.88900000,  0.00000000);
  hamremi->element(2,1) = complex<double>( 0.00000000,  0.00000000);
  hamremi->element(2,2) = complex<double>(29.54900000,  0.00000000);
  hamremi->element(2,3) = complex<double>( 2.28200000,  0.00000000);
  hamremi->element(3,0) = complex<double>( 0.00000000,  0.00000000);
  hamremi->element(3,1) = complex<double>(-0.88900000,  0.00000000);
  hamremi->element(3,2) = complex<double>( 2.28200000,  0.00000000);
  hamremi->element(3,3) = complex<double>( 0.20300000,  0.00000000);
  assert(hamremi->is_hermitian());
*/

#if 0
  // Rows:  11, 01, -11, 10, 00, -10, 1-1, 0-1, -1-1 (or similar for 3/2,1/2,-1/2,-3/2)
  // Columns:  0xx, 1yx, 2zx, 3xy, 4yy, 5zy, 6xz, 7yz, 8zz
  auto ps1 = make_shared<ZMatrix>(9,9);

  ps1->element(0,0) = complex<double>( 0.5,  0.0);
  ps1->element(0,4) = complex<double>( 0.5,  0.0);
  ps1->element(0,8) = complex<double>( 1.0,  0.0);
  ps1->element(0,3) = complex<double>( 0.0,  0.5);
  ps1->element(0,1) = complex<double>( 0.0, -0.5);

  ps1->element(8,0) = complex<double>( 0.5,  0.0);
  ps1->element(8,4) = complex<double>( 0.5,  0.0);
  ps1->element(8,8) = complex<double>( 1.0,  0.0);
  ps1->element(8,3) = complex<double>( 0.0, -0.5);
  ps1->element(8,1) = complex<double>( 0.0,  0.5);

  ps1->element(4,0) = complex<double>( 1.0,  0.0);
  ps1->element(4,4) = complex<double>( 1.0,  0.0);

  ps1->element(6,0) = complex<double>( 0.5,  0.0);
  ps1->element(6,4) = complex<double>(-0.5,  0.0);
  ps1->element(6,3) = complex<double>( 0.0, -0.5);
  ps1->element(6,1) = complex<double>( 0.0, -0.5);

  ps1->element(2,0) = complex<double>( 0.5,  0.0);
  ps1->element(2,4) = complex<double>(-0.5,  0.0);
  ps1->element(2,3) = complex<double>( 0.0,  0.5);
  ps1->element(2,1) = complex<double>( 0.0,  0.5);

  ps1->element(3,2) = complex<double>( 1.0/std::sqrt(2),  0.0);
  ps1->element(3,5) = complex<double>( 0.0, -1.0/std::sqrt(2));

  ps1->element(5,2) = complex<double>(-1.0/std::sqrt(2),  0.0);
  ps1->element(5,5) = complex<double>( 0.0, -1.0/std::sqrt(2));

  ps1->element(1,6) = complex<double>( 1.0/std::sqrt(2),  0.0);
  ps1->element(1,7) = complex<double>( 0.0,  1.0/std::sqrt(2));

  ps1->element(7,6) = complex<double>(-1.0/std::sqrt(2),  0.0);
  ps1->element(7,7) = complex<double>( 0.0,  1.0/std::sqrt(2));
#endif

#if 1
  auto ps32 = make_shared<ZMatrix>(16,9);

  // Rows:  0 33, 1 13, 2 -13, 3 -33, 4 31, 5 11, 6 -11, 7 -31, 8 3-1, 9 1-1, 10 -1-1, 11 -3-1, 12 3-3, 13 1-3, 14 -1-3, 15 -3-3,
  // Columns:  0xx, 1yx, 2zx, 3xy, 4yy, 5zy, 6xz, 7yz, 8zz
  ps32->element(11,2) = complex<double>(-0.75*std::sqrt(3.0),  0.0);
  ps32->element(11,6) = complex<double>(-0.25*std::sqrt(3.0),  0.0);
  ps32->element(11,7) = complex<double>( 0.0, -0.25*std::sqrt(3.0));
  ps32->element(11,5) = complex<double>( 0.0, -0.75*std::sqrt(3.0));

  ps32->element(14,2) = complex<double>(-0.25*std::sqrt(3.0),  0.0);
  ps32->element(14,6) = complex<double>(-0.75*std::sqrt(3.0),  0.0);
  ps32->element(14,7) = complex<double>( 0.0,  0.75*std::sqrt(3.0));
  ps32->element(14,5) = complex<double>( 0.0,  0.25*std::sqrt(3.0));

  ps32->element(1,2) = complex<double>( 0.25*std::sqrt(3.0),  0.0);
  ps32->element(1,6) = complex<double>( 0.75*std::sqrt(3.0),  0.0);
  ps32->element(1,7) = complex<double>( 0.0,  0.75*std::sqrt(3.0));
  ps32->element(1,5) = complex<double>( 0.0,  0.25*std::sqrt(3.0));

  ps32->element(4,2) = complex<double>( 0.75*std::sqrt(3.0),  0.0);
  ps32->element(4,6) = complex<double>( 0.25*std::sqrt(3.0),  0.0);
  ps32->element(4,7) = complex<double>( 0.0, -0.25*std::sqrt(3.0));
  ps32->element(4,5) = complex<double>( 0.0, -0.75*std::sqrt(3.0));

  ps32->element(6,2) = complex<double>(-0.5,  0.0);
  ps32->element(6,6) = complex<double>( 0.5,  0.0);
  ps32->element(6,7) = complex<double>( 0.0,  0.5);
  ps32->element(6,5) = complex<double>( 0.0, -0.5);

  ps32->element(9,2) = complex<double>( 0.5,  0.0);
  ps32->element(9,6) = complex<double>(-0.5,  0.0);
  ps32->element(9,7) = complex<double>( 0.0,  0.5);
  ps32->element(9,5) = complex<double>( 0.0, -0.5);

  ps32->element(7,0) = complex<double>( std::sqrt(3.0)/2.0,  0.0);
  ps32->element(7,4) = complex<double>(-std::sqrt(3.0)/2.0,  0.0);
  ps32->element(7,3) = complex<double>( 0.0,  std::sqrt(3.0)/2.0);
  ps32->element(7,1) = complex<double>( 0.0,  std::sqrt(3.0)/2.0);

  ps32->element(2,0) = complex<double>( std::sqrt(3.0)/2.0,  0.0);
  ps32->element(2,4) = complex<double>(-std::sqrt(3.0)/2.0,  0.0);
  ps32->element(2,3) = complex<double>( 0.0,  std::sqrt(3.0)/2.0);
  ps32->element(2,1) = complex<double>( 0.0,  std::sqrt(3.0)/2.0);

  ps32->element(13,0) = complex<double>( std::sqrt(3.0)/2.0,  0.0);
  ps32->element(13,4) = complex<double>(-std::sqrt(3.0)/2.0,  0.0);
  ps32->element(13,3) = complex<double>( 0.0, -std::sqrt(3.0)/2.0);
  ps32->element(13,1) = complex<double>( 0.0, -std::sqrt(3.0)/2.0);

  ps32->element(8,0) = complex<double>( std::sqrt(3.0)/2.0,  0.0);
  ps32->element(8,4) = complex<double>(-std::sqrt(3.0)/2.0,  0.0);
  ps32->element(8,3) = complex<double>( 0.0, -std::sqrt(3.0)/2.0);
  ps32->element(8,1) = complex<double>( 0.0, -std::sqrt(3.0)/2.0);

  ps32->element(15,0) = complex<double>( 0.75,  0.0);
  ps32->element(15,4) = complex<double>( 0.75,  0.0);
  ps32->element(15,8) = complex<double>( 2.25,  0.0);
  ps32->element(15,3) = complex<double>( 0.0, -0.75);
  ps32->element(15,1) = complex<double>( 0.0,  0.75);

  ps32->element(10,0) = complex<double>( 1.75,  0.0);
  ps32->element(10,4) = complex<double>( 1.75,  0.0);
  ps32->element(10,8) = complex<double>( 0.25,  0.0);
  ps32->element(10,3) = complex<double>( 0.0, -0.25);
  ps32->element(10,1) = complex<double>( 0.0,  0.25);

  ps32->element(5,0) = complex<double>( 1.75,  0.0);
  ps32->element(5,4) = complex<double>( 1.75,  0.0);
  ps32->element(5,8) = complex<double>( 0.25,  0.0);
  ps32->element(5,3) = complex<double>( 0.0,  0.25);
  ps32->element(5,1) = complex<double>( 0.0, -0.25);

  ps32->element(0,0) = complex<double>( 0.75,  0.0);
  ps32->element(0,4) = complex<double>( 0.75,  0.0);
  ps32->element(0,8) = complex<double>( 2.25,  0.0);
  ps32->element(0,3) = complex<double>( 0.0,  0.75);
  ps32->element(0,1) = complex<double>( 0.0, -0.75);
#endif

  /************/
  if (nspin_ == 0) {
    spinham_s_ = hamre;
//    d2h = ps32;
    nspin_ = 3;
    nspin1_ = 4;
    aniso_state = {{ 0, 1, 2, 3 }};
  } else if (nspin_ == 1) {
    spinham_s_ = hamre2;
//    d2h = ps32;
    nspin_ = 3;
    nspin1_ = 4;
    aniso_state = {{ 0, 1, 2, 3 }};
  }
#endif

  cout << endl;
  spinham_s_->print("Pseudospin Hamiltonian!");

  spinop_s_[0]->print("Spin matrix - x-component");
  spinop_s_[1]->print("Spin matrix - y-component");
  spinop_s_[2]->print("Spin matrix - z-component");
  /************/
  // To average out broken symmetry and obtain a consistent set of linear equations
  if (zfci.idata()->get<bool>("aniso_approximate", false)) {
    if (nspin_ == 3) {
      const complex<double> offdiag1 = 0.25 * (spinham_s_->element(0,1) + std::conj(spinham_s_->element(1,0)) - spinham_s_->element(2,3) - std::conj(spinham_s_->element(3,2)));
      const complex<double> offdiag2 = 0.25 * (spinham_s_->element(0,2) + std::conj(spinham_s_->element(2,0)) + spinham_s_->element(1,3) + std::conj(spinham_s_->element(3,1)));
      spinham_s_->element(0,1) = offdiag1;
      spinham_s_->element(1,0) = std::conj(offdiag1);
      spinham_s_->element(2,3) = -offdiag1;
      spinham_s_->element(3,2) = -std::conj(offdiag1);
      spinham_s_->element(0,2) = offdiag2;
      spinham_s_->element(1,3) = offdiag2;
      spinham_s_->element(2,0) = std::conj(offdiag2);
      spinham_s_->element(3,1) = std::conj(offdiag2);
      spinham_s_->print("Pseudospin Hamiltonian!...  Forcibly symmetrized");
    }
  }
}


void Pseudospin::extract_hamiltonian_parameters(const bool real) {
  /**  Part 3: Extract D-tensor from the numerical pseudospin Hamiltonian **/

  // Transformation matrix to build pseudospin Hamiltonian from D-tensor
  // Rows correspond to pairs of pseudospins (SS, S-1S, S-2S...)
  // Columns correspond to elements of the D-tensor (Dxx, Dyx, Dzx, Dxy...)
  // Note that we look over the first indices before the second, so we can copy data between vectors and matrices and have it come out in the right order
  auto d2h = make_shared<ZMatrix>(nspin1_ * nspin1_, 9);
  for (int i = 0; i != 3; ++i) {
    for (int j = 0; j != 3; ++j) {
      ZMatrix temp = *spin_xyz(i) * *spin_xyz(j);
      d2h->copy_block(0, 3*j+i, nspin1_ * nspin1_, 1, temp.data());
    }
  }

  auto Dtensor = make_shared<ZMatrix>(3,3);
  auto checkham = make_shared<ZMatrix>(nspin1_, nspin1_);

  // Convert from the pseudospin Hamiltonian to the D-tensor using the left-inverse of d2h
  if (real) {
    // By default, force the D tensor to be real

    // Separate out real and imaginary parts
    auto d2h_real = make_shared<Matrix>(nspin1_ * nspin1_ * 2, 9);
    auto spinham_vec_real = make_shared<Matrix>(nspin1_ * nspin1_ * 2, 1);
    d2h_real->copy_block(            0, 0, nspin1_ * nspin1_, 9, d2h->get_real_part());
    d2h_real->copy_block(nspin1_ * nspin1_, 0, nspin1_ * nspin1_, 9, d2h->get_imag_part());
    spinham_vec_real->copy_block(            0, 0, nspin1_ * nspin1_, 1, spinham_s_->get_real_part()->element_ptr(0,0));
    spinham_vec_real->copy_block(nspin1_ * nspin1_, 0, nspin1_ * nspin1_, 1, spinham_s_->get_imag_part()->element_ptr(0,0));

    // Compute left-inverse as  (A^T A)^-1 A^T
    Matrix d2h_sqinv_real = *d2h_real % *d2h_real;
    d2h_sqinv_real.inverse();
    Matrix h2d_real = d2h_sqinv_real ^ *d2h_real;
    assert((h2d_real * *d2h_real).is_identity());

    // Extract D-tensor from it
    Matrix Dtensor_vec_real = h2d_real * *spinham_vec_real;
    Matrix Dtensor_real(3, 3);
    Dtensor_real.copy_block(0, 0, 3, 3, Dtensor_vec_real.element_ptr(0,0));

    Dtensor->copy_real_block(1.0, 0, 0, 3, 3, Dtensor_real);

    // Recompute Hamiltonian from D so we can check the fit
    ZMatrix Dtensor_vec(9, 1);
    Dtensor_vec.copy_block(0, 0, 9, 1, Dtensor->element_ptr(0,0));
    ZMatrix checkham_vec = *d2h * Dtensor_vec;
    checkham->copy_block(0, 0, nspin1_, nspin1_, checkham_vec.element_ptr(0,0));

  } else {
    // On request, allow complex ZFS parameters
    // Same algorithm, working directly with complex matrices
    auto h2d = make_shared<ZMatrix>(9, nspin1_ * nspin1_);
    ZMatrix d2h_sqinv = *d2h % *d2h;
    d2h_sqinv.inverse();
    *h2d = d2h_sqinv ^ *d2h;
    assert((*h2d * *d2h).is_identity());

    auto spinham_vec = make_shared<ZMatrix>(nspin1_ * nspin1_,1);
    spinham_vec->copy_block(0, 0, nspin1_ * nspin1_, 1, spinham_s_->element_ptr(0,0));
    ZMatrix Dtensor_vec = *h2d * *spinham_vec;
    Dtensor->copy_block(0, 0, 3, 3, Dtensor_vec.element_ptr(0,0));

    ZMatrix check_spinham_vec = *d2h * Dtensor_vec;
    checkham->copy_block(0, 0, nspin1_, nspin1_, check_spinham_vec.element_ptr(0,0));
  }

  checkham->print("Pseudospin Hamiltonian, recomputed", 30);
  cout << "  Error in recomputation of spin Hamiltonian from D = " << (*checkham - *spinham_s_).rms() << endl << endl;

  Dtensor->print("D tensor");
  VectorB Ddiag(3);
  Dtensor->diagonalize(Ddiag);
  for (int i = 0; i != 3; ++i)
    cout << "Diagonalized D-tensor value " << i << " = " << Ddiag[i] << endl;

  // Compute Davg so that it works even if D is not traceless (which shouldn't happen on accident)
  const double Davg = 1.0 / 3.0 * (Ddiag[0] + Ddiag[1] + Ddiag[2]);

  int jmax = 0;
  const array<int,3> fwd = {{ 1, 2, 0 }};
  const array<int,3> bck = {{ 2, 0, 1 }};
  if (std::abs(Ddiag[1]-Davg) > std::abs(Ddiag[jmax]-Davg)) jmax = 1;
  if (std::abs(Ddiag[2]-Davg) > std::abs(Ddiag[jmax]-Davg)) jmax = 2;
  const double Dval = Ddiag[jmax] - 0.5*(Ddiag[fwd[jmax]] + Ddiag[bck[jmax]]);
  const double Eval = 0.5*(Ddiag[fwd[jmax]] - Ddiag[bck[jmax]]);
  cout << " ** D = " << Dval << " E_h, or " << Dval * au2wavenumber__ << " cm-1" << endl;
  cout << " ** E = " << std::abs(Eval) << " E_h, or " << std::abs(Eval * au2wavenumber__) << " cm-1" << endl;

  VectorB shenergies(nspin1_);
  checkham->diagonalize(shenergies);

  if (nspin_ == 2) {
    cout << "  ** Relative energies expected from diagonalized D parameters: " << endl;
    if (Dval > 0.0) {
      cout << "     2  " << Dval + std::abs(Eval) << " E_h  =  " << (Dval + std::abs(Eval))*au2wavenumber__ << " cm-1" << endl;
      cout << "     1  " << Dval - std::abs(Eval) << " E_h  =  " << (Dval - std::abs(Eval))*au2wavenumber__ << " cm-1" << endl;
      cout << "     0  " << 0.0 << " E_h  =  " << 0.0 << " cm-1" << endl << endl;
    } else {
      cout << "     2  " << -Dval + 0.5*std::abs(Eval) << " E_h  =  " << (-Dval + 0.5*std::abs(Eval))*au2wavenumber__ << " cm-1" << endl;
      cout << "     1  " << std::abs(Eval) << " E_h  =  " << std::abs(Eval)*au2wavenumber__ << " cm-1" << endl;
      cout << "     0  " << 0.0 << " E_h  =  " << 0.0 << " cm-1" << endl << endl;
    }
  } else if (nspin_ == 3) {
    cout << "  ** Relative energies expected from diagonalized D parameters: " << endl;
    const double energy32 = 2.0*std::sqrt(Dval*Dval + 3.0*Eval*Eval);
    cout << "     3  " << energy32 << " E_h  =  " << energy32*au2wavenumber__ << " cm-1" << endl;
    cout << "     2  " << energy32 << " E_h  =  " << energy32*au2wavenumber__ << " cm-1" << endl;
    cout << "     1  " << 0.0 << " E_h  =  " << 0.0 << " cm-1" << endl;
    cout << "     0  " << 0.0 << " E_h  =  " << 0.0 << " cm-1" << endl << endl;
  }

  cout << "  ** Relative energies expected from the recomputed Pseudospin Hamiltonian: " << endl;
  for (int i = nspin_; i >= 0; --i)
    cout << "     " << i << "  " << shenergies[i] - shenergies[0] << " E_h  =  " << (shenergies[i] - shenergies[0])*au2wavenumber__ << " cm-1" << endl;
  cout << endl;

  cout << "  ** Relative energies observed by relativistic configuration interaction: " << endl;
  for (int i = nspin_; i >= 0; --i)
    cout << "     " << i << "  " << ref_energy_[i] - ref_energy_[0] << " E_h  =  " << (ref_energy_[i] - ref_energy_[0])*au2wavenumber__ << " cm-1" << endl;
  cout << endl;

}
