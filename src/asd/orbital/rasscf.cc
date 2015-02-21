//
// BAGEL - Parallel electron correlation program.
// Filename: asd/orbital/ras/rasscf.cc
// Copyright (C) 2015 Toru Shiozaki
//
// Author: Inkoo Kim: <inkoo.kim@northwestern.edu>
// Maintainer: Shiozaki Group
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

#include <fstream>
#include <src/scf/hf/fock.h>
#include <src/asd/orbital/rasscf.h>
//#include <src/casscf/qvec.h>

#include <src/wfn/construct_method.h>

using namespace std;
using namespace bagel;


ASD_RASSCF::ASD_RASSCF(std::shared_ptr<const PTree> idat, const shared_ptr<const Geometry> geom, const shared_ptr<const Reference> re) 
  : Method(idat, geom, re), hcore_(make_shared<Hcore>(geom)) {
  cout << "ASD_RASSCF object constructed" << endl;
  if (!ref_) { //ref_ from method class
    cout << "ASD_RASSCF ctor: no reference given, create reference.." << endl;

    shared_ptr<const Reference> mref;

    // Monomer HF
    auto mhfdat = idat->get_child_optional("hf") ? idat->get_child_optional("hf") : make_shared<PTree>();
    shared_ptr<RHF> rhf = dynamic_pointer_cast<RHF>(construct_method("hf", mhfdat, geom_, ref_));
    rhf->compute();
    mref = rhf->conv_to_ref();
    // Dimerize from monomer
    auto dmrdat = idat->get_child_optional("dimerize") ? idat->get_child_optional("dimerize") : make_shared<PTree>();
    const string form = dmrdat->get<string>("form", "displace");
    if (form == "d" || form == "disp" || form == "displace") {
      if (static_cast<bool>(mref))
        dimer_ = make_shared<Dimer>(dmrdat, mref);
      else
        throw runtime_error("dimerize needs a reference calculation (for now)");
    }
    //TODO: form=="refs"
    // Dimer HF
    dimer_->scf(dmrdat);
    //Update Reference & Geometry objects of adapt dimer structure
    geom_ = dimer_->sgeom();
    ref_ = dimer_->sref();
    //update Hcore
    hcore_ = ref_->Reference::hcore();

    //ASD input
    asdinput_ = idat->get_child_optional("asd") ? idat->get_child_optional("asd") : make_shared<PTree>();

/*
        }
        else if (form == "r" || form == "refs") {
          vector<shared_ptr<const Reference>> dimer_refs;
          auto units = itree->get_vector<string>("refs", 2);
          for (auto& ikey : units) {
            auto tmp = saved.find(ikey);
            if (tmp == saved.end()) throw runtime_error(string("No reference found with name: ") + ikey);
            else dimer_refs.push_back(static_pointer_cast<const Reference>(tmp->second));
          }

          dimer = make_shared<Dimer>(itree, dimer_refs.at(0), dimer_refs.at(1));
        }

        dimer->scf(itree);


    // SCF
    auto hfdata = idata->get_child_optional("hf") ? idata->get_child_optional("hf") : make_shared<PTree>();
    shared_ptr<SCF> rhf = dynamic_pointer_cast<SCF>(construct_method("hf", hfdata, sgeom_, sref_));
    rhf->compute();
    sref_ = rhf->conv_to_ref();
    dimertime.tick_print("Dimer SCF");
*/

  }
  common_init();
}

/*
ASD_RASSCF::ASD_RASSCF(std::shared_ptr<const PTree> idat, const shared_ptr<const Geometry> geom, const shared_ptr<const Reference> ref, 
               std::tuple<std::shared_ptr<RDM<1>>,
                          std::shared_ptr<RDM<2>>> rdms )
  : geom_(geom), ref_(ref), rdms_(rdms) {

//if (!ref_) {
//  auto scf = make_shared<SCF>(idat, geom);
//  scf->compute();
//  ref_ = scf->conv_to_ref();
//}
  rdm1_ = make_shared<RDM<1>>(ref->nact());
  rdm2_ = make_shared<RDM<2>>(ref->nact());
  rdm1_ = get<0>(rdms_);
  rdm2_ = get<1>(rdms_);


  common_init();

}
*/

