#include "meas.h"
#include "data.h"
#include "util.h"
#include <stdio.h>
// number of types of bonds kept for 4-particle nematic correlators.
// 2 by default since these are slow measurerments
#define NEM_BONDS 2

// if complex numbers are being used, multiple some measurements by Peierls
// phases to preserve gauge invariance
#ifdef USE_CPLX
#define USE_PEIERLS
#else
// if not, these are equal to 1 anyway. multiplying by the variables costs a
// little performance, so #define them away at compile time
// TODO: exception: if using twisted boundaries, these are not always 1
#define pui0i1 1
#define pui1i0 1
#define pdi0i1 1
#define pdi1i0 1
#define puj0j1 1
#define puj1j0 1
#define pdj0j1 1
#define pdj1j0 1
#endif

void measure_eqlt(const struct params *const restrict p, const num phase,
		const num *const restrict gu,
		const num *const restrict gd,
		struct meas_eqlt *const restrict m)
{
	m->n_sample++;
	m->sign += phase;
	const int N = p->N, num_i = p->num_i, num_ij = p->num_ij;
	const int num_b = p->num_b, num_bs = p->num_bs, num_bb = p->num_bb;
	const int meas_energy_corr = p->meas_energy_corr;

	// 1 site measurements
	for (int i = 0; i < N; i++) {
		const int r = p->map_i[i];
		const num pre = phase / p->degen_i[r];
		const num guii = gu[i + i*N], gdii = gd[i + i*N];
		m->density[r] += pre*(2. - guii - gdii);
		m->double_occ[r] += pre*(1. - guii)*(1. - gdii);
	}

	// 2 site measurements
	for (int j = 0; j < N; j++)
	for (int i = 0; i < N; i++) {
		const int delta = (i == j);
		const int r = p->map_ij[i + j*N];
		const num pre = phase / p->degen_ij[r];
		const num guii = gu[i + i*N], gdii = gd[i + i*N];
		const num guij = gu[i + j*N], gdij = gd[i + j*N];
		const num guji = gu[j + i*N], gdji = gd[j + i*N];
		const num gujj = gu[j + j*N], gdjj = gd[j + j*N];
#ifdef USE_PEIERLS
		m->g00[r] += 0.5*pre*(guij*p->peierlsu[j + i*N] + gdij*p->peierlsd[j + i*N]);
#else
		m->g00[r] += 0.5*pre*(guij + gdij);
#endif
		const num x = delta*(guii + gdii) - (guji*guij + gdji*gdij);
		m->nn[r] += pre*((2. - guii - gdii)*(2. - gujj - gdjj) + x);
		m->xx[r] += 0.25*pre*(delta*(guii + gdii) - (guji*gdij + gdji*guij));
		m->zz[r] += 0.25*pre*((gdii - guii)*(gdjj - gujj) + x);
		m->pair_sw[r] += pre*guij*gdij;
		if (meas_energy_corr) {
			const double nuinuj = (1. - guii)*(1. - gujj) + (delta - guji)*guij;
			const double ndindj = (1. - gdii)*(1. - gdjj) + (delta - gdji)*gdij;
			m->vv[r] += pre*nuinuj*ndindj;
			m->vn[r] += pre*(nuinuj*(1. - gdii) + (1. - guii)*ndindj);
		}
	}

	if (!meas_energy_corr)
		return;

	// 1 bond 1 site measurements
	for (int j = 0; j < N; j++)
	for (int b = 0; b < num_b; b++) {
		const int i0 = p->bonds[b];
		const int i1 = p->bonds[b + num_b];
#ifdef USE_PEIERLS
		const num pui0i1 = p->peierlsu[i0 + N*i1];
		const num pui1i0 = p->peierlsu[i1 + N*i0];
		const num pdi0i1 = p->peierlsd[i0 + N*i1];
		const num pdi1i0 = p->peierlsd[i1 + N*i0];
#endif
		const int bs = p->map_bs[b + num_b*j];
		const num pre = phase / p->degen_bs[bs];
		const int delta_i0i1 = 0;
		const int delta_i0j = (i0 == j);
		const int delta_i1j = (i1 == j);
		const num gui0j = gu[i0 + N*j];
		const num guji0 = gu[j + N*i0];
		const num gdi0j = gd[i0 + N*j];
		const num gdji0 = gd[j + N*i0];
		const num gui1j = gu[i1 + N*j];
		const num guji1 = gu[j + N*i1];
		const num gdi1j = gd[i1 + N*j];
		const num gdji1 = gd[j + N*i1];
		const num gui0i1 = gu[i0 + N*i1];
		const num gui1i0 = gu[i1 + N*i0];
		const num gdi0i1 = gd[i0 + N*i1];
		const num gdi1i0 = gd[i1 + N*i0];
		const num gujj = gu[j + N*j];
		const num gdjj = gd[j + N*j];

		const num ku = pui1i0*(delta_i0i1 - gui0i1) + pui0i1*(delta_i0i1 - gui1i0);
		const num kd = pdi1i0*(delta_i0i1 - gdi0i1) + pdi0i1*(delta_i0i1 - gdi1i0);
		const num xu = pui0i1*(delta_i0j - guji0)*gui1j + pui1i0*(delta_i1j - guji1)*gui0j;
		const num xd = pdi0i1*(delta_i0j - gdji0)*gdi1j + pdi1i0*(delta_i1j - gdji1)*gdi0j;
		m->kv[bs] += pre*((ku*(1. - gujj) + xu)*(1. - gdjj)
		                + (kd*(1. - gdjj) + xd)*(1. - gujj));
		m->kn[bs] += pre*((ku + kd)*(2. - gujj - gdjj) + xu + xd);
	}

	// 2 bond measurements
	for (int c = 0; c < num_b; c++) {
		const int j0 = p->bonds[c];
		const int j1 = p->bonds[c + num_b];
#ifdef USE_PEIERLS
		const num puj0j1 = p->peierlsu[j0 + N*j1];
		const num puj1j0 = p->peierlsu[j1 + N*j0];
		const num pdj0j1 = p->peierlsd[j0 + N*j1];
		const num pdj1j0 = p->peierlsd[j1 + N*j0];
#endif
	for (int b = 0; b < num_b; b++) {
		const int i0 = p->bonds[b];
		const int i1 = p->bonds[b + num_b];
#ifdef USE_PEIERLS
		const num pui0i1 = p->peierlsu[i0 + N*i1];
		const num pui1i0 = p->peierlsu[i1 + N*i0];
		const num pdi0i1 = p->peierlsd[i0 + N*i1];
		const num pdi1i0 = p->peierlsd[i1 + N*i0];
#endif
		const int bb = p->map_bb[b + c*num_b];
		const num pre = phase / p->degen_bb[bb];
		const int delta_i0j0 = (i0 == j0);
		const int delta_i1j0 = (i1 == j0);
		const int delta_i0j1 = (i0 == j1);
		const int delta_i1j1 = (i1 == j1);
		const num gui1i0 = gu[i1 + i0*N];
		const num gui0i1 = gu[i0 + i1*N];
		const num gui0j0 = gu[i0 + j0*N];
		const num gui1j0 = gu[i1 + j0*N];
		const num gui0j1 = gu[i0 + j1*N];
		const num gui1j1 = gu[i1 + j1*N];
		const num guj0i0 = gu[j0 + i0*N];
		const num guj1i0 = gu[j1 + i0*N];
		const num guj0i1 = gu[j0 + i1*N];
		const num guj1i1 = gu[j1 + i1*N];
		const num guj1j0 = gu[j1 + j0*N];
		const num guj0j1 = gu[j0 + j1*N];
		const num gdi1i0 = gd[i1 + i0*N];
		const num gdi0i1 = gd[i0 + i1*N];
		const num gdi0j0 = gd[i0 + j0*N];
		const num gdi1j0 = gd[i1 + j0*N];
		const num gdi0j1 = gd[i0 + j1*N];
		const num gdi1j1 = gd[i1 + j1*N];
		const num gdj0i0 = gd[j0 + i0*N];
		const num gdj1i0 = gd[j1 + i0*N];
		const num gdj0i1 = gd[j0 + i1*N];
		const num gdj1i1 = gd[j1 + i1*N];
		const num gdj1j0 = gd[j1 + j0*N];
		const num gdj0j1 = gd[j0 + j1*N];
		const num x = pui0i1*puj0j1*(delta_i0j1 - guj1i0)*gui1j0 + pui1i0*puj1j0*(delta_i1j0 - guj0i1)*gui0j1
		            + pdi0i1*pdj0j1*(delta_i0j1 - gdj1i0)*gdi1j0 + pdi1i0*pdj1j0*(delta_i1j0 - gdj0i1)*gdi0j1;
		const num y = pui0i1*puj1j0*(delta_i0j0 - guj0i0)*gui1j1 + pui1i0*puj0j1*(delta_i1j1 - guj1i1)*gui0j0
		            + pdi0i1*pdj1j0*(delta_i0j0 - gdj0i0)*gdi1j1 + pdi1i0*pdj0j1*(delta_i1j1 - gdj1i1)*gdi0j0;
		m->kk[bb] += pre*((pui1i0*gui0i1 + pui0i1*gui1i0 + pdi1i0*gdi0i1 + pdi0i1*gdi1i0)
		                 *(puj1j0*guj0j1 + puj0j1*guj1j0 + pdj1j0*gdj0j1 + pdj0j1*gdj1j0) + x + y);
	}
	}
}

void measure_uneqlt(const struct params *const restrict p, const num phase,
		const num *const Gu0t,
		const num *const Gutt,
		const num *const Gut0,
		const num *const Gd0t,
		const num *const Gdtt,
		const num *const Gdt0,
		struct meas_uneqlt *const restrict m)
{
	m->n_sample++;
	m->sign += phase;
	const int N = p->N, L = p->L, num_i = p->num_i, num_ij = p->num_ij;
	const int num_b = p->num_b, num_bs = p->num_bs, num_bb = p->num_bb;
	const int meas_bond_corr = p->meas_bond_corr;
	const int meas_energy_corr = p->meas_energy_corr;
	const int meas_nematic_corr = p->meas_nematic_corr;

	const num *const restrict Gu00 = Gutt;
	const num *const restrict Gd00 = Gdtt;

	// 2 site measurements
	#pragma omp parallel for
	for (int t = 0; t < L; t++) {
		const int delta_t = (t == 0);
		const num *const restrict Gu0t_t = Gu0t + N*N*t;
		const num *const restrict Gutt_t = Gutt + N*N*t;
		const num *const restrict Gut0_t = Gut0 + N*N*t;
		const num *const restrict Gd0t_t = Gd0t + N*N*t;
		const num *const restrict Gdtt_t = Gdtt + N*N*t;
		const num *const restrict Gdt0_t = Gdt0 + N*N*t;
	for (int j = 0; j < N; j++)
	for (int i = 0; i < N; i++) {
		const int r = p->map_ij[i + j*N];
		const int delta_tij = delta_t * (i == j);
		const num pre = phase / p->degen_ij[r];
		const num guii = Gutt_t[i + N*i];
		const num guij = Gut0_t[i + N*j];
		const num guji = Gu0t_t[j + N*i];
		const num gujj = Gu00[j + N*j];
		const num gdii = Gdtt_t[i + N*i];
		const num gdij = Gdt0_t[i + N*j];
		const num gdji = Gd0t_t[j + N*i];
		const num gdjj = Gd00[j + N*j];
#ifdef USE_PEIERLS
		m->gt0[r + num_ij*t] += 0.5*pre*(guij*p->peierlsu[j + i*N] + gdij*p->peierlsd[j + i*N]);
#else
		m->gt0[r + num_ij*t] += 0.5*pre*(guij + gdij);
#endif
		const num x = delta_tij*(guii + gdii) - (guji*guij + gdji*gdij);
		m->nn[r + num_ij*t] += pre*((2. - guii - gdii)*(2. - gujj - gdjj) + x);
		m->xx[r + num_ij*t] += 0.25*pre*(delta_tij*(guii + gdii) - (guji*gdij + gdji*guij));
		m->zz[r + num_ij*t] += 0.25*pre*((gdii - guii)*(gdjj - gujj) + x);
		m->pair_sw[r + num_ij*t] += pre*guij*gdij;
		if (meas_energy_corr) {
			const num nuinuj = (1. - guii)*(1. - gujj) + (delta_tij - guji)*guij;
			const num ndindj = (1. - gdii)*(1. - gdjj) + (delta_tij - gdji)*gdij;
			m->vv[r + num_ij*t] += pre*nuinuj*ndindj;
			m->vn[r + num_ij*t] += pre*(nuinuj*(1. - gdii) + (1. - guii)*ndindj);
		}
	}
	}

	// 1 bond 1 site measurements
	if (meas_energy_corr)
	#pragma omp parallel for
	for (int t = 0; t < L; t++) {
		const int delta_t = (t == 0);
		const num *const restrict Gu0t_t = Gu0t + N*N*t;
		const num *const restrict Gutt_t = Gutt + N*N*t;
		const num *const restrict Gut0_t = Gut0 + N*N*t;
		const num *const restrict Gd0t_t = Gd0t + N*N*t;
		const num *const restrict Gdtt_t = Gdtt + N*N*t;
		const num *const restrict Gdt0_t = Gdt0 + N*N*t;
	for (int j = 0; j < N; j++)
	for (int b = 0; b < num_b; b++) {
		const int i0 = p->bonds[b];
		const int i1 = p->bonds[b + num_b];
#ifdef USE_PEIERLS
		const num pui0i1 = p->peierlsu[i0 + N*i1];
		const num pui1i0 = p->peierlsu[i1 + N*i0];
		const num pdi0i1 = p->peierlsd[i0 + N*i1];
		const num pdi1i0 = p->peierlsd[i1 + N*i0];
#endif
		const int bs = p->map_bs[b + num_b*j];
		const num pre = phase / p->degen_bs[bs];
		const int delta_i0i1 = 0;
		const int delta_i0j = delta_t*(i0 == j);
		const int delta_i1j = delta_t*(i1 == j);
		const num gui0j = Gut0_t[i0 + N*j];
		const num guji0 = Gu0t_t[j + N*i0];
		const num gdi0j = Gdt0_t[i0 + N*j];
		const num gdji0 = Gd0t_t[j + N*i0];
		const num gui1j = Gut0_t[i1 + N*j];
		const num guji1 = Gu0t_t[j + N*i1];
		const num gdi1j = Gdt0_t[i1 + N*j];
		const num gdji1 = Gd0t_t[j + N*i1];
		const num gui0i1 = Gutt_t[i0 + N*i1];
		const num gui1i0 = Gutt_t[i1 + N*i0];
		const num gdi0i1 = Gdtt_t[i0 + N*i1];
		const num gdi1i0 = Gdtt_t[i1 + N*i0];
		const num gujj = Gu00[j + N*j];
		const num gdjj = Gd00[j + N*j];

		const num ku = pui1i0*(delta_i0i1 - gui0i1) + pui0i1*(delta_i0i1 - gui1i0);
		const num kd = pdi1i0*(delta_i0i1 - gdi0i1) + pdi0i1*(delta_i0i1 - gdi1i0);
		const num xu = pui0i1*(delta_i0j - guji0)*gui1j + pui1i0*(delta_i1j - guji1)*gui0j;
		const num xd = pdi0i1*(delta_i0j - gdji0)*gdi1j + pdi1i0*(delta_i1j - gdji1)*gdi0j;
		m->kv[bs + num_bs*t] += pre*((ku*(1. - gujj) + xu)*(1. - gdjj)
		                           + (kd*(1. - gdjj) + xd)*(1. - gujj));
		m->kn[bs + num_bs*t] += pre*((ku + kd)*(2. - gujj - gdjj) + xu + xd);
	}
	}

	// 2 bond measurements
	// minor optimization: handle t = 0 separately, since there are no delta
	// functions for t > 0. not really needed in 2-site measurements above
	// as those are fast anyway
	if (meas_bond_corr)
	for (int c = 0; c < num_b; c++) {
		const int j0 = p->bonds[c];
		const int j1 = p->bonds[c + num_b];
#ifdef USE_PEIERLS
		const num puj0j1 = p->peierlsu[j0 + N*j1];
		const num puj1j0 = p->peierlsu[j1 + N*j0];
		const num pdj0j1 = p->peierlsd[j0 + N*j1];
		const num pdj1j0 = p->peierlsd[j1 + N*j0];
#endif
	for (int b = 0; b < num_b; b++) {
		const int i0 = p->bonds[b];
		const int i1 = p->bonds[b + num_b];
#ifdef USE_PEIERLS
		const num pui0i1 = p->peierlsu[i0 + N*i1];
		const num pui1i0 = p->peierlsu[i1 + N*i0];
		const num pdi0i1 = p->peierlsd[i0 + N*i1];
		const num pdi1i0 = p->peierlsd[i1 + N*i0];
#endif
		const int bb = p->map_bb[b + c*num_b];
		const num pre = phase / p->degen_bb[bb];
		const int delta_i0j0 = (i0 == j0);
		const int delta_i1j0 = (i1 == j0);
		const int delta_i0j1 = (i0 == j1);
		const int delta_i1j1 = (i1 == j1);
		const num gui1i0 = Gu00[i1 + i0*N];
		const num gui0i1 = Gu00[i0 + i1*N];
		const num gui0j0 = Gu00[i0 + j0*N];
		const num gui1j0 = Gu00[i1 + j0*N];
		const num gui0j1 = Gu00[i0 + j1*N];
		const num gui1j1 = Gu00[i1 + j1*N];
		const num guj0i0 = Gu00[j0 + i0*N];
		const num guj1i0 = Gu00[j1 + i0*N];
		const num guj0i1 = Gu00[j0 + i1*N];
		const num guj1i1 = Gu00[j1 + i1*N];
		const num guj1j0 = Gu00[j1 + j0*N];
		const num guj0j1 = Gu00[j0 + j1*N];
		const num gdi1i0 = Gd00[i1 + i0*N];
		const num gdi0i1 = Gd00[i0 + i1*N];
		const num gdi0j0 = Gd00[i0 + j0*N];
		const num gdi1j0 = Gd00[i1 + j0*N];
		const num gdi0j1 = Gd00[i0 + j1*N];
		const num gdi1j1 = Gd00[i1 + j1*N];
		const num gdj0i0 = Gd00[j0 + i0*N];
		const num gdj1i0 = Gd00[j1 + i0*N];
		const num gdj0i1 = Gd00[j0 + i1*N];
		const num gdj1i1 = Gd00[j1 + i1*N];
		const num gdj1j0 = Gd00[j1 + j0*N];
		const num gdj0j1 = Gd00[j0 + j1*N];
		m->pair_bb[bb] += 0.5*pre*(gui0j0*gdi1j1 + gui1j0*gdi0j1 + gui0j1*gdi1j0 + gui1j1*gdi0j0);
		const num x = pui0i1*puj0j1*(delta_i0j1 - guj1i0)*gui1j0 + pui1i0*puj1j0*(delta_i1j0 - guj0i1)*gui0j1
		            + pdi0i1*pdj0j1*(delta_i0j1 - gdj1i0)*gdi1j0 + pdi1i0*pdj1j0*(delta_i1j0 - gdj0i1)*gdi0j1;
		const num y = pui0i1*puj1j0*(delta_i0j0 - guj0i0)*gui1j1 + pui1i0*puj0j1*(delta_i1j1 - guj1i1)*gui0j0
		            + pdi0i1*pdj1j0*(delta_i0j0 - gdj0i0)*gdi1j1 + pdi1i0*pdj0j1*(delta_i1j1 - gdj1i1)*gdi0j0;
		m->jj[bb]   += pre*((pui1i0*gui0i1 - pui0i1*gui1i0 + pdi1i0*gdi0i1 - pdi0i1*gdi1i0)
		                   *(puj1j0*guj0j1 - puj0j1*guj1j0 + pdj1j0*gdj0j1 - pdj0j1*gdj1j0) + x - y);
		m->jsjs[bb] += pre*((pui1i0*gui0i1 - pui0i1*gui1i0 - pdi1i0*gdi0i1 + pdi0i1*gdi1i0)
		                   *(puj1j0*guj0j1 - puj0j1*guj1j0 - pdj1j0*gdj0j1 + pdj0j1*gdj1j0) + x - y);
		m->kk[bb]   += pre*((pui1i0*gui0i1 + pui0i1*gui1i0 + pdi1i0*gdi0i1 + pdi0i1*gdi1i0)
		                   *(puj1j0*guj0j1 + puj0j1*guj1j0 + pdj1j0*gdj0j1 + pdj0j1*gdj1j0) + x + y);
		m->ksks[bb] += pre*((pui1i0*gui0i1 + pui0i1*gui1i0 - pdi1i0*gdi0i1 - pdi0i1*gdi1i0)
		                   *(puj1j0*guj0j1 + puj0j1*guj1j0 - pdj1j0*gdj0j1 - pdj0j1*gdj1j0) + x + y);
	}
	}

