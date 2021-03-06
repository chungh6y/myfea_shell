#ifndef F_CORE_ELEMENT_H
#define F_CORE_ELEMENT_H

#include "./linshell.h"
#include "./f_DKT_OPT.h"
#include "./FEA_hy.h"
// #include "./f_nlgeom.h"
// #include "./f_EICR_shell.h"
using namespace Eigen;
struct CoreElement
{
    // MatrixXd Km(9,9), Kb(9,9), Kmb(9,9);
    // MatrixXd Fm(9,1), Fb(9,1), Fmf(9,1), Fbf(9,1);
    MatrixXd Km = MatrixXd::Zero(9, 9), Kb = MatrixXd::Zero(9, 9), Kmb = MatrixXd::Zero(9, 9);
    VectorXd Fm = VectorXd::Zero(9), Fb = VectorXd::Zero(9), Fmf = VectorXd::Zero(9), Fbf = VectorXd::Zero(9);
};

struct FilteredP
{
    Matrix<double, 9, 1> membrane = Matrix<double, 9, 1>::Zero(9), bending = Matrix<double, 9, 1>::Zero(9);
};

struct GaussProp // properties at the Gausspoints
{
    VectorXd eps0, kappa; // in-plane stress, curvature
    VectorXd resN, resM;  // resultants
    double sens;          // e.g., uKu
};

CoreElement f_core_element(const MatrixXd &xycoord, const Material_ABD &Mater_e, const VectorXd &FNM_r, const FilteredP &p_d);

CoreElement f_core_element(const MatrixXd &xycoord, const Material_ABD &Mater_e, const VectorXd &FNM_r);

std::vector<GaussProp> f_GaussProp(const MatrixXd &xycoord, const Material_ABD &Mater_e,
                                   const VectorXd &FNM_r, const FilteredP &p_d);
#endif