void ASD_RASSCF::common_init() {
  // at the moment I only care about C1 symmetry, with dynamics in mind
  if (dimer_->sgeom()->nirrep() > 1) throw runtime_error("ASD_RASSCF: C1 only at the moment.");
  print_header();

  // first set coefficient
  coeff_ = dimer_->sref()->coeff();
#if 0
  // make sure that coefficient diagonalizes overlap // TODO I think this is very dangerous
  coeff_orthog();
#endif

//ADDED TODO: get input
  // get maxiter from the input
  max_iter_ = idata_->get<int>("maxiter", 50);
  // get maxiter from the input
  max_micro_iter_ = idata_->get<int>("maxiter_micro", 100);
  // get nstate from the input
  nstate_ = idata_->get<int>("nstate", 1);
  // get istate from the input (for geometry optimization)
  istate_ = idata_->get<int>("istate", 0);
  // get thresh (for macro iteration) from the input
  thresh_ = idata_->get<double>("thresh", 1.0e-8);
  // get thresh (for micro iteration) from the input
  thresh_micro_ = idata_->get<double>("thresh_micro", thresh_);
  // option for printing natural orbitals
  natocc_ = idata_->get<bool>("natocc",false);

  // nocc from the input. If not present, full valence active space is generated.
//nact_ = idata_->get<int>("nact", 0);
//nact_ = idata_->get<int>("nact_cas", nact_);
  nact_ = dimer_->sref()->nact();

  //TODO: this is temporarily hard-coded
  nactA_ = nact_ / 2;
  nactB_ = nact_ / 2;
  rasA_ = {2,2,2};
  rasB_ = {2,2,2};
  for(const auto& s: rasA_) 
    std::cout << "RAS(A) " << s << " ";
  cout << endl;
  for(const auto& s: rasB_) 
    std::cout << "RAS(B) " << s << " ";
  cout << endl;
  cout << "TODO: nactA & B are set to half of nact_ for now" << endl;
  assert(nactA_ + nactB_ == nact_);

  // nclosed from the input. If not present, full core space is generated.
//nclosed_ = idata_->get<int>("nclosed", -1);
  nclosed_ = dimer_->sref()->nclosed();

  if (nclosed_ < -1) {
    throw runtime_error("It appears that nclosed < 0. Check nocc value.");
  } else if (nclosed_ == -1) {
    cout << "    * full core space generated for nclosed." << endl;
    //TODO below is from monomer geom_ obejct
    nclosed_ = geom_->num_count_ncore_only() / 2;
  }
  nocc_ = nclosed_ + nact_;

  nbasis_ = coeff_->mdim();
  nvirt_ = nbasis_ - nocc_;
  if (nvirt_ < 0) throw runtime_error("It appears that nvirt < 0. Check the nocc value");

  cout << "    * nstate   : " << setw(6) << nstate_ << endl;
  cout << "    * nclosed  : " << setw(6) << nclosed_ << endl;
  cout << "    * nact     : " << setw(6) << nact_ << endl;
  cout << "    * nocc     : " << setw(6) << nocc_ << endl;
  cout << "    * nvirt    : " << setw(6) << nvirt_ << endl;

  const int idel = dimer_->sgeom()->nbasis() - nbasis_;
  if (idel)
    cout << "      Due to linear dependency, " << idel << (idel==1 ? " function is" : " functions are") << " omitted" << endl;

//REMOVED
  // ASD_RASSCF methods should have FCI member. Inserting "ncore" and "norb" keyword for closed and total orbitals.
//mute_stdcout();
//fci_ = make_shared<KnowlesHandy>(idata_, geom_, ref_, nclosed_, nact_); // nstate does not need to be specified as it is in idata_...
//resume_stdcout();

//schwarz_ = geom_->schwarz();
//END REMOVED

  cout <<  "  === ASD_RASSCF iteration (" + geom_->basisfile() + ") ===" << endl << endl;

}