	if (meas_nematic_corr)
	for (int c = 0; c < NEM_BONDS*N; c++) {
		const int j0 = p->bonds[c];
		const int j1 = p->bonds[c + num_b];
#ifdef USE_PEIERLS
		const num puj0j1 = p->peierlsu[j0 + N*j1];
		const num puj1j0 = p->peierlsu[j1 + N*j0];
		const num pdj0j1 = p->peierlsd[j0 + N*j1];
		const num pdj1j0 = p->peierlsd[j1 + N*j0];
#endif
	for (int b = 0; b < NEM_BONDS*N; b++) {
		const int i0 = p->bonds[b];
		const int i1 = p->bonds[b + num_b];
#ifdef USE_PEIERLS
		const num pui0i1 = p->peierlsu[i0 + N*i1];
		const num pui1i0 = p->peierlsu[i1 + N*i0];
		const num pdi0i1 = p->peierlsd[i0 + N*i1];
		const num pdi1i0 = p->peierlsd[i1 + N*i0];
#endif
		const int bb = p->map_bb[b + c*num_b];
		const num pre = phase / p->degen_bb[bb];
		const int delta_i0j0 = (i0 == j0);
		const int delta_i1j0 = (i1 == j0);
		const int delta_i0j1 = (i0 == j1);
		const int delta_i1j1 = (i1 == j1);
		const num gui0i0 = Gu00[i0 + i0*N];
		const num gui1i0 = Gu00[i1 + i0*N];
		const num gui0i1 = Gu00[i0 + i1*N];
		const num gui1i1 = Gu00[i1 + i1*N];
		const num gui0j0 = Gu00[i0 + j0*N];
		const num gui1j0 = Gu00[i1 + j0*N];
		const num gui0j1 = Gu00[i0 + j1*N];
		const num gui1j1 = Gu00[i1 + j1*N];
		const num guj0i0 = Gu00[j0 + i0*N];
		const num guj1i0 = Gu00[j1 + i0*N];
		const num guj0i1 = Gu00[j0 + i1*N];
		const num guj1i1 = Gu00[j1 + i1*N];
		const num guj0j0 = Gu00[j0 + j0*N];
		const num guj1j0 = Gu00[j1 + j0*N];
		const num guj0j1 = Gu00[j0 + j1*N];
		const num guj1j1 = Gu00[j1 + j1*N];
		const num gdi0i0 = Gd00[i0 + i0*N];
		const num gdi1i0 = Gd00[i1 + i0*N];
		const num gdi0i1 = Gd00[i0 + i1*N];
		const num gdi1i1 = Gd00[i1 + i1*N];
		const num gdi0j0 = Gd00[i0 + j0*N];
		const num gdi1j0 = Gd00[i1 + j0*N];
		const num gdi0j1 = Gd00[i0 + j1*N];
		const num gdi1j1 = Gd00[i1 + j1*N];
		const num gdj0i0 = Gd00[j0 + i0*N];
		const num gdj1i0 = Gd00[j1 + i0*N];
		const num gdj0i1 = Gd00[j0 + i1*N];
		const num gdj1i1 = Gd00[j1 + i1*N];
		const num gdj0j0 = Gd00[j0 + j0*N];
		const num gdj1j0 = Gd00[j1 + j0*N];
		const num gdj0j1 = Gd00[j0 + j1*N];
		const num gdj1j1 = Gd00[j1 + j1*N];
		const int delta_i0i1 = 0;
		const int delta_j0j1 = 0;
		const num uuuu = +(1.-gui0i0)*(1.-gui1i1)*(1.-guj0j0)*(1.-guj1j1)+(1.-gui0i0)*(1.-gui1i1)*(delta_j0j1-guj1j0)*guj0j1+(1.-gui0i0)*(delta_i1j0-guj0i1)*gui1j0*(1.-guj1j1)-(1.-gui0i0)*(delta_i1j0-guj0i1)*gui1j1*(delta_j0j1-guj1j0)+(1.-gui0i0)*(delta_i1j1-guj1i1)*gui1j0*guj0j1+(1.-gui0i0)*(delta_i1j1-guj1i1)*gui1j1*(1.-guj0j0)+(delta_i0i1-gui1i0)*gui0i1*(1.-guj0j0)*(1.-guj1j1)+(delta_i0i1-gui1i0)*gui0i1*(delta_j0j1-guj1j0)*guj0j1-(delta_i0i1-gui1i0)*gui0j0*(delta_i1j0-guj0i1)*(1.-guj1j1)-(delta_i0i1-gui1i0)*gui0j0*(delta_i1j1-guj1i1)*guj0j1+(delta_i0i1-gui1i0)*gui0j1*(delta_i1j0-guj0i1)*(delta_j0j1-guj1j0)-(delta_i0i1-gui1i0)*gui0j1*(delta_i1j1-guj1i1)*(1.-guj0j0)+(delta_i0j0-guj0i0)*gui0i1*gui1j0*(1.-guj1j1)-(delta_i0j0-guj0i0)*gui0i1*gui1j1*(delta_j0j1-guj1j0)+(delta_i0j0-guj0i0)*gui0j0*(1.-gui1i1)*(1.-guj1j1)+(delta_i0j0-guj0i0)*gui0j0*(delta_i1j1-guj1i1)*gui1j1-(delta_i0j0-guj0i0)*gui0j1*(1.-gui1i1)*(delta_j0j1-guj1j0)-(delta_i0j0-guj0i0)*gui0j1*(delta_i1j1-guj1i1)*gui1j0+(delta_i0j1-guj1i0)*gui0i1*gui1j0*guj0j1+(delta_i0j1-guj1i0)*gui0i1*gui1j1*(1.-guj0j0)+(delta_i0j1-guj1i0)*gui0j0*(1.-gui1i1)*guj0j1-(delta_i0j1-guj1i0)*gui0j0*(delta_i1j0-guj0i1)*gui1j1+(delta_i0j1-guj1i0)*gui0j1*(1.-gui1i1)*(1.-guj0j0)+(delta_i0j1-guj1i0)*gui0j1*(delta_i1j0-guj0i1)*gui1j0;
		const num uuud = +(1.-gui0i0)*(1.-gui1i1)*(1.-guj0j0)*(1.-gdj1j1)+(1.-gui0i0)*(delta_i1j0-guj0i1)*gui1j0*(1.-gdj1j1)+(delta_i0i1-gui1i0)*gui0i1*(1.-guj0j0)*(1.-gdj1j1)-(delta_i0i1-gui1i0)*gui0j0*(delta_i1j0-guj0i1)*(1.-gdj1j1)+(delta_i0j0-guj0i0)*gui0i1*gui1j0*(1.-gdj1j1)+(delta_i0j0-guj0i0)*gui0j0*(1.-gui1i1)*(1.-gdj1j1);
		const num uudu = +(1.-gui0i0)*(1.-gui1i1)*(1.-gdj0j0)*(1.-guj1j1)+(1.-gui0i0)*(delta_i1j1-guj1i1)*gui1j1*(1.-gdj0j0)+(delta_i0i1-gui1i0)*gui0i1*(1.-gdj0j0)*(1.-guj1j1)-(delta_i0i1-gui1i0)*gui0j1*(delta_i1j1-guj1i1)*(1.-gdj0j0)+(delta_i0j1-guj1i0)*gui0i1*gui1j1*(1.-gdj0j0)+(delta_i0j1-guj1i0)*gui0j1*(1.-gui1i1)*(1.-gdj0j0);
		const num uudd = +(1.-gui0i0)*(1.-gui1i1)*(1.-gdj0j0)*(1.-gdj1j1)+(1.-gui0i0)*(1.-gui1i1)*(delta_j0j1-gdj1j0)*gdj0j1+(delta_i0i1-gui1i0)*gui0i1*(1.-gdj0j0)*(1.-gdj1j1)+(delta_i0i1-gui1i0)*gui0i1*(delta_j0j1-gdj1j0)*gdj0j1;
		const num uduu = +(1.-gui0i0)*(1.-gdi1i1)*(1.-guj0j0)*(1.-guj1j1)+(1.-gui0i0)*(1.-gdi1i1)*(delta_j0j1-guj1j0)*guj0j1+(delta_i0j0-guj0i0)*gui0j0*(1.-gdi1i1)*(1.-guj1j1)-(delta_i0j0-guj0i0)*gui0j1*(1.-gdi1i1)*(delta_j0j1-guj1j0)+(delta_i0j1-guj1i0)*gui0j0*(1.-gdi1i1)*guj0j1+(delta_i0j1-guj1i0)*gui0j1*(1.-gdi1i1)*(1.-guj0j0);
		const num udud = +(1.-gui0i0)*(1.-gdi1i1)*(1.-guj0j0)*(1.-gdj1j1)+(1.-gui0i0)*(delta_i1j1-gdj1i1)*gdi1j1*(1.-guj0j0)+(delta_i0j0-guj0i0)*gui0j0*(1.-gdi1i1)*(1.-gdj1j1)+(delta_i0j0-guj0i0)*gui0j0*(delta_i1j1-gdj1i1)*gdi1j1;
		const num uddu = +(1.-gui0i0)*(1.-gdi1i1)*(1.-gdj0j0)*(1.-guj1j1)+(1.-gui0i0)*(delta_i1j0-gdj0i1)*gdi1j0*(1.-guj1j1)+(delta_i0j1-guj1i0)*gui0j1*(1.-gdi1i1)*(1.-gdj0j0)+(delta_i0j1-guj1i0)*gui0j1*(delta_i1j0-gdj0i1)*gdi1j0;
		const num uddd = +(1.-gui0i0)*(1.-gdi1i1)*(1.-gdj0j0)*(1.-gdj1j1)+(1.-gui0i0)*(1.-gdi1i1)*(delta_j0j1-gdj1j0)*gdj0j1+(1.-gui0i0)*(delta_i1j0-gdj0i1)*gdi1j0*(1.-gdj1j1)-(1.-gui0i0)*(delta_i1j0-gdj0i1)*gdi1j1*(delta_j0j1-gdj1j0)+(1.-gui0i0)*(delta_i1j1-gdj1i1)*gdi1j0*gdj0j1+(1.-gui0i0)*(delta_i1j1-gdj1i1)*gdi1j1*(1.-gdj0j0);
		const num duuu = +(1.-gdi0i0)*(1.-gui1i1)*(1.-guj0j0)*(1.-guj1j1)+(1.-gdi0i0)*(1.-gui1i1)*(delta_j0j1-guj1j0)*guj0j1+(1.-gdi0i0)*(delta_i1j0-guj0i1)*gui1j0*(1.-guj1j1)-(1.-gdi0i0)*(delta_i1j0-guj0i1)*gui1j1*(delta_j0j1-guj1j0)+(1.-gdi0i0)*(delta_i1j1-guj1i1)*gui1j0*guj0j1+(1.-gdi0i0)*(delta_i1j1-guj1i1)*gui1j1*(1.-guj0j0);
		const num duud = +(1.-gdi0i0)*(1.-gui1i1)*(1.-guj0j0)*(1.-gdj1j1)+(1.-gdi0i0)*(delta_i1j0-guj0i1)*gui1j0*(1.-gdj1j1)+(delta_i0j1-gdj1i0)*gdi0j1*(1.-gui1i1)*(1.-guj0j0)+(delta_i0j1-gdj1i0)*gdi0j1*(delta_i1j0-guj0i1)*gui1j0;
		const num dudu = +(1.-gdi0i0)*(1.-gui1i1)*(1.-gdj0j0)*(1.-guj1j1)+(1.-gdi0i0)*(delta_i1j1-guj1i1)*gui1j1*(1.-gdj0j0)+(delta_i0j0-gdj0i0)*gdi0j0*(1.-gui1i1)*(1.-guj1j1)+(delta_i0j0-gdj0i0)*gdi0j0*(delta_i1j1-guj1i1)*gui1j1;
		const num dudd = +(1.-gdi0i0)*(1.-gui1i1)*(1.-gdj0j0)*(1.-gdj1j1)+(1.-gdi0i0)*(1.-gui1i1)*(delta_j0j1-gdj1j0)*gdj0j1+(delta_i0j0-gdj0i0)*gdi0j0*(1.-gui1i1)*(1.-gdj1j1)-(delta_i0j0-gdj0i0)*gdi0j1*(1.-gui1i1)*(delta_j0j1-gdj1j0)+(delta_i0j1-gdj1i0)*gdi0j0*(1.-gui1i1)*gdj0j1+(delta_i0j1-gdj1i0)*gdi0j1*(1.-gui1i1)*(1.-gdj0j0);
		const num dduu = +(1.-gdi0i0)*(1.-gdi1i1)*(1.-guj0j0)*(1.-guj1j1)+(1.-gdi0i0)*(1.-gdi1i1)*(delta_j0j1-guj1j0)*guj0j1+(delta_i0i1-gdi1i0)*gdi0i1*(1.-guj0j0)*(1.-guj1j1)+(delta_i0i1-gdi1i0)*gdi0i1*(delta_j0j1-guj1j0)*guj0j1;
		const num ddud = +(1.-gdi0i0)*(1.-gdi1i1)*(1.-guj0j0)*(1.-gdj1j1)+(1.-gdi0i0)*(delta_i1j1-gdj1i1)*gdi1j1*(1.-guj0j0)+(delta_i0i1-gdi1i0)*gdi0i1*(1.-guj0j0)*(1.-gdj1j1)-(delta_i0i1-gdi1i0)*gdi0j1*(delta_i1j1-gdj1i1)*(1.-guj0j0)+(delta_i0j1-gdj1i0)*gdi0i1*gdi1j1*(1.-guj0j0)+(delta_i0j1-gdj1i0)*gdi0j1*(1.-gdi1i1)*(1.-guj0j0);
		const num dddu = +(1.-gdi0i0)*(1.-gdi1i1)*(1.-gdj0j0)*(1.-guj1j1)+(1.-gdi0i0)*(delta_i1j0-gdj0i1)*gdi1j0*(1.-guj1j1)+(delta_i0i1-gdi1i0)*gdi0i1*(1.-gdj0j0)*(1.-guj1j1)-(delta_i0i1-gdi1i0)*gdi0j0*(delta_i1j0-gdj0i1)*(1.-guj1j1)+(delta_i0j0-gdj0i0)*gdi0i1*gdi1j0*(1.-guj1j1)+(delta_i0j0-gdj0i0)*gdi0j0*(1.-gdi1i1)*(1.-guj1j1);
		const num dddd = +(1.-gdi0i0)*(1.-gdi1i1)*(1.-gdj0j0)*(1.-gdj1j1)+(1.-gdi0i0)*(1.-gdi1i1)*(delta_j0j1-gdj1j0)*gdj0j1+(1.-gdi0i0)*(delta_i1j0-gdj0i1)*gdi1j0*(1.-gdj1j1)-(1.-gdi0i0)*(delta_i1j0-gdj0i1)*gdi1j1*(delta_j0j1-gdj1j0)+(1.-gdi0i0)*(delta_i1j1-gdj1i1)*gdi1j0*gdj0j1+(1.-gdi0i0)*(delta_i1j1-gdj1i1)*gdi1j1*(1.-gdj0j0)+(delta_i0i1-gdi1i0)*gdi0i1*(1.-gdj0j0)*(1.-gdj1j1)+(delta_i0i1-gdi1i0)*gdi0i1*(delta_j0j1-gdj1j0)*gdj0j1-(delta_i0i1-gdi1i0)*gdi0j0*(delta_i1j0-gdj0i1)*(1.-gdj1j1)-(delta_i0i1-gdi1i0)*gdi0j0*(delta_i1j1-gdj1i1)*gdj0j1+(delta_i0i1-gdi1i0)*gdi0j1*(delta_i1j0-gdj0i1)*(delta_j0j1-gdj1j0)-(delta_i0i1-gdi1i0)*gdi0j1*(delta_i1j1-gdj1i1)*(1.-gdj0j0)+(delta_i0j0-gdj0i0)*gdi0i1*gdi1j0*(1.-gdj1j1)-(delta_i0j0-gdj0i0)*gdi0i1*gdi1j1*(delta_j0j1-gdj1j0)+(delta_i0j0-gdj0i0)*gdi0j0*(1.-gdi1i1)*(1.-gdj1j1)+(delta_i0j0-gdj0i0)*gdi0j0*(delta_i1j1-gdj1i1)*gdi1j1-(delta_i0j0-gdj0i0)*gdi0j1*(1.-gdi1i1)*(delta_j0j1-gdj1j0)-(delta_i0j0-gdj0i0)*gdi0j1*(delta_i1j1-gdj1i1)*gdi1j0+(delta_i0j1-gdj1i0)*gdi0i1*gdi1j0*gdj0j1+(delta_i0j1-gdj1i0)*gdi0i1*gdi1j1*(1.-gdj0j0)+(delta_i0j1-gdj1i0)*gdi0j0*(1.-gdi1i1)*gdj0j1-(delta_i0j1-gdj1i0)*gdi0j0*(delta_i1j0-gdj0i1)*gdi1j1+(delta_i0j1-gdj1i0)*gdi0j1*(1.-gdi1i1)*(1.-gdj0j0)+(delta_i0j1-gdj1i0)*gdi0j1*(delta_i1j0-gdj0i1)*gdi1j0;
		m->nem_nnnn[bb] += pre*(uuuu + uuud + uudu + uudd
				      + uduu + udud + uddu + uddd
				      + duuu + duud + dudu + dudd
				      + dduu + ddud + dddu + dddd);
		m->nem_ssss[bb] += pre*(uuuu - uuud - uudu + uudd
				      - uduu + udud + uddu - uddd
				      - duuu + duud + dudu - dudd
				      + dduu - ddud - dddu + dddd);
	}
	}