ASD_RASSCF::~ASD_RASSCF() {

}


void ASD_RASSCF::print_header() const {
  cout << "  ---------------------------" << endl;
  cout << "      ASD_RASSCF calculation     " << endl;
  cout << "  ---------------------------" << endl << endl;
}

void ASD_RASSCF::print_iteration(int iter, int miter, int tcount, const vector<double> energy, const double error, const double time) const {
  if (energy.size() != 1 && iter) cout << endl;

  int i = 0;
  for (auto& e : energy) {
    cout << "ZZ" << setw(5) << iter << setw(3) << i << setw(4) << miter << setw(4) << tcount
                 << setw(20) << fixed << setprecision(12) << e << "   "
                 << setw(10) << scientific << setprecision(2) << (i==0 ? error : 0.0) << fixed << setw(10) << setprecision(2)
                 << time << endl;
    ++i;
  }
}

static streambuf* backup_stream_;
static ofstream* ofs_;

void ASD_RASSCF::mute_stdcout() {
  ofstream* ofs(new ofstream("asdscf.log",(backup_stream_ ? ios::app : ios::trunc)));
  ofs_ = ofs;
  backup_stream_ = cout.rdbuf(ofs->rdbuf());
}


void ASD_RASSCF::resume_stdcout() {
  cout.rdbuf(backup_stream_);
  delete ofs_;
}


shared_ptr<Matrix> ASD_RASSCF::ao_rdm1(shared_ptr<const RDM<1>> rdm1, const bool inactive_only) const {
  // first make 1RDM in MO
  const size_t nmobasis = coeff_->mdim();
  auto mo_rdm1 = make_shared<Matrix>(nmobasis, nmobasis);
  for (int i = 0; i != nclosed_; ++i) mo_rdm1->element(i,i) = 2.0;
  if (!inactive_only) {
    for (int i = 0; i != nact_; ++i) {
      for (int j = 0; j != nact_; ++j) {
        mo_rdm1->element(nclosed_+j, nclosed_+i) = rdm1->element(j,i);
      }
    }
  }
  // transform into AO basis
  return make_shared<Matrix>(*coeff_ * *mo_rdm1 ^ *coeff_);
}


shared_ptr<Matrix> ASD_RASSCF::Qvec(const int n, const int m, shared_ptr<const Matrix> coeff, const size_t nclosed) const {
  //orig: asd_nevpt2_mat.cc
  //use half_ (see form_natorb)
  assert(n == coeff->mdim());

  cout << "ASD_RASSCF-Qvec: check 1" << endl;
  // one index transformed integrals (active)
//shared_ptr<const DFHalfDist> half = fci->jop()->mo2e_1ext();
//const MatView cdata = acoeff_->slice(0,nact_);
//shared_ptr<DFHalfDist> half = geom_->df()->compute_half_transform(cdata); //cf. fci/mofile.cc

  // J^{-1}(D|xy)
  // TODO : DFDistT needs to be modified to handle cases where number of nodes is larger than half->nocc() * cdata.mdim()
  shared_ptr<const DFFullDist> full;
  if (half_->nocc() * coeff->mdim() > mpi__->size()) {
    cout << "ASD_RASSCF-Qvec: check 1a" << endl;
    full = half_->apply_JJ()->compute_second_transform(coeff->slice(nclosed, nclosed+m));
  } else {
    cout << "ASD_RASSCF-Qvec: check 1b" << endl;
    full = half_->compute_second_transform(coeff->slice(nclosed, nclosed+m))->apply_JJ();
  }

  cout << "ASD_RASSCF-Qvec: check 2" << endl;

  // [D|tu] = (D|xy)Gamma_xy,tu
  shared_ptr<const DFFullDist> prdm = full->apply_2rdm(*rdm2_);
  cout << "ASD_RASSCF-Qvec: check 3" << endl;

  // (r,u) = (rt|D)[D|tu]
  shared_ptr<const Matrix> tmp = half_->form_2index(prdm, 1.0);
  cout << "ASD_RASSCF-Qvec: check 4" << endl;

  // MO transformation of the first index
//*this = *coeff % *tmp;
  auto out = make_shared<Matrix>(n,m);
  cout << "ASD_RASSCF-Qvec: check 5" << endl;
  *out = *coeff % *tmp;
  cout << "ASD_RASSCF-Qvec: check 6" << endl;
  return out;

}


double ASD_RASSCF::check_symmetric(std::shared_ptr<Matrix>& mat) const {
  int n = mat->ndim();
  assert(n == mat->mdim());
  auto tran = make_shared<Matrix>(n,n);
  tran = mat->transpose();
  auto subt = make_shared<Matrix>(n,n);
  *subt = *mat - *tran;
  return subt->rms();
}

shared_ptr<const Coeff> ASD_RASSCF::update_coeff(const shared_ptr<const Matrix> cold, shared_ptr<const Matrix> mat) const {
  auto cnew = make_shared<Coeff>(*cold);
  int nbas = cold->ndim();
  assert(nbas == geom_->nbasis());
  dgemm_("N", "N", nbas, nact_, nact_, 1.0, cold->data()+nbas*nclosed_, nbas, mat->data(), nact_,
                   0.0, cnew->data()+nbas*nclosed_, nbas);
  return cnew;
}



shared_ptr<Matrix> ASD_RASSCF::form_natural_orbs() {
  // here make a natural orbitals and update the coefficients
  // this effectively updates 1,2RDM and integrals
//const pair<shared_ptr<Matrix>, VectorB> natorb = fci_->natorb_convert();
  const pair<shared_ptr<Matrix>, VectorB> natorb = natorb_convert();
  // new coefficients
  coeff_ = update_coeff(coeff_, natorb.first);
  // occupation number of the natural orbitals
  occup_ = natorb.second;
  if (natocc_) print_natocc();
  return natorb.first;
}


shared_ptr<const Coeff> ASD_RASSCF::semi_canonical_orb() const {
  cout << "ASD_RASSCF: semi_canonical_orb: not implemented" << endl;
  assert(false);
#if 0
  auto rdm1mat = make_shared<Matrix>(nact_, nact_);
  copy_n(fci_->rdm1_av()->data(), rdm1mat->size(), rdm1mat->data());
  rdm1mat->sqrt();
  rdm1mat->scale(1.0/sqrt(2.0));
  auto ocoeff = coeff_->slice(0, nclosed_);
  auto acoeff = coeff_->slice(nclosed_, nocc_);
  auto vcoeff = coeff_->slice(nocc_, nbasis_);

  VectorB eig(coeff_->mdim());
  Fock<1> fock(geom_, fci_->jop()->core_fock(), nullptr, acoeff * *rdm1mat, false, /*rhf*/true);
  Matrix trans(nbasis_, nbasis_);
  trans.unit();
  if (nclosed_) {
    Matrix ofock = ocoeff % fock * ocoeff;
    ofock.diagonalize(eig);
    trans.copy_block(0, 0, nclosed_, nclosed_, ofock);
  }
  Matrix vfock = vcoeff % fock * vcoeff;
  vfock.diagonalize(eig);
  trans.copy_block(nocc_, nocc_, nvirt_, nvirt_, vfock);
  return make_shared<Coeff>(*coeff_ * trans);
#endif
}


shared_ptr<const Reference> ASD_RASSCF::conv_to_ref() const {
  cout << "ASD_RASSCF: conv_to_ref: not implemented" << endl;
  assert(false);
/*
  auto out = make_shared<Reference>(geom_, coeff_, nclosed_, nact_, nvirt_, energy(),
                                    fci_->rdm1(), fci_->rdm2(), fci_->rdm1_av(), fci_->rdm2_av(), fci_->conv_to_ciwfn());

  // TODO
  // compute one-body operators
  shared_ptr<Matrix> f;
  shared_ptr<Matrix> fact, factp, gaa;
  shared_ptr<ASDRASRotFile>  denom;
  one_body_operators(f, fact, factp, gaa, denom);
  if (natocc_) print_natocc();

  *f *= 2.0;

  for (int i = 0; i != nbasis_; ++i) {
    for (int j = 0; j != nbasis_; ++j) {
      if (i < nocc_ && j < nocc_) continue;
      f->element(j,i) = 0.0;
    }
  }
  for (int j = 0; j != nact_; ++j) {
    for (int i = 0; i != nocc_; ++i) {
      f->element(i,j+nclosed_) = fact->element(i,j);
    }
  }

  auto erdm = make_shared<Matrix>(*coeff_ * *f ^ *coeff_);

  out->set_erdm1(erdm);
  out->set_nstate(nstate_);
  return out;
*/
}