	// no delta functions here.
	if (meas_bond_corr)
	#pragma omp parallel for
	for (int t = 1; t < L; t++) {
		const num *const restrict Gu0t_t = Gu0t + N*N*t;
		const num *const restrict Gutt_t = Gutt + N*N*t;
		const num *const restrict Gut0_t = Gut0 + N*N*t;
		const num *const restrict Gd0t_t = Gd0t + N*N*t;
		const num *const restrict Gdtt_t = Gdtt + N*N*t;
		const num *const restrict Gdt0_t = Gdt0 + N*N*t;
	for (int c = 0; c < num_b; c++) {
		const int j0 = p->bonds[c];
		const int j1 = p->bonds[c + num_b];
#ifdef USE_PEIERLS
		const num puj0j1 = p->peierlsu[j0 + N*j1];
		const num puj1j0 = p->peierlsu[j1 + N*j0];
		const num pdj0j1 = p->peierlsd[j0 + N*j1];
		const num pdj1j0 = p->peierlsd[j1 + N*j0];
#endif
	for (int b = 0; b < num_b; b++) {
		const int i0 = p->bonds[b];
		const int i1 = p->bonds[b + num_b];
#ifdef USE_PEIERLS
		const num pui0i1 = p->peierlsu[i0 + N*i1];
		const num pui1i0 = p->peierlsu[i1 + N*i0];
		const num pdi0i1 = p->peierlsd[i0 + N*i1];
		const num pdi1i0 = p->peierlsd[i1 + N*i0];
#endif
		const int bb = p->map_bb[b + c*num_b];
		const num pre = phase / p->degen_bb[bb];
		const num gui0i0 = Gutt_t[i0 + i0*N];
		const num gui1i0 = Gutt_t[i1 + i0*N];
		const num gui0i1 = Gutt_t[i0 + i1*N];
		const num gui1i1 = Gutt_t[i1 + i1*N];
		const num gui0j0 = Gut0_t[i0 + j0*N];
		const num gui1j0 = Gut0_t[i1 + j0*N];
		const num gui0j1 = Gut0_t[i0 + j1*N];
		const num gui1j1 = Gut0_t[i1 + j1*N];
		const num guj0i0 = Gu0t_t[j0 + i0*N];
		const num guj1i0 = Gu0t_t[j1 + i0*N];
		const num guj0i1 = Gu0t_t[j0 + i1*N];
		const num guj1i1 = Gu0t_t[j1 + i1*N];
		const num guj0j0 = Gu00[j0 + j0*N];
		const num guj1j0 = Gu00[j1 + j0*N];
		const num guj0j1 = Gu00[j0 + j1*N];
		const num guj1j1 = Gu00[j1 + j1*N];
		const num gdi0i0 = Gdtt_t[i0 + i0*N];
		const num gdi1i0 = Gdtt_t[i1 + i0*N];
		const num gdi0i1 = Gdtt_t[i0 + i1*N];
		const num gdi1i1 = Gdtt_t[i1 + i1*N];
		const num gdi0j0 = Gdt0_t[i0 + j0*N];
		const num gdi1j0 = Gdt0_t[i1 + j0*N];
		const num gdi0j1 = Gdt0_t[i0 + j1*N];
		const num gdi1j1 = Gdt0_t[i1 + j1*N];
		const num gdj0i0 = Gd0t_t[j0 + i0*N];
		const num gdj1i0 = Gd0t_t[j1 + i0*N];
		const num gdj0i1 = Gd0t_t[j0 + i1*N];
		const num gdj1i1 = Gd0t_t[j1 + i1*N];
		const num gdj0j0 = Gd00[j0 + j0*N];
		const num gdj1j0 = Gd00[j1 + j0*N];
		const num gdj0j1 = Gd00[j0 + j1*N];
		const num gdj1j1 = Gd00[j1 + j1*N];
		m->pair_bb[bb + num_bb*t] += 0.5*pre*(gui0j0*gdi1j1 + gui1j0*gdi0j1 + gui0j1*gdi1j0 + gui1j1*gdi0j0);
		const num x = -pui0i1*puj0j1*guj1i0*gui1j0 - pui1i0*puj1j0*guj0i1*gui0j1
		             - pdi0i1*pdj0j1*gdj1i0*gdi1j0 - pdi1i0*pdj1j0*gdj0i1*gdi0j1;
		const num y = -pui0i1*puj1j0*guj0i0*gui1j1 - pui1i0*puj0j1*guj1i1*gui0j0
		             - pdi0i1*pdj1j0*gdj0i0*gdi1j1 - pdi1i0*pdj0j1*gdj1i1*gdi0j0;
		m->jj[bb + num_bb*t]   += pre*((pui1i0*gui0i1 - pui0i1*gui1i0 + pdi1i0*gdi0i1 - pdi0i1*gdi1i0)
		                              *(puj1j0*guj0j1 - puj0j1*guj1j0 + pdj1j0*gdj0j1 - pdj0j1*gdj1j0) + x - y);
		m->jsjs[bb + num_bb*t] += pre*((pui1i0*gui0i1 - pui0i1*gui1i0 - pdi1i0*gdi0i1 + pdi0i1*gdi1i0)
		                              *(puj1j0*guj0j1 - puj0j1*guj1j0 - pdj1j0*gdj0j1 + pdj0j1*gdj1j0) + x - y);
		m->kk[bb + num_bb*t]   += pre*((pui1i0*gui0i1 + pui0i1*gui1i0 + pdi1i0*gdi0i1 + pdi0i1*gdi1i0)
		                              *(puj1j0*guj0j1 + puj0j1*guj1j0 + pdj1j0*gdj0j1 + pdj0j1*gdj1j0) + x + y);
		m->ksks[bb + num_bb*t] += pre*((pui1i0*gui0i1 + pui0i1*gui1i0 - pdi1i0*gdi0i1 - pdi0i1*gdi1i0)
		                              *(puj1j0*guj0j1 + puj0j1*guj1j0 - pdj1j0*gdj0j1 - pdj0j1*gdj1j0) + x + y);
	}
	}
	}

	if (meas_nematic_corr)
	#pragma omp parallel for
	for (int t = 1; t < L; t++) {
		const num *const restrict Gu0t_t = Gu0t + N*N*t;
		const num *const restrict Gutt_t = Gutt + N*N*t;
		const num *const restrict Gut0_t = Gut0 + N*N*t;
		const num *const restrict Gd0t_t = Gd0t + N*N*t;
		const num *const restrict Gdtt_t = Gdtt + N*N*t;
		const num *const restrict Gdt0_t = Gdt0 + N*N*t;
	for (int c = 0; c < NEM_BONDS*N; c++) {
		const int j0 = p->bonds[c];
		const int j1 = p->bonds[c + num_b];
	for (int b = 0; b < NEM_BONDS*N; b++) {
		const int i0 = p->bonds[b];
		const int i1 = p->bonds[b + num_b];
		const int bb = p->map_bb[b + c*num_b];
		const num pre = phase / p->degen_bb[bb];
		const num gui0i0 = Gutt_t[i0 + i0*N];
		const num gui1i0 = Gutt_t[i1 + i0*N];
		const num gui0i1 = Gutt_t[i0 + i1*N];
		const num gui1i1 = Gutt_t[i1 + i1*N];
		const num gui0j0 = Gut0_t[i0 + j0*N];
		const num gui1j0 = Gut0_t[i1 + j0*N];
		const num gui0j1 = Gut0_t[i0 + j1*N];
		const num gui1j1 = Gut0_t[i1 + j1*N];
		const num guj0i0 = Gu0t_t[j0 + i0*N];
		const num guj1i0 = Gu0t_t[j1 + i0*N];
		const num guj0i1 = Gu0t_t[j0 + i1*N];
		const num guj1i1 = Gu0t_t[j1 + i1*N];
		const num guj0j0 = Gu00[j0 + j0*N];
		const num guj1j0 = Gu00[j1 + j0*N];
		const num guj0j1 = Gu00[j0 + j1*N];
		const num guj1j1 = Gu00[j1 + j1*N];
		const num gdi0i0 = Gdtt_t[i0 + i0*N];
		const num gdi1i0 = Gdtt_t[i1 + i0*N];
		const num gdi0i1 = Gdtt_t[i0 + i1*N];
		const num gdi1i1 = Gdtt_t[i1 + i1*N];
		const num gdi0j0 = Gdt0_t[i0 + j0*N];
		const num gdi1j0 = Gdt0_t[i1 + j0*N];
		const num gdi0j1 = Gdt0_t[i0 + j1*N];
		const num gdi1j1 = Gdt0_t[i1 + j1*N];
		const num gdj0i0 = Gd0t_t[j0 + i0*N];
		const num gdj1i0 = Gd0t_t[j1 + i0*N];
		const num gdj0i1 = Gd0t_t[j0 + i1*N];
		const num gdj1i1 = Gd0t_t[j1 + i1*N];
		const num gdj0j0 = Gd00[j0 + j0*N];
		const num gdj1j0 = Gd00[j1 + j0*N];
		const num gdj0j1 = Gd00[j0 + j1*N];
		const num gdj1j1 = Gd00[j1 + j1*N];
		const int delta_i0i1 = 0;
		const int delta_j0j1 = 0;
		const int delta_i0j0 = 0;
		const int delta_i1j0 = 0;
		const int delta_i0j1 = 0;
		const int delta_i1j1 = 0;
		const num uuuu = +(1.-gui0i0)*(1.-gui1i1)*(1.-guj0j0)*(1.-guj1j1)+(1.-gui0i0)*(1.-gui1i1)*(delta_j0j1-guj1j0)*guj0j1+(1.-gui0i0)*(delta_i1j0-guj0i1)*gui1j0*(1.-guj1j1)-(1.-gui0i0)*(delta_i1j0-guj0i1)*gui1j1*(delta_j0j1-guj1j0)+(1.-gui0i0)*(delta_i1j1-guj1i1)*gui1j0*guj0j1+(1.-gui0i0)*(delta_i1j1-guj1i1)*gui1j1*(1.-guj0j0)+(delta_i0i1-gui1i0)*gui0i1*(1.-guj0j0)*(1.-guj1j1)+(delta_i0i1-gui1i0)*gui0i1*(delta_j0j1-guj1j0)*guj0j1-(delta_i0i1-gui1i0)*gui0j0*(delta_i1j0-guj0i1)*(1.-guj1j1)-(delta_i0i1-gui1i0)*gui0j0*(delta_i1j1-guj1i1)*guj0j1+(delta_i0i1-gui1i0)*gui0j1*(delta_i1j0-guj0i1)*(delta_j0j1-guj1j0)-(delta_i0i1-gui1i0)*gui0j1*(delta_i1j1-guj1i1)*(1.-guj0j0)+(delta_i0j0-guj0i0)*gui0i1*gui1j0*(1.-guj1j1)-(delta_i0j0-guj0i0)*gui0i1*gui1j1*(delta_j0j1-guj1j0)+(delta_i0j0-guj0i0)*gui0j0*(1.-gui1i1)*(1.-guj1j1)+(delta_i0j0-guj0i0)*gui0j0*(delta_i1j1-guj1i1)*gui1j1-(delta_i0j0-guj0i0)*gui0j1*(1.-gui1i1)*(delta_j0j1-guj1j0)-(delta_i0j0-guj0i0)*gui0j1*(delta_i1j1-guj1i1)*gui1j0+(delta_i0j1-guj1i0)*gui0i1*gui1j0*guj0j1+(delta_i0j1-guj1i0)*gui0i1*gui1j1*(1.-guj0j0)+(delta_i0j1-guj1i0)*gui0j0*(1.-gui1i1)*guj0j1-(delta_i0j1-guj1i0)*gui0j0*(delta_i1j0-guj0i1)*gui1j1+(delta_i0j1-guj1i0)*gui0j1*(1.-gui1i1)*(1.-guj0j0)+(delta_i0j1-guj1i0)*gui0j1*(delta_i1j0-guj0i1)*gui1j0;
		const num uuud = +(1.-gui0i0)*(1.-gui1i1)*(1.-guj0j0)*(1.-gdj1j1)+(1.-gui0i0)*(delta_i1j0-guj0i1)*gui1j0*(1.-gdj1j1)+(delta_i0i1-gui1i0)*gui0i1*(1.-guj0j0)*(1.-gdj1j1)-(delta_i0i1-gui1i0)*gui0j0*(delta_i1j0-guj0i1)*(1.-gdj1j1)+(delta_i0j0-guj0i0)*gui0i1*gui1j0*(1.-gdj1j1)+(delta_i0j0-guj0i0)*gui0j0*(1.-gui1i1)*(1.-gdj1j1);
		const num uudu = +(1.-gui0i0)*(1.-gui1i1)*(1.-gdj0j0)*(1.-guj1j1)+(1.-gui0i0)*(delta_i1j1-guj1i1)*gui1j1*(1.-gdj0j0)+(delta_i0i1-gui1i0)*gui0i1*(1.-gdj0j0)*(1.-guj1j1)-(delta_i0i1-gui1i0)*gui0j1*(delta_i1j1-guj1i1)*(1.-gdj0j0)+(delta_i0j1-guj1i0)*gui0i1*gui1j1*(1.-gdj0j0)+(delta_i0j1-guj1i0)*gui0j1*(1.-gui1i1)*(1.-gdj0j0);
		const num uudd = +(1.-gui0i0)*(1.-gui1i1)*(1.-gdj0j0)*(1.-gdj1j1)+(1.-gui0i0)*(1.-gui1i1)*(delta_j0j1-gdj1j0)*gdj0j1+(delta_i0i1-gui1i0)*gui0i1*(1.-gdj0j0)*(1.-gdj1j1)+(delta_i0i1-gui1i0)*gui0i1*(delta_j0j1-gdj1j0)*gdj0j1;
		const num uduu = +(1.-gui0i0)*(1.-gdi1i1)*(1.-guj0j0)*(1.-guj1j1)+(1.-gui0i0)*(1.-gdi1i1)*(delta_j0j1-guj1j0)*guj0j1+(delta_i0j0-guj0i0)*gui0j0*(1.-gdi1i1)*(1.-guj1j1)-(delta_i0j0-guj0i0)*gui0j1*(1.-gdi1i1)*(delta_j0j1-guj1j0)+(delta_i0j1-guj1i0)*gui0j0*(1.-gdi1i1)*guj0j1+(delta_i0j1-guj1i0)*gui0j1*(1.-gdi1i1)*(1.-guj0j0);
		const num udud = +(1.-gui0i0)*(1.-gdi1i1)*(1.-guj0j0)*(1.-gdj1j1)+(1.-gui0i0)*(delta_i1j1-gdj1i1)*gdi1j1*(1.-guj0j0)+(delta_i0j0-guj0i0)*gui0j0*(1.-gdi1i1)*(1.-gdj1j1)+(delta_i0j0-guj0i0)*gui0j0*(delta_i1j1-gdj1i1)*gdi1j1;
		const num uddu = +(1.-gui0i0)*(1.-gdi1i1)*(1.-gdj0j0)*(1.-guj1j1)+(1.-gui0i0)*(delta_i1j0-gdj0i1)*gdi1j0*(1.-guj1j1)+(delta_i0j1-guj1i0)*gui0j1*(1.-gdi1i1)*(1.-gdj0j0)+(delta_i0j1-guj1i0)*gui0j1*(delta_i1j0-gdj0i1)*gdi1j0;
		const num uddd = +(1.-gui0i0)*(1.-gdi1i1)*(1.-gdj0j0)*(1.-gdj1j1)+(1.-gui0i0)*(1.-gdi1i1)*(delta_j0j1-gdj1j0)*gdj0j1+(1.-gui0i0)*(delta_i1j0-gdj0i1)*gdi1j0*(1.-gdj1j1)-(1.-gui0i0)*(delta_i1j0-gdj0i1)*gdi1j1*(delta_j0j1-gdj1j0)+(1.-gui0i0)*(delta_i1j1-gdj1i1)*gdi1j0*gdj0j1+(1.-gui0i0)*(delta_i1j1-gdj1i1)*gdi1j1*(1.-gdj0j0);
		const num duuu = +(1.-gdi0i0)*(1.-gui1i1)*(1.-guj0j0)*(1.-guj1j1)+(1.-gdi0i0)*(1.-gui1i1)*(delta_j0j1-guj1j0)*guj0j1+(1.-gdi0i0)*(delta_i1j0-guj0i1)*gui1j0*(1.-guj1j1)-(1.-gdi0i0)*(delta_i1j0-guj0i1)*gui1j1*(delta_j0j1-guj1j0)+(1.-gdi0i0)*(delta_i1j1-guj1i1)*gui1j0*guj0j1+(1.-gdi0i0)*(delta_i1j1-guj1i1)*gui1j1*(1.-guj0j0);
		const num duud = +(1.-gdi0i0)*(1.-gui1i1)*(1.-guj0j0)*(1.-gdj1j1)+(1.-gdi0i0)*(delta_i1j0-guj0i1)*gui1j0*(1.-gdj1j1)+(delta_i0j1-gdj1i0)*gdi0j1*(1.-gui1i1)*(1.-guj0j0)+(delta_i0j1-gdj1i0)*gdi0j1*(delta_i1j0-guj0i1)*gui1j0;
		const num dudu = +(1.-gdi0i0)*(1.-gui1i1)*(1.-gdj0j0)*(1.-guj1j1)+(1.-gdi0i0)*(delta_i1j1-guj1i1)*gui1j1*(1.-gdj0j0)+(delta_i0j0-gdj0i0)*gdi0j0*(1.-gui1i1)*(1.-guj1j1)+(delta_i0j0-gdj0i0)*gdi0j0*(delta_i1j1-guj1i1)*gui1j1;
		const num dudd = +(1.-gdi0i0)*(1.-gui1i1)*(1.-gdj0j0)*(1.-gdj1j1)+(1.-gdi0i0)*(1.-gui1i1)*(delta_j0j1-gdj1j0)*gdj0j1+(delta_i0j0-gdj0i0)*gdi0j0*(1.-gui1i1)*(1.-gdj1j1)-(delta_i0j0-gdj0i0)*gdi0j1*(1.-gui1i1)*(delta_j0j1-gdj1j0)+(delta_i0j1-gdj1i0)*gdi0j0*(1.-gui1i1)*gdj0j1+(delta_i0j1-gdj1i0)*gdi0j1*(1.-gui1i1)*(1.-gdj0j0);
		const num dduu = +(1.-gdi0i0)*(1.-gdi1i1)*(1.-guj0j0)*(1.-guj1j1)+(1.-gdi0i0)*(1.-gdi1i1)*(delta_j0j1-guj1j0)*guj0j1+(delta_i0i1-gdi1i0)*gdi0i1*(1.-guj0j0)*(1.-guj1j1)+(delta_i0i1-gdi1i0)*gdi0i1*(delta_j0j1-guj1j0)*guj0j1;
		const num ddud = +(1.-gdi0i0)*(1.-gdi1i1)*(1.-guj0j0)*(1.-gdj1j1)+(1.-gdi0i0)*(delta_i1j1-gdj1i1)*gdi1j1*(1.-guj0j0)+(delta_i0i1-gdi1i0)*gdi0i1*(1.-guj0j0)*(1.-gdj1j1)-(delta_i0i1-gdi1i0)*gdi0j1*(delta_i1j1-gdj1i1)*(1.-guj0j0)+(delta_i0j1-gdj1i0)*gdi0i1*gdi1j1*(1.-guj0j0)+(delta_i0j1-gdj1i0)*gdi0j1*(1.-gdi1i1)*(1.-guj0j0);
		const num dddu = +(1.-gdi0i0)*(1.-gdi1i1)*(1.-gdj0j0)*(1.-guj1j1)+(1.-gdi0i0)*(delta_i1j0-gdj0i1)*gdi1j0*(1.-guj1j1)+(delta_i0i1-gdi1i0)*gdi0i1*(1.-gdj0j0)*(1.-guj1j1)-(delta_i0i1-gdi1i0)*gdi0j0*(delta_i1j0-gdj0i1)*(1.-guj1j1)+(delta_i0j0-gdj0i0)*gdi0i1*gdi1j0*(1.-guj1j1)+(delta_i0j0-gdj0i0)*gdi0j0*(1.-gdi1i1)*(1.-guj1j1);
		const num dddd = +(1.-gdi0i0)*(1.-gdi1i1)*(1.-gdj0j0)*(1.-gdj1j1)+(1.-gdi0i0)*(1.-gdi1i1)*(delta_j0j1-gdj1j0)*gdj0j1+(1.-gdi0i0)*(delta_i1j0-gdj0i1)*gdi1j0*(1.-gdj1j1)-(1.-gdi0i0)*(delta_i1j0-gdj0i1)*gdi1j1*(delta_j0j1-gdj1j0)+(1.-gdi0i0)*(delta_i1j1-gdj1i1)*gdi1j0*gdj0j1+(1.-gdi0i0)*(delta_i1j1-gdj1i1)*gdi1j1*(1.-gdj0j0)+(delta_i0i1-gdi1i0)*gdi0i1*(1.-gdj0j0)*(1.-gdj1j1)+(delta_i0i1-gdi1i0)*gdi0i1*(delta_j0j1-gdj1j0)*gdj0j1-(delta_i0i1-gdi1i0)*gdi0j0*(delta_i1j0-gdj0i1)*(1.-gdj1j1)-(delta_i0i1-gdi1i0)*gdi0j0*(delta_i1j1-gdj1i1)*gdj0j1+(delta_i0i1-gdi1i0)*gdi0j1*(delta_i1j0-gdj0i1)*(delta_j0j1-gdj1j0)-(delta_i0i1-gdi1i0)*gdi0j1*(delta_i1j1-gdj1i1)*(1.-gdj0j0)+(delta_i0j0-gdj0i0)*gdi0i1*gdi1j0*(1.-gdj1j1)-(delta_i0j0-gdj0i0)*gdi0i1*gdi1j1*(delta_j0j1-gdj1j0)+(delta_i0j0-gdj0i0)*gdi0j0*(1.-gdi1i1)*(1.-gdj1j1)+(delta_i0j0-gdj0i0)*gdi0j0*(delta_i1j1-gdj1i1)*gdi1j1-(delta_i0j0-gdj0i0)*gdi0j1*(1.-gdi1i1)*(delta_j0j1-gdj1j0)-(delta_i0j0-gdj0i0)*gdi0j1*(delta_i1j1-gdj1i1)*gdi1j0+(delta_i0j1-gdj1i0)*gdi0i1*gdi1j0*gdj0j1+(delta_i0j1-gdj1i0)*gdi0i1*gdi1j1*(1.-gdj0j0)+(delta_i0j1-gdj1i0)*gdi0j0*(1.-gdi1i1)*gdj0j1-(delta_i0j1-gdj1i0)*gdi0j0*(delta_i1j0-gdj0i1)*gdi1j1+(delta_i0j1-gdj1i0)*gdi0j1*(1.-gdi1i1)*(1.-gdj0j0)+(delta_i0j1-gdj1i0)*gdi0j1*(delta_i1j0-gdj0i1)*gdi1j0;
		m->nem_nnnn[bb + num_bb*t] += pre*(uuuu + uuud + uudu + uudd
		                                 + uduu + udud + uddu + uddd
		                                 + duuu + duud + dudu + dudd
		                                 + dduu + ddud + dddu + dddd);
		m->nem_ssss[bb + num_bb*t] += pre*(uuuu - uuud - uudu + uudd
		                                 - uduu + udud + uddu - uddd
		                                 - duuu + duud + dudu - dudd
		                                 + dduu - ddud - dddu + dddd);
	}
	}
	}
}


void measure_uneqlt_full(const struct params *const restrict p, const num phase,
		const num *const Gu,
		const num *const Gd,
		struct meas_uneqlt *const restrict m)
{
	m->n_sample++;
	m->sign += phase;
	const int N = p->N, L = p->L, num_i = p->num_i, num_ij = p->num_ij;
	const int num_b = p->num_b, num_bs = p->num_bs, num_bb = p->num_bb, num_bbb = p->num_bbb, num_bbb_lim = p->num_bbb_lim;
	const int meas_bond_corr = p->meas_bond_corr;
	const int meas_energy_corr = p->meas_energy_corr;
	const int meas_nematic_corr = p->meas_nematic_corr;
        const int meas_3curr = p->meas_3curr;
        const int meas_3curr_limit = p-> meas_3curr_limit;


	const num *const restrict Gu00 = Gu;
	const num *const restrict Gd00 = Gd;

	// 2 site measurements
	#pragma omp parallel for
	for (int t = 0; t < L; t++) {
		const int delta_t = (t == 0);
		const num *const restrict Gu0t_t = Gu + N*N*(0+L*t);
		const num *const restrict Gutt_t = Gu + N*N*(t+L*t);
		const num *const restrict Gut0_t = Gu + N*N*(t+L*0);
		const num *const restrict Gd0t_t = Gd + N*N*(0+L*t);
		const num *const restrict Gdtt_t = Gd + N*N*(t+L*t);
		const num *const restrict Gdt0_t = Gd + N*N*(t+L*0);
	for (int j = 0; j < N; j++)
	for (int i = 0; i < N; i++) {
		const int r = p->map_ij[i + j*N];
		const int delta_tij = delta_t * (i == j);
		const num pre = phase / p->degen_ij[r];
		const num guii = Gutt_t[i + N*i];
		const num guij = Gut0_t[i + N*j];
		const num guji = Gu0t_t[j + N*i];
		const num gujj = Gu00[j + N*j];
		const num gdii = Gdtt_t[i + N*i];
		const num gdij = Gdt0_t[i + N*j];
		const num gdji = Gd0t_t[j + N*i];
		const num gdjj = Gd00[j + N*j];
#ifdef USE_PEIERLS
		m->gt0[r + num_ij*t] += 0.5*pre*(guij*p->peierlsu[j + i*N] + gdij*p->peierlsd[j + i*N]);
#else
		m->gt0[r + num_ij*t] += 0.5*pre*(guij + gdij);
#endif
		const num x = delta_tij*(guii + gdii) - (guji*guij + gdji*gdij);
		m->nn[r + num_ij*t] += pre*((2. - guii - gdii)*(2. - gujj - gdjj) + x);
		m->xx[r + num_ij*t] += 0.25*pre*(delta_tij*(guii + gdii) - (guji*gdij + gdji*guij));
		m->zz[r + num_ij*t] += 0.25*pre*((gdii - guii)*(gdjj - gujj) + x);
		m->pair_sw[r + num_ij*t] += pre*guij*gdij;
		if (meas_energy_corr) {
			const num nuinuj = (1. - guii)*(1. - gujj) + (delta_tij - guji)*guij;
			const num ndindj = (1. - gdii)*(1. - gdjj) + (delta_tij - gdji)*gdij;
			m->vv[r + num_ij*t] += pre*nuinuj*ndindj;
			m->vn[r + num_ij*t] += pre*(nuinuj*(1. - gdii) + (1. - guii)*ndindj);
		}
	}
	}

	// 1 bond 1 site measurements
	if (meas_energy_corr)
	#pragma omp parallel for
	for (int t = 0; t < L; t++) {
		const int delta_t = (t == 0);
		const num *const restrict Gu0t_t = Gu + N*N*(0+L*t);
		const num *const restrict Gutt_t = Gu + N*N*(t+L*t);
		const num *const restrict Gut0_t = Gu + N*N*(t+L*0);
		const num *const restrict Gd0t_t = Gd + N*N*(0+L*t);
		const num *const restrict Gdtt_t = Gd + N*N*(t+L*t);
		const num *const restrict Gdt0_t = Gd + N*N*(t+L*0);
	for (int j = 0; j < N; j++)
	for (int b = 0; b < num_b; b++) {
		const int i0 = p->bonds[b];
		const int i1 = p->bonds[b + num_b];
#ifdef USE_PEIERLS
		const num pui0i1 = p->peierlsu[i0 + N*i1];
		const num pui1i0 = p->peierlsu[i1 + N*i0];
		const num pdi0i1 = p->peierlsd[i0 + N*i1];
		const num pdi1i0 = p->peierlsd[i1 + N*i0];
#endif
		const int bs = p->map_bs[b + num_b*j];
		const num pre = phase / p->degen_bs[bs];
		const int delta_i0i1 = 0;
		const int delta_i0j = delta_t*(i0 == j);
		const int delta_i1j = delta_t*(i1 == j);
		const num gui0j = Gut0_t[i0 + N*j];
		const num guji0 = Gu0t_t[j + N*i0];
		const num gdi0j = Gdt0_t[i0 + N*j];
		const num gdji0 = Gd0t_t[j + N*i0];
		const num gui1j = Gut0_t[i1 + N*j];
		const num guji1 = Gu0t_t[j + N*i1];
		const num gdi1j = Gdt0_t[i1 + N*j];
		const num gdji1 = Gd0t_t[j + N*i1];
		const num gui0i1 = Gutt_t[i0 + N*i1];
		const num gui1i0 = Gutt_t[i1 + N*i0];
		const num gdi0i1 = Gdtt_t[i0 + N*i1];
		const num gdi1i0 = Gdtt_t[i1 + N*i0];
		const num gujj = Gu00[j + N*j];
		const num gdjj = Gd00[j + N*j];

		const num ku = pui1i0*(delta_i0i1 - gui0i1) + pui0i1*(delta_i0i1 - gui1i0);
		const num kd = pdi1i0*(delta_i0i1 - gdi0i1) + pdi0i1*(delta_i0i1 - gdi1i0);
		const num xu = pui0i1*(delta_i0j - guji0)*gui1j + pui1i0*(delta_i1j - guji1)*gui0j;
		const num xd = pdi0i1*(delta_i0j - gdji0)*gdi1j + pdi1i0*(delta_i1j - gdji1)*gdi0j;
		m->kv[bs + num_bs*t] += pre*((ku*(1. - gujj) + xu)*(1. - gdjj)
		                           + (kd*(1. - gdjj) + xd)*(1. - gujj));
		m->kn[bs + num_bs*t] += pre*((ku + kd)*(2. - gujj - gdjj) + xu + xd);
	}
	}

	// 2 bond measurements
	// minor optimization: handle t = 0 separately, since there are no delta
	// functions for t > 0. not really needed in 2-site measurements above
	// as those are fast anyway
	if (meas_bond_corr)
	for (int c = 0; c < num_b; c++) {
		const int j0 = p->bonds[c];
		const int j1 = p->bonds[c + num_b];
#ifdef USE_PEIERLS
		const num puj0j1 = p->peierlsu[j0 + N*j1];
		const num puj1j0 = p->peierlsu[j1 + N*j0];
		const num pdj0j1 = p->peierlsd[j0 + N*j1];
		const num pdj1j0 = p->peierlsd[j1 + N*j0];
#endif
	for (int b = 0; b < num_b; b++) {
		const int i0 = p->bonds[b];
		const int i1 = p->bonds[b + num_b];
#ifdef USE_PEIERLS
		const num pui0i1 = p->peierlsu[i0 + N*i1];
		const num pui1i0 = p->peierlsu[i1 + N*i0];
		const num pdi0i1 = p->peierlsd[i0 + N*i1];
		const num pdi1i0 = p->peierlsd[i1 + N*i0];
#endif
		const int bb = p->map_bb[b + c*num_b];
		const num pre = phase / p->degen_bb[bb];
		const int delta_i0j0 = (i0 == j0);
		const int delta_i1j0 = (i1 == j0);
		const int delta_i0j1 = (i0 == j1);
		const int delta_i1j1 = (i1 == j1);
		const num gui1i0 = Gu00[i1 + i0*N];
		const num gui0i1 = Gu00[i0 + i1*N];
		const num gui0j0 = Gu00[i0 + j0*N];
		const num gui1j0 = Gu00[i1 + j0*N];
		const num gui0j1 = Gu00[i0 + j1*N];
		const num gui1j1 = Gu00[i1 + j1*N];
		const num guj0i0 = Gu00[j0 + i0*N];
		const num guj1i0 = Gu00[j1 + i0*N];
		const num guj0i1 = Gu00[j0 + i1*N];
		const num guj1i1 = Gu00[j1 + i1*N];
		const num guj1j0 = Gu00[j1 + j0*N];
		const num guj0j1 = Gu00[j0 + j1*N];
		const num gdi1i0 = Gd00[i1 + i0*N];
		const num gdi0i1 = Gd00[i0 + i1*N];
		const num gdi0j0 = Gd00[i0 + j0*N];
		const num gdi1j0 = Gd00[i1 + j0*N];
		const num gdi0j1 = Gd00[i0 + j1*N];
		const num gdi1j1 = Gd00[i1 + j1*N];
		const num gdj0i0 = Gd00[j0 + i0*N];
		const num gdj1i0 = Gd00[j1 + i0*N];
		const num gdj0i1 = Gd00[j0 + i1*N];
		const num gdj1i1 = Gd00[j1 + i1*N];
		const num gdj1j0 = Gd00[j1 + j0*N];
		const num gdj0j1 = Gd00[j0 + j1*N];
		m->pair_bb[bb] += 0.5*pre*(gui0j0*gdi1j1 + gui1j0*gdi0j1 + gui0j1*gdi1j0 + gui1j1*gdi0j0);
		const num x = pui0i1*puj0j1*(delta_i0j1 - guj1i0)*gui1j0 + pui1i0*puj1j0*(delta_i1j0 - guj0i1)*gui0j1
		            + pdi0i1*pdj0j1*(delta_i0j1 - gdj1i0)*gdi1j0 + pdi1i0*pdj1j0*(delta_i1j0 - gdj0i1)*gdi0j1;
		const num y = pui0i1*puj1j0*(delta_i0j0 - guj0i0)*gui1j1 + pui1i0*puj0j1*(delta_i1j1 - guj1i1)*gui0j0
		            + pdi0i1*pdj1j0*(delta_i0j0 - gdj0i0)*gdi1j1 + pdi1i0*pdj0j1*(delta_i1j1 - gdj1i1)*gdi0j0;
		m->jj[bb]   += pre*((pui1i0*gui0i1 - pui0i1*gui1i0 + pdi1i0*gdi0i1 - pdi0i1*gdi1i0)
		                   *(puj1j0*guj0j1 - puj0j1*guj1j0 + pdj1j0*gdj0j1 - pdj0j1*gdj1j0) + x - y);
		m->jsjs[bb] += pre*((pui1i0*gui0i1 - pui0i1*gui1i0 - pdi1i0*gdi0i1 + pdi0i1*gdi1i0)
		                   *(puj1j0*guj0j1 - puj0j1*guj1j0 - pdj1j0*gdj0j1 + pdj0j1*gdj1j0) + x - y);
		m->kk[bb]   += pre*((pui1i0*gui0i1 + pui0i1*gui1i0 + pdi1i0*gdi0i1 + pdi0i1*gdi1i0)
		                   *(puj1j0*guj0j1 + puj0j1*guj1j0 + pdj1j0*gdj0j1 + pdj0j1*gdj1j0) + x + y);
		m->ksks[bb] += pre*((pui1i0*gui0i1 + pui0i1*gui1i0 - pdi1i0*gdi0i1 - pdi0i1*gdi1i0)
		                   *(puj1j0*guj0j1 + puj0j1*guj1j0 - pdj1j0*gdj0j1 - pdj0j1*gdj1j0) + x + y);
	}
	}