void ASD_RASSCF::print_natocc() const {
  assert(occup_.size() > 0);
  cout << " " << endl;
  cout << "  ========       state-averaged       ======== " << endl;
  cout << "  ======== natural occupation numbers ======== " << endl;
  for (int i=0; i!=occup_.size(); ++i)
    cout << setprecision(4) << "   Orbital " << i << " : " << occup_[i] << endl;
  cout << "  ============================================ " << endl;
}

// note that this does not transform internal integrals (since it is not needed in CASSCF).
pair<shared_ptr<Matrix>, VectorB> ASD_RASSCF::natorb_convert() {
  cout << "ASD_RASSCF: natorb_convert.." << endl;
//assert(rdm1_av_ != nullptr);
//TODO: not average, since singel RDM is passed from ASD
  pair<shared_ptr<Matrix>, VectorB> natorb = rdm1_->generate_natural_orbitals();
  for (int i = 0; i != nact_; ++i) {
    cout << "nat orb occ (" << i << ") = " << (natorb.second)(i) << endl;
  }

  update_rdms(natorb.first);

//TODO: disabled since fci_ is removed, should be restored
//jop_->update_1ext_ints(natorb.first); //fci/mofile.cc
  cout << "ASD_RASSCF: update half_ (mo2e_1ext).." << endl;
  const MatView cdata = coeff_->slice(nclosed_, nclosed_+nact_);
  half_ = geom_->df()->compute_half_transform(cdata);
  cout << "ASD_RASSCF: update_1ext_ints.." << endl;
  half_->rotate_occ(natorb.first); // fci/mofile
  cout << "ASD_RASSCF: update_1ext_ints done.." << endl;

//shared_ptr<const DFFullDist> full;
//full = half_->compute_second_transform(coeff_->slice(nclosed_, nclosed_+nact_))->apply_JJ();
//cout << "ASD_RASSCF: test done.." << endl;
//assert(false);

  return natorb;
}


void ASD_RASSCF::update_rdms(const shared_ptr<Matrix>& coeff) {
  cout << "ASD_RASSCF: update_rdms.." << endl;
//TODO:At the moment, not a vector of RDMs
//for (auto iter = rdm1_.begin(); iter != rdm1_.end(); ++iter)
//  (*iter)->transform(coeff);
//for (auto iter = rdm2_.begin(); iter != rdm2_.end(); ++iter)
//  (*iter)->transform(coeff);

  auto rdm1mat = make_shared<Matrix>(nact_, nact_);
  rdm1mat = rdm1_->rdm1_mat(/*nclose*/0);
  cout << "symmetric (before)?" << check_symmetric(rdm1mat) << endl;

  rdm1_->transform(coeff);
  rdm2_->transform(coeff);

  rdm1mat = rdm1_->rdm1_mat(/*nclose*/0);
  cout << "symmetric (after)?" << check_symmetric(rdm1mat) << endl;

//TODO DISABLED TEMPORARILY since Nstate=1
  // Only when #state > 1, this is needed.
  // Actually rdm1_av_ points to the same object as rdm1_ in 1 state runs. Therefore if you do twice, you get wrong.
//if (rdm1_.size() > 1) rdm1_av_->transform(coeff);
//if (rdm2_.size() > 1) rdm2_av_->transform(coeff);
//assert(rdm1_.size() > 1 || rdm1_.front() == rdm1_av_);
//assert(rdm2_.size() > 1 || rdm2_.front() == rdm2_av_);
}