	if (meas_nematic_corr)
	for (int c = 0; c < NEM_BONDS*N; c++) {
		const int j0 = p->bonds[c];
		const int j1 = p->bonds[c + num_b];
#ifdef USE_PEIERLS
		const num puj0j1 = p->peierlsu[j0 + N*j1];
		const num puj1j0 = p->peierlsu[j1 + N*j0];
		const num pdj0j1 = p->peierlsd[j0 + N*j1];
		const num pdj1j0 = p->peierlsd[j1 + N*j0];
#endif
	for (int b = 0; b < NEM_BONDS*N; b++) {
		const int i0 = p->bonds[b];
		const int i1 = p->bonds[b + num_b];
#ifdef USE_PEIERLS
		const num pui0i1 = p->peierlsu[i0 + N*i1];
		const num pui1i0 = p->peierlsu[i1 + N*i0];
		const num pdi0i1 = p->peierlsd[i0 + N*i1];
		const num pdi1i0 = p->peierlsd[i1 + N*i0];
#endif
		const int bb = p->map_bb[b + c*num_b];
		const num pre = phase / p->degen_bb[bb];
		const int delta_i0j0 = (i0 == j0);
		const int delta_i1j0 = (i1 == j0);
		const int delta_i0j1 = (i0 == j1);
		const int delta_i1j1 = (i1 == j1);
		const num gui0i0 = Gu00[i0 + i0*N];
		const num gui1i0 = Gu00[i1 + i0*N];
		const num gui0i1 = Gu00[i0 + i1*N];
		const num gui1i1 = Gu00[i1 + i1*N];
		const num gui0j0 = Gu00[i0 + j0*N];
		const num gui1j0 = Gu00[i1 + j0*N];
		const num gui0j1 = Gu00[i0 + j1*N];
		const num gui1j1 = Gu00[i1 + j1*N];
		const num guj0i0 = Gu00[j0 + i0*N];
		const num guj1i0 = Gu00[j1 + i0*N];
		const num guj0i1 = Gu00[j0 + i1*N];
		const num guj1i1 = Gu00[j1 + i1*N];
		const num guj0j0 = Gu00[j0 + j0*N];
		const num guj1j0 = Gu00[j1 + j0*N];
		const num guj0j1 = Gu00[j0 + j1*N];
		const num guj1j1 = Gu00[j1 + j1*N];
		const num gdi0i0 = Gd00[i0 + i0*N];
		const num gdi1i0 = Gd00[i1 + i0*N];
		const num gdi0i1 = Gd00[i0 + i1*N];
		const num gdi1i1 = Gd00[i1 + i1*N];
		const num gdi0j0 = Gd00[i0 + j0*N];
		const num gdi1j0 = Gd00[i1 + j0*N];
		const num gdi0j1 = Gd00[i0 + j1*N];
		const num gdi1j1 = Gd00[i1 + j1*N];
		const num gdj0i0 = Gd00[j0 + i0*N];
		const num gdj1i0 = Gd00[j1 + i0*N];
		const num gdj0i1 = Gd00[j0 + i1*N];
		const num gdj1i1 = Gd00[j1 + i1*N];
		const num gdj0j0 = Gd00[j0 + j0*N];
		const num gdj1j0 = Gd00[j1 + j0*N];
		const num gdj0j1 = Gd00[j0 + j1*N];
		const num gdj1j1 = Gd00[j1 + j1*N];
		const int delta_i0i1 = 0;
		const int delta_j0j1 = 0;
		const num uuuu = +(1.-gui0i0)*(1.-gui1i1)*(1.-guj0j0)*(1.-guj1j1)+(1.-gui0i0)*(1.-gui1i1)*(delta_j0j1-guj1j0)*guj0j1+(1.-gui0i0)*(delta_i1j0-guj0i1)*gui1j0*(1.-guj1j1)-(1.-gui0i0)*(delta_i1j0-guj0i1)*gui1j1*(delta_j0j1-guj1j0)+(1.-gui0i0)*(delta_i1j1-guj1i1)*gui1j0*guj0j1+(1.-gui0i0)*(delta_i1j1-guj1i1)*gui1j1*(1.-guj0j0)+(delta_i0i1-gui1i0)*gui0i1*(1.-guj0j0)*(1.-guj1j1)+(delta_i0i1-gui1i0)*gui0i1*(delta_j0j1-guj1j0)*guj0j1-(delta_i0i1-gui1i0)*gui0j0*(delta_i1j0-guj0i1)*(1.-guj1j1)-(delta_i0i1-gui1i0)*gui0j0*(delta_i1j1-guj1i1)*guj0j1+(delta_i0i1-gui1i0)*gui0j1*(delta_i1j0-guj0i1)*(delta_j0j1-guj1j0)-(delta_i0i1-gui1i0)*gui0j1*(delta_i1j1-guj1i1)*(1.-guj0j0)+(delta_i0j0-guj0i0)*gui0i1*gui1j0*(1.-guj1j1)-(delta_i0j0-guj0i0)*gui0i1*gui1j1*(delta_j0j1-guj1j0)+(delta_i0j0-guj0i0)*gui0j0*(1.-gui1i1)*(1.-guj1j1)+(delta_i0j0-guj0i0)*gui0j0*(delta_i1j1-guj1i1)*gui1j1-(delta_i0j0-guj0i0)*gui0j1*(1.-gui1i1)*(delta_j0j1-guj1j0)-(delta_i0j0-guj0i0)*gui0j1*(delta_i1j1-guj1i1)*gui1j0+(delta_i0j1-guj1i0)*gui0i1*gui1j0*guj0j1+(delta_i0j1-guj1i0)*gui0i1*gui1j1*(1.-guj0j0)+(delta_i0j1-guj1i0)*gui0j0*(1.-gui1i1)*guj0j1-(delta_i0j1-guj1i0)*gui0j0*(delta_i1j0-guj0i1)*gui1j1+(delta_i0j1-guj1i0)*gui0j1*(1.-gui1i1)*(1.-guj0j0)+(delta_i0j1-guj1i0)*gui0j1*(delta_i1j0-guj0i1)*gui1j0;
		const num uuud = +(1.-gui0i0)*(1.-gui1i1)*(1.-guj0j0)*(1.-gdj1j1)+(1.-gui0i0)*(delta_i1j0-guj0i1)*gui1j0*(1.-gdj1j1)+(delta_i0i1-gui1i0)*gui0i1*(1.-guj0j0)*(1.-gdj1j1)-(delta_i0i1-gui1i0)*gui0j0*(delta_i1j0-guj0i1)*(1.-gdj1j1)+(delta_i0j0-guj0i0)*gui0i1*gui1j0*(1.-gdj1j1)+(delta_i0j0-guj0i0)*gui0j0*(1.-gui1i1)*(1.-gdj1j1);
		const num uudu = +(1.-gui0i0)*(1.-gui1i1)*(1.-gdj0j0)*(1.-guj1j1)+(1.-gui0i0)*(delta_i1j1-guj1i1)*gui1j1*(1.-gdj0j0)+(delta_i0i1-gui1i0)*gui0i1*(1.-gdj0j0)*(1.-guj1j1)-(delta_i0i1-gui1i0)*gui0j1*(delta_i1j1-guj1i1)*(1.-gdj0j0)+(delta_i0j1-guj1i0)*gui0i1*gui1j1*(1.-gdj0j0)+(delta_i0j1-guj1i0)*gui0j1*(1.-gui1i1)*(1.-gdj0j0);
		const num uudd = +(1.-gui0i0)*(1.-gui1i1)*(1.-gdj0j0)*(1.-gdj1j1)+(1.-gui0i0)*(1.-gui1i1)*(delta_j0j1-gdj1j0)*gdj0j1+(delta_i0i1-gui1i0)*gui0i1*(1.-gdj0j0)*(1.-gdj1j1)+(delta_i0i1-gui1i0)*gui0i1*(delta_j0j1-gdj1j0)*gdj0j1;
		const num uduu = +(1.-gui0i0)*(1.-gdi1i1)*(1.-guj0j0)*(1.-guj1j1)+(1.-gui0i0)*(1.-gdi1i1)*(delta_j0j1-guj1j0)*guj0j1+(delta_i0j0-guj0i0)*gui0j0*(1.-gdi1i1)*(1.-guj1j1)-(delta_i0j0-guj0i0)*gui0j1*(1.-gdi1i1)*(delta_j0j1-guj1j0)+(delta_i0j1-guj1i0)*gui0j0*(1.-gdi1i1)*guj0j1+(delta_i0j1-guj1i0)*gui0j1*(1.-gdi1i1)*(1.-guj0j0);
		const num udud = +(1.-gui0i0)*(1.-gdi1i1)*(1.-guj0j0)*(1.-gdj1j1)+(1.-gui0i0)*(delta_i1j1-gdj1i1)*gdi1j1*(1.-guj0j0)+(delta_i0j0-guj0i0)*gui0j0*(1.-gdi1i1)*(1.-gdj1j1)+(delta_i0j0-guj0i0)*gui0j0*(delta_i1j1-gdj1i1)*gdi1j1;
		const num uddu = +(1.-gui0i0)*(1.-gdi1i1)*(1.-gdj0j0)*(1.-guj1j1)+(1.-gui0i0)*(delta_i1j0-gdj0i1)*gdi1j0*(1.-guj1j1)+(delta_i0j1-guj1i0)*gui0j1*(1.-gdi1i1)*(1.-gdj0j0)+(delta_i0j1-guj1i0)*gui0j1*(delta_i1j0-gdj0i1)*gdi1j0;
		const num uddd = +(1.-gui0i0)*(1.-gdi1i1)*(1.-gdj0j0)*(1.-gdj1j1)+(1.-gui0i0)*(1.-gdi1i1)*(delta_j0j1-gdj1j0)*gdj0j1+(1.-gui0i0)*(delta_i1j0-gdj0i1)*gdi1j0*(1.-gdj1j1)-(1.-gui0i0)*(delta_i1j0-gdj0i1)*gdi1j1*(delta_j0j1-gdj1j0)+(1.-gui0i0)*(delta_i1j1-gdj1i1)*gdi1j0*gdj0j1+(1.-gui0i0)*(delta_i1j1-gdj1i1)*gdi1j1*(1.-gdj0j0);
		const num duuu = +(1.-gdi0i0)*(1.-gui1i1)*(1.-guj0j0)*(1.-guj1j1)+(1.-gdi0i0)*(1.-gui1i1)*(delta_j0j1-guj1j0)*guj0j1+(1.-gdi0i0)*(delta_i1j0-guj0i1)*gui1j0*(1.-guj1j1)-(1.-gdi0i0)*(delta_i1j0-guj0i1)*gui1j1*(delta_j0j1-guj1j0)+(1.-gdi0i0)*(delta_i1j1-guj1i1)*gui1j0*guj0j1+(1.-gdi0i0)*(delta_i1j1-guj1i1)*gui1j1*(1.-guj0j0);
		const num duud = +(1.-gdi0i0)*(1.-gui1i1)*(1.-guj0j0)*(1.-gdj1j1)+(1.-gdi0i0)*(delta_i1j0-guj0i1)*gui1j0*(1.-gdj1j1)+(delta_i0j1-gdj1i0)*gdi0j1*(1.-gui1i1)*(1.-guj0j0)+(delta_i0j1-gdj1i0)*gdi0j1*(delta_i1j0-guj0i1)*gui1j0;
		const num dudu = +(1.-gdi0i0)*(1.-gui1i1)*(1.-gdj0j0)*(1.-guj1j1)+(1.-gdi0i0)*(delta_i1j1-guj1i1)*gui1j1*(1.-gdj0j0)+(delta_i0j0-gdj0i0)*gdi0j0*(1.-gui1i1)*(1.-guj1j1)+(delta_i0j0-gdj0i0)*gdi0j0*(delta_i1j1-guj1i1)*gui1j1;
		const num dudd = +(1.-gdi0i0)*(1.-gui1i1)*(1.-gdj0j0)*(1.-gdj1j1)+(1.-gdi0i0)*(1.-gui1i1)*(delta_j0j1-gdj1j0)*gdj0j1+(delta_i0j0-gdj0i0)*gdi0j0*(1.-gui1i1)*(1.-gdj1j1)-(delta_i0j0-gdj0i0)*gdi0j1*(1.-gui1i1)*(delta_j0j1-gdj1j0)+(delta_i0j1-gdj1i0)*gdi0j0*(1.-gui1i1)*gdj0j1+(delta_i0j1-gdj1i0)*gdi0j1*(1.-gui1i1)*(1.-gdj0j0);
		const num dduu = +(1.-gdi0i0)*(1.-gdi1i1)*(1.-guj0j0)*(1.-guj1j1)+(1.-gdi0i0)*(1.-gdi1i1)*(delta_j0j1-guj1j0)*guj0j1+(delta_i0i1-gdi1i0)*gdi0i1*(1.-guj0j0)*(1.-guj1j1)+(delta_i0i1-gdi1i0)*gdi0i1*(delta_j0j1-guj1j0)*guj0j1;
		const num ddud = +(1.-gdi0i0)*(1.-gdi1i1)*(1.-guj0j0)*(1.-gdj1j1)+(1.-gdi0i0)*(delta_i1j1-gdj1i1)*gdi1j1*(1.-guj0j0)+(delta_i0i1-gdi1i0)*gdi0i1*(1.-guj0j0)*(1.-gdj1j1)-(delta_i0i1-gdi1i0)*gdi0j1*(delta_i1j1-gdj1i1)*(1.-guj0j0)+(delta_i0j1-gdj1i0)*gdi0i1*gdi1j1*(1.-guj0j0)+(delta_i0j1-gdj1i0)*gdi0j1*(1.-gdi1i1)*(1.-guj0j0);
		const num dddu = +(1.-gdi0i0)*(1.-gdi1i1)*(1.-gdj0j0)*(1.-guj1j1)+(1.-gdi0i0)*(delta_i1j0-gdj0i1)*gdi1j0*(1.-guj1j1)+(delta_i0i1-gdi1i0)*gdi0i1*(1.-gdj0j0)*(1.-guj1j1)-(delta_i0i1-gdi1i0)*gdi0j0*(delta_i1j0-gdj0i1)*(1.-guj1j1)+(delta_i0j0-gdj0i0)*gdi0i1*gdi1j0*(1.-guj1j1)+(delta_i0j0-gdj0i0)*gdi0j0*(1.-gdi1i1)*(1.-guj1j1);
		const num dddd = +(1.-gdi0i0)*(1.-gdi1i1)*(1.-gdj0j0)*(1.-gdj1j1)+(1.-gdi0i0)*(1.-gdi1i1)*(delta_j0j1-gdj1j0)*gdj0j1+(1.-gdi0i0)*(delta_i1j0-gdj0i1)*gdi1j0*(1.-gdj1j1)-(1.-gdi0i0)*(delta_i1j0-gdj0i1)*gdi1j1*(delta_j0j1-gdj1j0)+(1.-gdi0i0)*(delta_i1j1-gdj1i1)*gdi1j0*gdj0j1+(1.-gdi0i0)*(delta_i1j1-gdj1i1)*gdi1j1*(1.-gdj0j0)+(delta_i0i1-gdi1i0)*gdi0i1*(1.-gdj0j0)*(1.-gdj1j1)+(delta_i0i1-gdi1i0)*gdi0i1*(delta_j0j1-gdj1j0)*gdj0j1-(delta_i0i1-gdi1i0)*gdi0j0*(delta_i1j0-gdj0i1)*(1.-gdj1j1)-(delta_i0i1-gdi1i0)*gdi0j0*(delta_i1j1-gdj1i1)*gdj0j1+(delta_i0i1-gdi1i0)*gdi0j1*(delta_i1j0-gdj0i1)*(delta_j0j1-gdj1j0)-(delta_i0i1-gdi1i0)*gdi0j1*(delta_i1j1-gdj1i1)*(1.-gdj0j0)+(delta_i0j0-gdj0i0)*gdi0i1*gdi1j0*(1.-gdj1j1)-(delta_i0j0-gdj0i0)*gdi0i1*gdi1j1*(delta_j0j1-gdj1j0)+(delta_i0j0-gdj0i0)*gdi0j0*(1.-gdi1i1)*(1.-gdj1j1)+(delta_i0j0-gdj0i0)*gdi0j0*(delta_i1j1-gdj1i1)*gdi1j1-(delta_i0j0-gdj0i0)*gdi0j1*(1.-gdi1i1)*(delta_j0j1-gdj1j0)-(delta_i0j0-gdj0i0)*gdi0j1*(delta_i1j1-gdj1i1)*gdi1j0+(delta_i0j1-gdj1i0)*gdi0i1*gdi1j0*gdj0j1+(delta_i0j1-gdj1i0)*gdi0i1*gdi1j1*(1.-gdj0j0)+(delta_i0j1-gdj1i0)*gdi0j0*(1.-gdi1i1)*gdj0j1-(delta_i0j1-gdj1i0)*gdi0j0*(delta_i1j0-gdj0i1)*gdi1j1+(delta_i0j1-gdj1i0)*gdi0j1*(1.-gdi1i1)*(1.-gdj0j0)+(delta_i0j1-gdj1i0)*gdi0j1*(delta_i1j0-gdj0i1)*gdi1j0;
		m->nem_nnnn[bb] += pre*(uuuu + uuud + uudu + uudd
				      + uduu + udud + uddu + uddd
				      + duuu + duud + dudu + dudd
				      + dduu + ddud + dddu + dddd);
		m->nem_ssss[bb] += pre*(uuuu - uuud - uudu + uudd
				      - uduu + udud + uddu - uddd
				      - duuu + duud + dudu - dudd
				      + dduu - ddud - dddu + dddd);
	}
	}

	// no delta functions here.
	if (meas_bond_corr)
	#pragma omp parallel for
	for (int t = 1; t < L; t++) {
		const num *const restrict Gu0t_t = Gu + N*N*(0+L*t);
		const num *const restrict Gutt_t = Gu + N*N*(t+L*t);
		const num *const restrict Gut0_t = Gu + N*N*(t+L*0);
		const num *const restrict Gd0t_t = Gd + N*N*(0+L*t);
		const num *const restrict Gdtt_t = Gd + N*N*(t+L*t);
		const num *const restrict Gdt0_t = Gd + N*N*(t+L*0);
	for (int c = 0; c < num_b; c++) {
		const int j0 = p->bonds[c];
		const int j1 = p->bonds[c + num_b];
#ifdef USE_PEIERLS
		const num puj0j1 = p->peierlsu[j0 + N*j1];
		const num puj1j0 = p->peierlsu[j1 + N*j0];
		const num pdj0j1 = p->peierlsd[j0 + N*j1];
		const num pdj1j0 = p->peierlsd[j1 + N*j0];
#endif
	for (int b = 0; b < num_b; b++) {
		const int i0 = p->bonds[b];
		const int i1 = p->bonds[b + num_b];
#ifdef USE_PEIERLS
		const num pui0i1 = p->peierlsu[i0 + N*i1];
		const num pui1i0 = p->peierlsu[i1 + N*i0];
		const num pdi0i1 = p->peierlsd[i0 + N*i1];
		const num pdi1i0 = p->peierlsd[i1 + N*i0];
#endif
		const int bb = p->map_bb[b + c*num_b];
		const num pre = phase / p->degen_bb[bb];
		const num gui0i0 = Gutt_t[i0 + i0*N];
		const num gui1i0 = Gutt_t[i1 + i0*N];
		const num gui0i1 = Gutt_t[i0 + i1*N];
		const num gui1i1 = Gutt_t[i1 + i1*N];
		const num gui0j0 = Gut0_t[i0 + j0*N];
		const num gui1j0 = Gut0_t[i1 + j0*N];
		const num gui0j1 = Gut0_t[i0 + j1*N];
		const num gui1j1 = Gut0_t[i1 + j1*N];
		const num guj0i0 = Gu0t_t[j0 + i0*N];
		const num guj1i0 = Gu0t_t[j1 + i0*N];
		const num guj0i1 = Gu0t_t[j0 + i1*N];
		const num guj1i1 = Gu0t_t[j1 + i1*N];
		const num guj0j0 = Gu00[j0 + j0*N];
		const num guj1j0 = Gu00[j1 + j0*N];
		const num guj0j1 = Gu00[j0 + j1*N];
		const num guj1j1 = Gu00[j1 + j1*N];
		const num gdi0i0 = Gdtt_t[i0 + i0*N];
		const num gdi1i0 = Gdtt_t[i1 + i0*N];
		const num gdi0i1 = Gdtt_t[i0 + i1*N];
		const num gdi1i1 = Gdtt_t[i1 + i1*N];
		const num gdi0j0 = Gdt0_t[i0 + j0*N];
		const num gdi1j0 = Gdt0_t[i1 + j0*N];
		const num gdi0j1 = Gdt0_t[i0 + j1*N];
		const num gdi1j1 = Gdt0_t[i1 + j1*N];
		const num gdj0i0 = Gd0t_t[j0 + i0*N];
		const num gdj1i0 = Gd0t_t[j1 + i0*N];
		const num gdj0i1 = Gd0t_t[j0 + i1*N];
		const num gdj1i1 = Gd0t_t[j1 + i1*N];
		const num gdj0j0 = Gd00[j0 + j0*N];
		const num gdj1j0 = Gd00[j1 + j0*N];
		const num gdj0j1 = Gd00[j0 + j1*N];
		const num gdj1j1 = Gd00[j1 + j1*N];
		m->pair_bb[bb + num_bb*t] += 0.5*pre*(gui0j0*gdi1j1 + gui1j0*gdi0j1 + gui0j1*gdi1j0 + gui1j1*gdi0j0);
		const num x = -pui0i1*puj0j1*guj1i0*gui1j0 - pui1i0*puj1j0*guj0i1*gui0j1
		             - pdi0i1*pdj0j1*gdj1i0*gdi1j0 - pdi1i0*pdj1j0*gdj0i1*gdi0j1;
		const num y = -pui0i1*puj1j0*guj0i0*gui1j1 - pui1i0*puj0j1*guj1i1*gui0j0
		             - pdi0i1*pdj1j0*gdj0i0*gdi1j1 - pdi1i0*pdj0j1*gdj1i1*gdi0j0;
		m->jj[bb + num_bb*t]   += pre*((pui1i0*gui0i1 - pui0i1*gui1i0 + pdi1i0*gdi0i1 - pdi0i1*gdi1i0)
		                              *(puj1j0*guj0j1 - puj0j1*guj1j0 + pdj1j0*gdj0j1 - pdj0j1*gdj1j0) + x - y);
		m->jsjs[bb + num_bb*t] += pre*((pui1i0*gui0i1 - pui0i1*gui1i0 - pdi1i0*gdi0i1 + pdi0i1*gdi1i0)
		                              *(puj1j0*guj0j1 - puj0j1*guj1j0 - pdj1j0*gdj0j1 + pdj0j1*gdj1j0) + x - y);
		m->kk[bb + num_bb*t]   += pre*((pui1i0*gui0i1 + pui0i1*gui1i0 + pdi1i0*gdi0i1 + pdi0i1*gdi1i0)
		                              *(puj1j0*guj0j1 + puj0j1*guj1j0 + pdj1j0*gdj0j1 + pdj0j1*gdj1j0) + x + y);
		m->ksks[bb + num_bb*t] += pre*((pui1i0*gui0i1 + pui0i1*gui1i0 - pdi1i0*gdi0i1 - pdi0i1*gdi1i0)
		                              *(puj1j0*guj0j1 + puj0j1*guj1j0 - pdj1j0*gdj0j1 - pdj0j1*gdj1j0) + x + y);
	}
	}
	}

        if (meas_3curr)
	#pragma omp parallel for
	for (int t = 0; t < L; t++) {
		num factor1 = 1.0;
		num factor2 = 1.0;
                num factor3 = 1.0;
                const int delta_t = (t == 0);
		const num *const restrict Gu0t_t = Gu + N*N*(0+L*t);
		const num *const restrict Gutt_t = Gu + N*N*(t+L*t);
		const num *const restrict Gut0_t = Gu + N*N*(t+L*0);
		const num *const restrict Gd0t_t = Gd + N*N*(0+L*t);
		const num *const restrict Gdtt_t = Gd + N*N*(t+L*t);
		const num *const restrict Gdt0_t = Gd + N*N*(t+L*0);
        for (int dt = 0; (t+dt) < L; dt++) {
		const num *const restrict Gu0t_dt = Gu + N*N*(t+L*(dt+t));
		const num *const restrict Gutt_dt = Gu + N*N*(dt+t+L*(dt+t));
		const num *const restrict Gut0_dt = Gu + N*N*(dt+t+L*t);
		const num *const restrict Gd0t_dt = Gd + N*N*(t+L*(dt+t));
		const num *const restrict Gdtt_dt = Gd + N*N*(dt+t+L*(dt+t));
		const num *const restrict Gdt0_dt = Gd + N*N*(dt+t+L*t);
                const int delta_dt = (dt == 0);
                const num *const restrict Gu0t_tdt = Gu + N*N*(0+L*(t+dt));
		const num *const restrict Gutt_tdt = Gu + N*N*(t+dt+L*(t+dt));
		const num *const restrict Gut0_tdt = Gu + N*N*(t+dt+L*0);
		const num *const restrict Gd0t_tdt = Gd + N*N*(0+L*(t+dt));
		const num *const restrict Gdtt_tdt = Gd + N*N*(t+dt+L*(t+dt));
		const num *const restrict Gdt0_tdt = Gd + N*N*(t+dt+L*0);
                const int delta_tdt = delta_t*delta_dt;
	for (int c = 0; c < num_b; c++) {
		const int j0 = p->bonds[c];
		const int j1 = p->bonds[c + num_b];
	for (int b1 = 0; b1 < num_b; b1++) {
		const int i0 = p->bonds[b1];
		const int i1 = p->bonds[b1 + num_b];
        for (int b2 = 0; b2 < num_b; b2++) {
		const int k0 = p->bonds[b2];
		const int k1 = p->bonds[b2 + num_b];
                
		const int bbb1 = p->map_bbb[b2 + b1*num_b + c*num_b*num_b];
                const int bbb2 = p->map_bbb[b1 + b2*num_b + c*num_b*num_b];
		const int bbb3 = p->map_bbb[b2 + c*num_b + b1*num_b*num_b];
		const num pre1 = phase / p->degen_bbb[bbb1];
                const num pre2 = phase / p->degen_bbb[bbb2];
		const num pre3 = phase / p->degen_bbb[bbb3];
		const int delta_i0k0 = (i0 == k0)*delta_dt;
		const int delta_i1k0 = (i1 == k0)*delta_dt;
		const int delta_i0k1 = (i0 == k1)*delta_dt;
		const int delta_i1k1 = (i1 == k1)*delta_dt;
		
                const int delta_i0j0 = (j0 == i0)*delta_t;
		const int delta_i0j1 = (j1 == i0)*delta_t;
		const int delta_i1j0 = (j0 == i1)*delta_t;
		const int delta_i1j1 = (j1 == i1)*delta_t;
                
                const int delta_j0k0 = (k0 == j0)*delta_tdt;
		const int delta_j0k1 = (k1 == j0)*delta_tdt;
		const int delta_j1k0 = (k0 == j1)*delta_tdt;
		const int delta_j1k1 = (k1 == j1)*delta_tdt;
                
		const num gui0i0 = Gutt_t[i0 + i0*N];
		const num gui1i0 = Gutt_t[i1 + i0*N];
		const num gui0i1 = Gutt_t[i0 + i1*N];
		const num gui1i1 = Gutt_t[i1 + i1*N];
		const num gui0j0 = Gut0_t[i0 + j0*N];
		const num gui1j0 = Gut0_t[i1 + j0*N];
		const num gui0j1 = Gut0_t[i0 + j1*N];
		const num gui1j1 = Gut0_t[i1 + j1*N];
		const num guj0i0 = Gu0t_t[j0 + i0*N];
		const num guj1i0 = Gu0t_t[j1 + i0*N];
		const num guj0i1 = Gu0t_t[j0 + i1*N];
		const num guj1i1 = Gu0t_t[j1 + i1*N];
		const num guj0j0 = Gu00[j0 + j0*N];
		const num guj1j0 = Gu00[j1 + j0*N];
		const num guj0j1 = Gu00[j0 + j1*N];
		const num guj1j1 = Gu00[j1 + j1*N];
		const num gdi0i0 = Gdtt_t[i0 + i0*N];
		const num gdi1i0 = Gdtt_t[i1 + i0*N];
		const num gdi0i1 = Gdtt_t[i0 + i1*N];
		const num gdi1i1 = Gdtt_t[i1 + i1*N];
		const num gdi0j0 = Gdt0_t[i0 + j0*N];
		const num gdi1j0 = Gdt0_t[i1 + j0*N];
		const num gdi0j1 = Gdt0_t[i0 + j1*N];
		const num gdi1j1 = Gdt0_t[i1 + j1*N];
		const num gdj0i0 = Gd0t_t[j0 + i0*N];
		const num gdj1i0 = Gd0t_t[j1 + i0*N];
		const num gdj0i1 = Gd0t_t[j0 + i1*N];
		const num gdj1i1 = Gd0t_t[j1 + i1*N];
		const num gdj0j0 = Gd00[j0 + j0*N];
		const num gdj1j0 = Gd00[j1 + j0*N];
		const num gdj0j1 = Gd00[j0 + j1*N];
		const num gdj1j1 = Gd00[j1 + j1*N];
                                           
                const num guk0k0 = Gutt_tdt[k0 + k0*N];
		const num guk1k0 = Gutt_tdt[k1 + k0*N];
		const num guk0k1 = Gutt_tdt[k0 + k1*N];
		const num guk1k1 = Gutt_tdt[k1 + k1*N];
		const num guk0j0 = Gut0_tdt[k0 + j0*N];
		const num guk1j0 = Gut0_tdt[k1 + j0*N];
		const num guk0j1 = Gut0_tdt[k0 + j1*N];
		const num guk1j1 = Gut0_tdt[k1 + j1*N];
		const num guj0k0 = Gu0t_tdt[j0 + k0*N];
		const num guj1k0 = Gu0t_tdt[j1 + k0*N];
		const num guj0k1 = Gu0t_tdt[j0 + k1*N];
		const num guj1k1 = Gu0t_tdt[j1 + k1*N];
		const num gdk0k0 = Gdtt_tdt[k0 + k0*N];
		const num gdk1k0 = Gdtt_tdt[k1 + k0*N];
		const num gdk0k1 = Gdtt_tdt[k0 + k1*N];
		const num gdk1k1 = Gdtt_tdt[k1 + k1*N];
		const num gdk0j0 = Gdt0_tdt[k0 + j0*N];
		const num gdk1j0 = Gdt0_tdt[k1 + j0*N];
		const num gdk0j1 = Gdt0_tdt[k0 + j1*N];
		const num gdk1j1 = Gdt0_tdt[k1 + j1*N];
		const num gdj0k0 = Gd0t_tdt[j0 + k0*N];
		const num gdj1k0 = Gd0t_tdt[j1 + k0*N];
		const num gdj0k1 = Gd0t_tdt[j0 + k1*N];
		const num gdj1k1 = Gd0t_tdt[j1 + k1*N];

		const num guk0i0 = Gut0_dt[k0 + i0*N];
		const num guk1i0 = Gut0_dt[k1 + i0*N];
		const num guk0i1 = Gut0_dt[k0 + i1*N];
		const num guk1i1 = Gut0_dt[k1 + i1*N];
		const num gui0k0 = Gu0t_dt[i0 + k0*N];
		const num gui1k0 = Gu0t_dt[i1 + k0*N];
		const num gui0k1 = Gu0t_dt[i0 + k1*N];
		const num gui1k1 = Gu0t_dt[i1 + k1*N];
		const num gdk0i0 = Gdt0_dt[k0 + i0*N];
		const num gdk1i0 = Gdt0_dt[k1 + i0*N];
		const num gdk0i1 = Gdt0_dt[k0 + i1*N];
		const num gdk1i1 = Gdt0_dt[k1 + i1*N];
		const num gdi0k0 = Gd0t_dt[i0 + k0*N];
		const num gdi1k0 = Gd0t_dt[i1 + k0*N];
		const num gdi0k1 = Gd0t_dt[i0 + k1*N];
		const num gdi1k1 = Gd0t_dt[i1 + k1*N];

		const num jku =  (guk0k1-guk1k0);
                const num jkd =  (gdk0k1-gdk1k0);
                const num jiu =  (gui0i1-gui1i0);
                const num jid =  (gdi0i1-gdi1i0);
                const num jju =  (guj0j1-guj1j0);
                const num jjd =  (gdj0j1-gdj1j0);

                const num jjju = jku*jiu*jju;
                const num jjjd = jkd*jid*jjd;

                const num ij_u = (gui1i0-gui0i1)*(guj1j0-guj0j1) + (delta_i0j1-guj1i0)*gui1j0 - (delta_i0j0-guj0i0)*gui1j1 + (delta_i1j0-guj0i1)*gui0j1 - (delta_i1j1-guj1i1)*gui0j0;
                const num ij_d = (gdi1i0-gdi0i1)*(gdj1j0-gdj0j1) + (delta_i0j1-gdj1i0)*gdi1j0 - (delta_i0j0-gdj0i0)*gdi1j1 + (delta_i1j0-gdj0i1)*gdi0j1 - (delta_i1j1-gdj1i1)*gdi0j0;
                const num kj_u = (guk1k0-guk0k1)*(guj1j0-guj0j1) + (delta_j1k0-guj1k0)*guk1j0 - (delta_j0k0-guj0k0)*guk1j1 + (delta_j0k1-guj0k1)*guk0j1 - (delta_j1k1-guj1k1)*guk0j0;
                const num kj_d = (gdk1k0-gdk0k1)*(gdj1j0-gdj0j1) + (delta_j1k0-gdj1k0)*gdk1j0 - (delta_j0k0-gdj0k0)*gdk1j1 + (delta_j0k1-gdj0k1)*gdk0j1 - (delta_j1k1-gdj1k1)*gdk0j0;
                const num ki_u = (guk1k0-guk0k1)*(gui1i0-gui0i1) + (delta_i1k0-gui1k0)*guk1i0 - (delta_i0k0-gui0k0)*guk1i1 + (delta_i0k1-gui0k1)*guk0i1 - (delta_i1k1-gui1k1)*guk0i0;
                const num ki_d = (gdk1k0-gdk0k1)*(gdi1i0-gdi0i1) + (delta_i1k0-gdi1k0)*gdk1i0 - (delta_i0k0-gdi0k0)*gdk1i1 + (delta_i0k1-gdi0k1)*gdk0i1 - (delta_i1k1-gdi1k1)*gdk0i0;
                const num part1 = (jku+jkd)*(ij_u+ij_d)+(jiu+jid)*(kj_u+kj_d)+(jju+jjd)*(ki_u+ki_d);

                const num part2u = (delta_i1k0-gui1k0)*guk1j0*(delta_i0j1-guj1i0) - (delta_i1k1-gui1k1)*guk0j0*(delta_i0j1-guj1i0) 
                                  -(delta_i0k0-gui0k0)*guk1j0*(delta_i1j1-guj1i1) - (delta_i1k0-gui1k0)*guk1j1*(delta_i0j0-guj0i0) 
                                  +(delta_i0k1-gui0k1)*guk0j0*(delta_i1j1-guj1i1) + (delta_i1k1-gui1k1)*guk0j1*(delta_i0j0-guj0i0) 
                                  +(delta_i0k0-gui0k0)*guk1j1*(delta_i1j0-guj0i1) - (delta_i0k1-gui0k1)*guk0j1*(delta_i1j0-guj0i1);

                const num part2d = (delta_i1k0-gdi1k0)*gdk1j0*(delta_i0j1-gdj1i0) - (delta_i1k1-gdi1k1)*gdk0j0*(delta_i0j1-gdj1i0) 
                                  -(delta_i0k0-gdi0k0)*gdk1j0*(delta_i1j1-gdj1i1) - (delta_i1k0-gdi1k0)*gdk1j1*(delta_i0j0-gdj0i0) 
                                  +(delta_i0k1-gdi0k1)*gdk0j0*(delta_i1j1-gdj1i1) + (delta_i1k1-gdi1k1)*gdk0j1*(delta_i0j0-gdj0i0) 
                                  +(delta_i0k0-gdi0k0)*gdk1j1*(delta_i1j0-gdj0i1) - (delta_i0k1-gdi0k1)*gdk0j1*(delta_i1j0-gdj0i1);

                const num part3u = guk1i0*gui1j0*(delta_j1k0-guj1k0) - guk0i0*gui1j0*(delta_j1k1-guj1k1) - guk1i1*gui0j0*(delta_j1k0-guj1k0) 
                                 - guk1i0*gui1j1*(delta_j0k0-guj0k0) + guk0i1*gui0j0*(delta_j1k1-guj1k1) + guk0i0*gui1j1*(delta_j0k1-guj0k1) 
                                 + guk1i1*gui0j1*(delta_j0k0-guj0k0) - guk0i1*gui0j1*(delta_j0k1-guj0k1);

                const num part3d = gdk1i0*gdi1j0*(delta_j1k0-gdj1k0) - gdk0i0*gdi1j0*(delta_j1k1-gdj1k1) - gdk1i1*gdi0j0*(delta_j1k0-gdj1k0) 
                                 - gdk1i0*gdi1j1*(delta_j0k0-gdj0k0) + gdk0i1*gdi0j0*(delta_j1k1-gdj1k1) + gdk0i0*gdi1j1*(delta_j0k1-gdj0k1) 
                                 + gdk1i1*gdi0j1*(delta_j0k0-gdj0k0) - gdk0i1*gdi0j1*(delta_j0k1-gdj0k1);
		const num meas=-2*jjju -2*jjjd +part1 -part2u -part2d +part3u +part3d;
                factor1 = p->integral_kernel[(t+dt)*(L+2) + t]; 
                factor2 = p->integral_kernel[     t*(L+2) + (t+1+dt)]; 
                factor3 = p->integral_kernel[(t+dt)*(L+2) + (L+1)];
                //printf("%f\t%f\t%f\t%f\t%f\t%f\n", pre1, pre2, pre3, meas, factor1, factor2);
                m->jjj[bbb1 + num_bbb*(t+dt)] += pre1*meas*factor1;
                m->jjj[bbb2 + num_bbb*t] += pre2*meas*factor2;
                if (t==0)
                m->jjj[bbb3 + num_bbb*(t+dt)] += pre3*meas*factor3;
        }
        }
	}
	}
        }
        
        // This is only for none-tp case.
        // If consider tp and want to improve the speed by removing some measurements, please use a mark matrix to rule out the specific measurements.
        // It is also a good idea to just use the original 3-current measurments since the "unnecessary" measurements may be used in the future.
        if (meas_3curr_limit)
	#pragma omp parallel for
	for (int t = 0; t < L; t++) {
                num factor1 = 1.0;
		num factor2 = 1.0;
                num factor3 = 1.0;
                const int delta_t = (t == 0);
		const num *const restrict Gu0t_t = Gu + N*N*(0+L*t);
		const num *const restrict Gutt_t = Gu + N*N*(t+L*t);
		const num *const restrict Gut0_t = Gu + N*N*(t+L*0);
		const num *const restrict Gd0t_t = Gd + N*N*(0+L*t);
		const num *const restrict Gdtt_t = Gd + N*N*(t+L*t);
		const num *const restrict Gdt0_t = Gd + N*N*(t+L*0);
        for (int dt = 0; (t+dt) < L; dt++) {
		const num *const restrict Gu0t_dt = Gu + N*N*(t+L*(dt+t));
		const num *const restrict Gutt_dt = Gu + N*N*(dt+t+L*(dt+t));
		const num *const restrict Gut0_dt = Gu + N*N*(dt+t+L*t);
		const num *const restrict Gd0t_dt = Gd + N*N*(t+L*(dt+t));
		const num *const restrict Gdtt_dt = Gd + N*N*(dt+t+L*(dt+t));
		const num *const restrict Gdt0_dt = Gd + N*N*(dt+t+L*t);
                const int delta_dt = (dt == 0);
                const num *const restrict Gu0t_tdt = Gu + N*N*(0+L*(t+dt));
		const num *const restrict Gutt_tdt = Gu + N*N*(t+dt+L*(t+dt));
		const num *const restrict Gut0_tdt = Gu + N*N*(t+dt+L*0);
		const num *const restrict Gd0t_tdt = Gd + N*N*(0+L*(t+dt));
		const num *const restrict Gdtt_tdt = Gd + N*N*(t+dt+L*(t+dt));
		const num *const restrict Gdt0_tdt = Gd + N*N*(t+dt+L*0);
                const int delta_tdt = delta_t*delta_dt;
	for (int c = 0; c < num_b; c++) {
		const int j0 = p->bonds[c];
		const int j1 = p->bonds[c + num_b];
	for (int b1 = 0; b1 < num_b; b1++) {
		const int i0 = p->bonds[b1];
		const int i1 = p->bonds[b1 + num_b];
        for (int b2 = 0; b2 < num_b; b2++) {
		const int k0 = p->bonds[b2];
		const int k1 = p->bonds[b2 + num_b];
                
		const int bbb1 = p->map_bbb_lim[b2 + b1*num_b + c*num_b*num_b];
                const int bbb2 = p->map_bbb_lim[b1 + b2*num_b + c*num_b*num_b];
		const int bbb3 = p->map_bbb_lim[b2 + c*num_b + b1*num_b*num_b];
                // If none of the bondbondbond types is of interest, don't measure
                if ((bbb1 == -1) && (bbb2 == -1) && (bbb3 == -1)){
                    continue;}
                // This condition is because bbb3 is only for the internal integral at tau'=beta
                // even if the bond bbb3 is of interest, we don't measure the unnecessary time difference types of correlations
                // we only want t==0 for bbb3 != -1
                if ((bbb1 == -1) && (bbb2 == -1) && (bbb3 != -1) && (t!=0)){
                    continue;}
                
                num pre1=0.0;
                num pre2=0.0;
                num pre3=0.0; 
		if (bbb1 != -1){
                    pre1 = phase / p->degen_bbb_lim[bbb1];}
                if (bbb2 != -1){
                    pre2 = phase / p->degen_bbb_lim[bbb2];}
                if (bbb3 != -1){
		    pre3 = phase / p->degen_bbb_lim[bbb3];}
                
		const int delta_i0k0 = (i0 == k0)*delta_dt;
		const int delta_i1k0 = (i1 == k0)*delta_dt;
		const int delta_i0k1 = (i0 == k1)*delta_dt;
		const int delta_i1k1 = (i1 == k1)*delta_dt;
		
                const int delta_i0j0 = (j0 == i0)*delta_t;
		const int delta_i0j1 = (j1 == i0)*delta_t;
		const int delta_i1j0 = (j0 == i1)*delta_t;
		const int delta_i1j1 = (j1 == i1)*delta_t;
                
                const int delta_j0k0 = (k0 == j0)*delta_tdt;
		const int delta_j0k1 = (k1 == j0)*delta_tdt;
		const int delta_j1k0 = (k0 == j1)*delta_tdt;
		const int delta_j1k1 = (k1 == j1)*delta_tdt;
                
		const num gui0i0 = Gutt_t[i0 + i0*N];
		const num gui1i0 = Gutt_t[i1 + i0*N];
		const num gui0i1 = Gutt_t[i0 + i1*N];
		const num gui1i1 = Gutt_t[i1 + i1*N];
		const num gui0j0 = Gut0_t[i0 + j0*N];
		const num gui1j0 = Gut0_t[i1 + j0*N];
		const num gui0j1 = Gut0_t[i0 + j1*N];
		const num gui1j1 = Gut0_t[i1 + j1*N];
		const num guj0i0 = Gu0t_t[j0 + i0*N];
		const num guj1i0 = Gu0t_t[j1 + i0*N];
		const num guj0i1 = Gu0t_t[j0 + i1*N];
		const num guj1i1 = Gu0t_t[j1 + i1*N];
		const num guj0j0 = Gu00[j0 + j0*N];
		const num guj1j0 = Gu00[j1 + j0*N];
		const num guj0j1 = Gu00[j0 + j1*N];
		const num guj1j1 = Gu00[j1 + j1*N];
		const num gdi0i0 = Gdtt_t[i0 + i0*N];
		const num gdi1i0 = Gdtt_t[i1 + i0*N];
		const num gdi0i1 = Gdtt_t[i0 + i1*N];
		const num gdi1i1 = Gdtt_t[i1 + i1*N];
		const num gdi0j0 = Gdt0_t[i0 + j0*N];
		const num gdi1j0 = Gdt0_t[i1 + j0*N];
		const num gdi0j1 = Gdt0_t[i0 + j1*N];
		const num gdi1j1 = Gdt0_t[i1 + j1*N];
		const num gdj0i0 = Gd0t_t[j0 + i0*N];
		const num gdj1i0 = Gd0t_t[j1 + i0*N];
		const num gdj0i1 = Gd0t_t[j0 + i1*N];
		const num gdj1i1 = Gd0t_t[j1 + i1*N];
		const num gdj0j0 = Gd00[j0 + j0*N];
		const num gdj1j0 = Gd00[j1 + j0*N];
		const num gdj0j1 = Gd00[j0 + j1*N];
		const num gdj1j1 = Gd00[j1 + j1*N];
                                           
                const num guk0k0 = Gutt_tdt[k0 + k0*N];
		const num guk1k0 = Gutt_tdt[k1 + k0*N];
		const num guk0k1 = Gutt_tdt[k0 + k1*N];
		const num guk1k1 = Gutt_tdt[k1 + k1*N];
		const num guk0j0 = Gut0_tdt[k0 + j0*N];
		const num guk1j0 = Gut0_tdt[k1 + j0*N];
		const num guk0j1 = Gut0_tdt[k0 + j1*N];
		const num guk1j1 = Gut0_tdt[k1 + j1*N];
		const num guj0k0 = Gu0t_tdt[j0 + k0*N];
		const num guj1k0 = Gu0t_tdt[j1 + k0*N];
		const num guj0k1 = Gu0t_tdt[j0 + k1*N];
		const num guj1k1 = Gu0t_tdt[j1 + k1*N];
		const num gdk0k0 = Gdtt_tdt[k0 + k0*N];
		const num gdk1k0 = Gdtt_tdt[k1 + k0*N];
		const num gdk0k1 = Gdtt_tdt[k0 + k1*N];
		const num gdk1k1 = Gdtt_tdt[k1 + k1*N];
		const num gdk0j0 = Gdt0_tdt[k0 + j0*N];
		const num gdk1j0 = Gdt0_tdt[k1 + j0*N];
		const num gdk0j1 = Gdt0_tdt[k0 + j1*N];
		const num gdk1j1 = Gdt0_tdt[k1 + j1*N];
		const num gdj0k0 = Gd0t_tdt[j0 + k0*N];
		const num gdj1k0 = Gd0t_tdt[j1 + k0*N];
		const num gdj0k1 = Gd0t_tdt[j0 + k1*N];
		const num gdj1k1 = Gd0t_tdt[j1 + k1*N];

		const num guk0i0 = Gut0_dt[k0 + i0*N];
		const num guk1i0 = Gut0_dt[k1 + i0*N];
		const num guk0i1 = Gut0_dt[k0 + i1*N];
		const num guk1i1 = Gut0_dt[k1 + i1*N];
		const num gui0k0 = Gu0t_dt[i0 + k0*N];
		const num gui1k0 = Gu0t_dt[i1 + k0*N];
		const num gui0k1 = Gu0t_dt[i0 + k1*N];
		const num gui1k1 = Gu0t_dt[i1 + k1*N];
		const num gdk0i0 = Gdt0_dt[k0 + i0*N];
		const num gdk1i0 = Gdt0_dt[k1 + i0*N];
		const num gdk0i1 = Gdt0_dt[k0 + i1*N];
		const num gdk1i1 = Gdt0_dt[k1 + i1*N];
		const num gdi0k0 = Gd0t_dt[i0 + k0*N];
		const num gdi1k0 = Gd0t_dt[i1 + k0*N];
		const num gdi0k1 = Gd0t_dt[i0 + k1*N];
		const num gdi1k1 = Gd0t_dt[i1 + k1*N];

                const num jku =  (guk0k1-guk1k0);
                const num jkd =  (gdk0k1-gdk1k0);
                const num jiu =  (gui0i1-gui1i0);
                const num jid =  (gdi0i1-gdi1i0);
                const num jju =  (guj0j1-guj1j0);
                const num jjd =  (gdj0j1-gdj1j0);

                const num jjju = jku*jiu*jju;
                const num jjjd = jkd*jid*jjd;

                const num ij_u = (gui1i0-gui0i1)*(guj1j0-guj0j1) + (delta_i0j1-guj1i0)*gui1j0 - (delta_i0j0-guj0i0)*gui1j1 + (delta_i1j0-guj0i1)*gui0j1 - (delta_i1j1-guj1i1)*gui0j0;
                const num ij_d = (gdi1i0-gdi0i1)*(gdj1j0-gdj0j1) + (delta_i0j1-gdj1i0)*gdi1j0 - (delta_i0j0-gdj0i0)*gdi1j1 + (delta_i1j0-gdj0i1)*gdi0j1 - (delta_i1j1-gdj1i1)*gdi0j0;
                const num kj_u = (guk1k0-guk0k1)*(guj1j0-guj0j1) + (delta_j1k0-guj1k0)*guk1j0 - (delta_j0k0-guj0k0)*guk1j1 + (delta_j0k1-guj0k1)*guk0j1 - (delta_j1k1-guj1k1)*guk0j0;
                const num kj_d = (gdk1k0-gdk0k1)*(gdj1j0-gdj0j1) + (delta_j1k0-gdj1k0)*gdk1j0 - (delta_j0k0-gdj0k0)*gdk1j1 + (delta_j0k1-gdj0k1)*gdk0j1 - (delta_j1k1-gdj1k1)*gdk0j0;
                const num ki_u = (guk1k0-guk0k1)*(gui1i0-gui0i1) + (delta_i1k0-gui1k0)*guk1i0 - (delta_i0k0-gui0k0)*guk1i1 + (delta_i0k1-gui0k1)*guk0i1 - (delta_i1k1-gui1k1)*guk0i0;
                const num ki_d = (gdk1k0-gdk0k1)*(gdi1i0-gdi0i1) + (delta_i1k0-gdi1k0)*gdk1i0 - (delta_i0k0-gdi0k0)*gdk1i1 + (delta_i0k1-gdi0k1)*gdk0i1 - (delta_i1k1-gdi1k1)*gdk0i0;
                const num part1 = (jku+jkd)*(ij_u+ij_d)+(jiu+jid)*(kj_u+kj_d)+(jju+jjd)*(ki_u+ki_d);

                const num part2u = (delta_i1k0-gui1k0)*guk1j0*(delta_i0j1-guj1i0) - (delta_i1k1-gui1k1)*guk0j0*(delta_i0j1-guj1i0) 
                                  -(delta_i0k0-gui0k0)*guk1j0*(delta_i1j1-guj1i1) - (delta_i1k0-gui1k0)*guk1j1*(delta_i0j0-guj0i0) 
                                  +(delta_i0k1-gui0k1)*guk0j0*(delta_i1j1-guj1i1) + (delta_i1k1-gui1k1)*guk0j1*(delta_i0j0-guj0i0) 
                                  +(delta_i0k0-gui0k0)*guk1j1*(delta_i1j0-guj0i1) - (delta_i0k1-gui0k1)*guk0j1*(delta_i1j0-guj0i1);

                const num part2d = (delta_i1k0-gdi1k0)*gdk1j0*(delta_i0j1-gdj1i0) - (delta_i1k1-gdi1k1)*gdk0j0*(delta_i0j1-gdj1i0) 
                                  -(delta_i0k0-gdi0k0)*gdk1j0*(delta_i1j1-gdj1i1) - (delta_i1k0-gdi1k0)*gdk1j1*(delta_i0j0-gdj0i0) 
                                  +(delta_i0k1-gdi0k1)*gdk0j0*(delta_i1j1-gdj1i1) + (delta_i1k1-gdi1k1)*gdk0j1*(delta_i0j0-gdj0i0) 
                                  +(delta_i0k0-gdi0k0)*gdk1j1*(delta_i1j0-gdj0i1) - (delta_i0k1-gdi0k1)*gdk0j1*(delta_i1j0-gdj0i1);

                const num part3u = guk1i0*gui1j0*(delta_j1k0-guj1k0) - guk0i0*gui1j0*(delta_j1k1-guj1k1) - guk1i1*gui0j0*(delta_j1k0-guj1k0) 
                                 - guk1i0*gui1j1*(delta_j0k0-guj0k0) + guk0i1*gui0j0*(delta_j1k1-guj1k1) + guk0i0*gui1j1*(delta_j0k1-guj0k1) 
                                 + guk1i1*gui0j1*(delta_j0k0-guj0k0) - guk0i1*gui0j1*(delta_j0k1-guj0k1);

                const num part3d = gdk1i0*gdi1j0*(delta_j1k0-gdj1k0) - gdk0i0*gdi1j0*(delta_j1k1-gdj1k1) - gdk1i1*gdi0j0*(delta_j1k0-gdj1k0) 
                                 - gdk1i0*gdi1j1*(delta_j0k0-gdj0k0) + gdk0i1*gdi0j0*(delta_j1k1-gdj1k1) + gdk0i0*gdi1j1*(delta_j0k1-gdj0k1) 
                                 + gdk1i1*gdi0j1*(delta_j0k0-gdj0k0) - gdk0i1*gdi0j1*(delta_j0k1-gdj0k1);
		const num meas=-2*jjju -2*jjjd +part1 -part2u -part2d +part3u +part3d;
                factor1 = p->integral_kernel[(t+dt)*(L+2) + t]; 
                factor2 = p->integral_kernel[     t*(L+2) + (t+1+dt)]; 
                factor3 = p->integral_kernel[(t+dt)*(L+2) + (L+1)];
                // printf("%f\t%f\t%f\t%f\t%f\t%f\t%f\n", pre1, pre2, pre3, factor1, factor2, factor3);
                if (bbb1 != -1){
                    m->jjj_l[bbb1 + num_bbb_lim*(t+dt)] += pre1*meas*factor1;}
                if (bbb2 != -1){
                    m->jjj_l[bbb2 + num_bbb_lim*t] += pre2*meas*factor2;}
                if ( (t==0) && (bbb3 != -1) ){
                    m->jjj_l[bbb3 + num_bbb_lim*(t+dt)] += pre3*meas*factor3;}
        }
        }
	}
	}
        }




	if (meas_nematic_corr)
	#pragma omp parallel for
	for (int t = 1; t < L; t++) {
		const num *const restrict Gu0t_t = Gu + N*N*(0+L*t);
		const num *const restrict Gutt_t = Gu + N*N*(t+L*t);
		const num *const restrict Gut0_t = Gu + N*N*(t+L*0);
		const num *const restrict Gd0t_t = Gd + N*N*(0+L*t);
		const num *const restrict Gdtt_t = Gd + N*N*(t+L*t);
		const num *const restrict Gdt0_t = Gd + N*N*(t+L*0);
	for (int c = 0; c < NEM_BONDS*N; c++) {
		const int j0 = p->bonds[c];
		const int j1 = p->bonds[c + num_b];
	for (int b = 0; b < NEM_BONDS*N; b++) {
		const int i0 = p->bonds[b];
		const int i1 = p->bonds[b + num_b];
		const int bb = p->map_bb[b + c*num_b];
		const num pre = phase / p->degen_bb[bb];
		const num gui0i0 = Gutt_t[i0 + i0*N];
		const num gui1i0 = Gutt_t[i1 + i0*N];
		const num gui0i1 = Gutt_t[i0 + i1*N];
		const num gui1i1 = Gutt_t[i1 + i1*N];
		const num gui0j0 = Gut0_t[i0 + j0*N];
		const num gui1j0 = Gut0_t[i1 + j0*N];
		const num gui0j1 = Gut0_t[i0 + j1*N];
		const num gui1j1 = Gut0_t[i1 + j1*N];
		const num guj0i0 = Gu0t_t[j0 + i0*N];
		const num guj1i0 = Gu0t_t[j1 + i0*N];
		const num guj0i1 = Gu0t_t[j0 + i1*N];
		const num guj1i1 = Gu0t_t[j1 + i1*N];
		const num guj0j0 = Gu00[j0 + j0*N];
		const num guj1j0 = Gu00[j1 + j0*N];
		const num guj0j1 = Gu00[j0 + j1*N];
		const num guj1j1 = Gu00[j1 + j1*N];
		const num gdi0i0 = Gdtt_t[i0 + i0*N];
		const num gdi1i0 = Gdtt_t[i1 + i0*N];
		const num gdi0i1 = Gdtt_t[i0 + i1*N];
		const num gdi1i1 = Gdtt_t[i1 + i1*N];
		const num gdi0j0 = Gdt0_t[i0 + j0*N];
		const num gdi1j0 = Gdt0_t[i1 + j0*N];
		const num gdi0j1 = Gdt0_t[i0 + j1*N];
		const num gdi1j1 = Gdt0_t[i1 + j1*N];
		const num gdj0i0 = Gd0t_t[j0 + i0*N];
		const num gdj1i0 = Gd0t_t[j1 + i0*N];
		const num gdj0i1 = Gd0t_t[j0 + i1*N];
		const num gdj1i1 = Gd0t_t[j1 + i1*N];
		const num gdj0j0 = Gd00[j0 + j0*N];
		const num gdj1j0 = Gd00[j1 + j0*N];
		const num gdj0j1 = Gd00[j0 + j1*N];
		const num gdj1j1 = Gd00[j1 + j1*N];
		const int delta_i0i1 = 0;
		const int delta_j0j1 = 0;
		const int delta_i0j0 = 0;
		const int delta_i1j0 = 0;
		const int delta_i0j1 = 0;
		const int delta_i1j1 = 0;
		const num uuuu = +(1.-gui0i0)*(1.-gui1i1)*(1.-guj0j0)*(1.-guj1j1)+(1.-gui0i0)*(1.-gui1i1)*(delta_j0j1-guj1j0)*guj0j1+(1.-gui0i0)*(delta_i1j0-guj0i1)*gui1j0*(1.-guj1j1)-(1.-gui0i0)*(delta_i1j0-guj0i1)*gui1j1*(delta_j0j1-guj1j0)+(1.-gui0i0)*(delta_i1j1-guj1i1)*gui1j0*guj0j1+(1.-gui0i0)*(delta_i1j1-guj1i1)*gui1j1*(1.-guj0j0)+(delta_i0i1-gui1i0)*gui0i1*(1.-guj0j0)*(1.-guj1j1)+(delta_i0i1-gui1i0)*gui0i1*(delta_j0j1-guj1j0)*guj0j1-(delta_i0i1-gui1i0)*gui0j0*(delta_i1j0-guj0i1)*(1.-guj1j1)-(delta_i0i1-gui1i0)*gui0j0*(delta_i1j1-guj1i1)*guj0j1+(delta_i0i1-gui1i0)*gui0j1*(delta_i1j0-guj0i1)*(delta_j0j1-guj1j0)-(delta_i0i1-gui1i0)*gui0j1*(delta_i1j1-guj1i1)*(1.-guj0j0)+(delta_i0j0-guj0i0)*gui0i1*gui1j0*(1.-guj1j1)-(delta_i0j0-guj0i0)*gui0i1*gui1j1*(delta_j0j1-guj1j0)+(delta_i0j0-guj0i0)*gui0j0*(1.-gui1i1)*(1.-guj1j1)+(delta_i0j0-guj0i0)*gui0j0*(delta_i1j1-guj1i1)*gui1j1-(delta_i0j0-guj0i0)*gui0j1*(1.-gui1i1)*(delta_j0j1-guj1j0)-(delta_i0j0-guj0i0)*gui0j1*(delta_i1j1-guj1i1)*gui1j0+(delta_i0j1-guj1i0)*gui0i1*gui1j0*guj0j1+(delta_i0j1-guj1i0)*gui0i1*gui1j1*(1.-guj0j0)+(delta_i0j1-guj1i0)*gui0j0*(1.-gui1i1)*guj0j1-(delta_i0j1-guj1i0)*gui0j0*(delta_i1j0-guj0i1)*gui1j1+(delta_i0j1-guj1i0)*gui0j1*(1.-gui1i1)*(1.-guj0j0)+(delta_i0j1-guj1i0)*gui0j1*(delta_i1j0-guj0i1)*gui1j0;
		const num uuud = +(1.-gui0i0)*(1.-gui1i1)*(1.-guj0j0)*(1.-gdj1j1)+(1.-gui0i0)*(delta_i1j0-guj0i1)*gui1j0*(1.-gdj1j1)+(delta_i0i1-gui1i0)*gui0i1*(1.-guj0j0)*(1.-gdj1j1)-(delta_i0i1-gui1i0)*gui0j0*(delta_i1j0-guj0i1)*(1.-gdj1j1)+(delta_i0j0-guj0i0)*gui0i1*gui1j0*(1.-gdj1j1)+(delta_i0j0-guj0i0)*gui0j0*(1.-gui1i1)*(1.-gdj1j1);
		const num uudu = +(1.-gui0i0)*(1.-gui1i1)*(1.-gdj0j0)*(1.-guj1j1)+(1.-gui0i0)*(delta_i1j1-guj1i1)*gui1j1*(1.-gdj0j0)+(delta_i0i1-gui1i0)*gui0i1*(1.-gdj0j0)*(1.-guj1j1)-(delta_i0i1-gui1i0)*gui0j1*(delta_i1j1-guj1i1)*(1.-gdj0j0)+(delta_i0j1-guj1i0)*gui0i1*gui1j1*(1.-gdj0j0)+(delta_i0j1-guj1i0)*gui0j1*(1.-gui1i1)*(1.-gdj0j0);
		const num uudd = +(1.-gui0i0)*(1.-gui1i1)*(1.-gdj0j0)*(1.-gdj1j1)+(1.-gui0i0)*(1.-gui1i1)*(delta_j0j1-gdj1j0)*gdj0j1+(delta_i0i1-gui1i0)*gui0i1*(1.-gdj0j0)*(1.-gdj1j1)+(delta_i0i1-gui1i0)*gui0i1*(delta_j0j1-gdj1j0)*gdj0j1;
		const num uduu = +(1.-gui0i0)*(1.-gdi1i1)*(1.-guj0j0)*(1.-guj1j1)+(1.-gui0i0)*(1.-gdi1i1)*(delta_j0j1-guj1j0)*guj0j1+(delta_i0j0-guj0i0)*gui0j0*(1.-gdi1i1)*(1.-guj1j1)-(delta_i0j0-guj0i0)*gui0j1*(1.-gdi1i1)*(delta_j0j1-guj1j0)+(delta_i0j1-guj1i0)*gui0j0*(1.-gdi1i1)*guj0j1+(delta_i0j1-guj1i0)*gui0j1*(1.-gdi1i1)*(1.-guj0j0);
		const num udud = +(1.-gui0i0)*(1.-gdi1i1)*(1.-guj0j0)*(1.-gdj1j1)+(1.-gui0i0)*(delta_i1j1-gdj1i1)*gdi1j1*(1.-guj0j0)+(delta_i0j0-guj0i0)*gui0j0*(1.-gdi1i1)*(1.-gdj1j1)+(delta_i0j0-guj0i0)*gui0j0*(delta_i1j1-gdj1i1)*gdi1j1;
		const num uddu = +(1.-gui0i0)*(1.-gdi1i1)*(1.-gdj0j0)*(1.-guj1j1)+(1.-gui0i0)*(delta_i1j0-gdj0i1)*gdi1j0*(1.-guj1j1)+(delta_i0j1-guj1i0)*gui0j1*(1.-gdi1i1)*(1.-gdj0j0)+(delta_i0j1-guj1i0)*gui0j1*(delta_i1j0-gdj0i1)*gdi1j0;
		const num uddd = +(1.-gui0i0)*(1.-gdi1i1)*(1.-gdj0j0)*(1.-gdj1j1)+(1.-gui0i0)*(1.-gdi1i1)*(delta_j0j1-gdj1j0)*gdj0j1+(1.-gui0i0)*(delta_i1j0-gdj0i1)*gdi1j0*(1.-gdj1j1)-(1.-gui0i0)*(delta_i1j0-gdj0i1)*gdi1j1*(delta_j0j1-gdj1j0)+(1.-gui0i0)*(delta_i1j1-gdj1i1)*gdi1j0*gdj0j1+(1.-gui0i0)*(delta_i1j1-gdj1i1)*gdi1j1*(1.-gdj0j0);
		const num duuu = +(1.-gdi0i0)*(1.-gui1i1)*(1.-guj0j0)*(1.-guj1j1)+(1.-gdi0i0)*(1.-gui1i1)*(delta_j0j1-guj1j0)*guj0j1+(1.-gdi0i0)*(delta_i1j0-guj0i1)*gui1j0*(1.-guj1j1)-(1.-gdi0i0)*(delta_i1j0-guj0i1)*gui1j1*(delta_j0j1-guj1j0)+(1.-gdi0i0)*(delta_i1j1-guj1i1)*gui1j0*guj0j1+(1.-gdi0i0)*(delta_i1j1-guj1i1)*gui1j1*(1.-guj0j0);
		const num duud = +(1.-gdi0i0)*(1.-gui1i1)*(1.-guj0j0)*(1.-gdj1j1)+(1.-gdi0i0)*(delta_i1j0-guj0i1)*gui1j0*(1.-gdj1j1)+(delta_i0j1-gdj1i0)*gdi0j1*(1.-gui1i1)*(1.-guj0j0)+(delta_i0j1-gdj1i0)*gdi0j1*(delta_i1j0-guj0i1)*gui1j0;
		const num dudu = +(1.-gdi0i0)*(1.-gui1i1)*(1.-gdj0j0)*(1.-guj1j1)+(1.-gdi0i0)*(delta_i1j1-guj1i1)*gui1j1*(1.-gdj0j0)+(delta_i0j0-gdj0i0)*gdi0j0*(1.-gui1i1)*(1.-guj1j1)+(delta_i0j0-gdj0i0)*gdi0j0*(delta_i1j1-guj1i1)*gui1j1;
		const num dudd = +(1.-gdi0i0)*(1.-gui1i1)*(1.-gdj0j0)*(1.-gdj1j1)+(1.-gdi0i0)*(1.-gui1i1)*(delta_j0j1-gdj1j0)*gdj0j1+(delta_i0j0-gdj0i0)*gdi0j0*(1.-gui1i1)*(1.-gdj1j1)-(delta_i0j0-gdj0i0)*gdi0j1*(1.-gui1i1)*(delta_j0j1-gdj1j0)+(delta_i0j1-gdj1i0)*gdi0j0*(1.-gui1i1)*gdj0j1+(delta_i0j1-gdj1i0)*gdi0j1*(1.-gui1i1)*(1.-gdj0j0);
		const num dduu = +(1.-gdi0i0)*(1.-gdi1i1)*(1.-guj0j0)*(1.-guj1j1)+(1.-gdi0i0)*(1.-gdi1i1)*(delta_j0j1-guj1j0)*guj0j1+(delta_i0i1-gdi1i0)*gdi0i1*(1.-guj0j0)*(1.-guj1j1)+(delta_i0i1-gdi1i0)*gdi0i1*(delta_j0j1-guj1j0)*guj0j1;
		const num ddud = +(1.-gdi0i0)*(1.-gdi1i1)*(1.-guj0j0)*(1.-gdj1j1)+(1.-gdi0i0)*(delta_i1j1-gdj1i1)*gdi1j1*(1.-guj0j0)+(delta_i0i1-gdi1i0)*gdi0i1*(1.-guj0j0)*(1.-gdj1j1)-(delta_i0i1-gdi1i0)*gdi0j1*(delta_i1j1-gdj1i1)*(1.-guj0j0)+(delta_i0j1-gdj1i0)*gdi0i1*gdi1j1*(1.-guj0j0)+(delta_i0j1-gdj1i0)*gdi0j1*(1.-gdi1i1)*(1.-guj0j0);
		const num dddu = +(1.-gdi0i0)*(1.-gdi1i1)*(1.-gdj0j0)*(1.-guj1j1)+(1.-gdi0i0)*(delta_i1j0-gdj0i1)*gdi1j0*(1.-guj1j1)+(delta_i0i1-gdi1i0)*gdi0i1*(1.-gdj0j0)*(1.-guj1j1)-(delta_i0i1-gdi1i0)*gdi0j0*(delta_i1j0-gdj0i1)*(1.-guj1j1)+(delta_i0j0-gdj0i0)*gdi0i1*gdi1j0*(1.-guj1j1)+(delta_i0j0-gdj0i0)*gdi0j0*(1.-gdi1i1)*(1.-guj1j1);
		const num dddd = +(1.-gdi0i0)*(1.-gdi1i1)*(1.-gdj0j0)*(1.-gdj1j1)+(1.-gdi0i0)*(1.-gdi1i1)*(delta_j0j1-gdj1j0)*gdj0j1+(1.-gdi0i0)*(delta_i1j0-gdj0i1)*gdi1j0*(1.-gdj1j1)-(1.-gdi0i0)*(delta_i1j0-gdj0i1)*gdi1j1*(delta_j0j1-gdj1j0)+(1.-gdi0i0)*(delta_i1j1-gdj1i1)*gdi1j0*gdj0j1+(1.-gdi0i0)*(delta_i1j1-gdj1i1)*gdi1j1*(1.-gdj0j0)+(delta_i0i1-gdi1i0)*gdi0i1*(1.-gdj0j0)*(1.-gdj1j1)+(delta_i0i1-gdi1i0)*gdi0i1*(delta_j0j1-gdj1j0)*gdj0j1-(delta_i0i1-gdi1i0)*gdi0j0*(delta_i1j0-gdj0i1)*(1.-gdj1j1)-(delta_i0i1-gdi1i0)*gdi0j0*(delta_i1j1-gdj1i1)*gdj0j1+(delta_i0i1-gdi1i0)*gdi0j1*(delta_i1j0-gdj0i1)*(delta_j0j1-gdj1j0)-(delta_i0i1-gdi1i0)*gdi0j1*(delta_i1j1-gdj1i1)*(1.-gdj0j0)+(delta_i0j0-gdj0i0)*gdi0i1*gdi1j0*(1.-gdj1j1)-(delta_i0j0-gdj0i0)*gdi0i1*gdi1j1*(delta_j0j1-gdj1j0)+(delta_i0j0-gdj0i0)*gdi0j0*(1.-gdi1i1)*(1.-gdj1j1)+(delta_i0j0-gdj0i0)*gdi0j0*(delta_i1j1-gdj1i1)*gdi1j1-(delta_i0j0-gdj0i0)*gdi0j1*(1.-gdi1i1)*(delta_j0j1-gdj1j0)-(delta_i0j0-gdj0i0)*gdi0j1*(delta_i1j1-gdj1i1)*gdi1j0+(delta_i0j1-gdj1i0)*gdi0i1*gdi1j0*gdj0j1+(delta_i0j1-gdj1i0)*gdi0i1*gdi1j1*(1.-gdj0j0)+(delta_i0j1-gdj1i0)*gdi0j0*(1.-gdi1i1)*gdj0j1-(delta_i0j1-gdj1i0)*gdi0j0*(delta_i1j0-gdj0i1)*gdi1j1+(delta_i0j1-gdj1i0)*gdi0j1*(1.-gdi1i1)*(1.-gdj0j0)+(delta_i0j1-gdj1i0)*gdi0j1*(delta_i1j0-gdj0i1)*gdi1j0;
		m->nem_nnnn[bb + num_bb*t] += pre*(uuuu + uuud + uudu + uudd
		                                 + uduu + udud + uddu + uddd
		                                 + duuu + duud + dudu + dudd
		                                 + dduu + ddud + dddu + dddd);
		m->nem_ssss[bb + num_bb*t] += pre*(uuuu - uuud - uudu + uudd
		                                 - uduu + udud + uddu - uddd
		                                 - duuu + duud + dudu - dudd
		                                 + dduu - ddud - dddu + dddd);
	}
	}
	}
}
