#include "./f_EICR_shell.h"
#define PI 3.14159265359	
void f_EICR_shell(class FEAMesh & feaMesh, MatrixXd & GU_u0, VectorXd & GU_Rv0, std::vector<Material_ABD> & material, 
                struct Force & force, SparseMatrix<double>& sGKT, VectorXd& Res){
    // int dpn = 6, dpe = 18, npe = 3;
    const int & dpn = feaMesh.dpn, & dpe = feaMesh.dpe, & npe = feaMesh.npe;
    const bool & isOverlaid = feaMesh.isOverlaid;
    VectorXi BCid = feaMesh.BCid;

    // parsing inputs
    MatrixXd FNM = force.NM, Ffix = force.fix;
    MatrixXd NODE = feaMesh.NODE;
    MatrixXi ELEM = feaMesh.ELEM;
    const int nNODE = NODE.rows(), nELEM = ELEM.rows();
    const int nDOF = nNODE*dpn;

    // assert(FNM.rows() == nELEM, "size of F_nm does not match to nELEM")

    // current state of deformation
    // MatrixXd xm_u; xm_u = Map<MatrixXd>(GU_u0.data, nNODE*3 ,1);
    Map<MatrixXd> xm_u(GU_u0.data(), nNODE*3, 1);
    MatrixXd xm_u3 = GU_u0;
    VectorXd xm_Rv = GU_Rv0;
    MatrixXd xm_R = fstore2mat(xm_Rv);

    std::vector<Triplet<double> > GKT;
    
	if (isOverlaid == true){
		GKT.reserve(nELEM*dpe*dpe*4);
	}
	else GKT.reserve(nELEM*dpe*dpe);
    
    // MatrixXd GKT_mat = MatrixXd::Zero(nDOF,nDOF);
    VectorXd GFi = VectorXd::Zero(nDOF);
    MatrixXd GF_dof = Ffix.transpose();
    MatrixXd GF_dof_map = Map<VectorXd>(GF_dof.data(),nDOF);

    // ids for membrane and bending 
    Matrix<int,9,1> mdof, bdof;
    mdof << 0, 1, 5, 6, 7, 11, 12, 13, 17;
    bdof << 2, 3, 4, 8, 9, 10, 14, 15, 16;

    // gpts
    int ngp = 3;    // num of gpts
    std::vector<double> ri(ngp), si(ngp), wi(ngp);
    if (ngp == 3){
        ri = {0, 0.5, 0.5}, si = {0.5, 0, 0.5}, wi = {0.333333333, 0.333333333, 0.333333333};
    }
    else if(ngp == 1){
        ri[0] = 1.0/3, si[0] = 1.0/3, wi[0] = 1;
    }

    // Overlaid case 
    MatrixXi elem_id0(3,1), elem_order(1,3);
    elem_order << 0, 1, 2;
    // Vector3i elem_id0;
    // Matrix<int, 1, 3> elem_order; elem_order << 0, 1, 2;
    if (isOverlaid == true){
        elem_id0 = MatrixXi::Zero(4,1);
        elem_order = MatrixXi::Zero(4,3);
        elem_order << 0,1,2, 1,2,3, 2,3,0, 3,0,1;
        // Matrix<int, 4, 1> elem_id0;
        // Matrix<int, 4, 3> elem_order; elem_order << 0,1,2, 1,2,3, 2,3,0, 3,0,1;
    }
    
    for (int ee = 0; ee < nELEM; ++ee){
        elem_id0 = ELEM.row(ee); 

        Matrix<double,3,3> Amat, Dmat, Bmat;        
        Amat = material[ee].Amat;
        Dmat = material[ee].Dmat;
        Bmat = material[ee].Bmat;

        for (unsigned int kk = 0; kk < elem_order.rows(); ++kk){
            Vector3i elem_id; 
            for (unsigned int ppp = 0; ppp < 3; ++ppp ){
                elem_id(ppp) = elem_id0(elem_order(kk,ppp));
            }

            Matrix<double,3,3> X, u, x; 
            for (int mmm = 0; mmm < 3; ++mmm){
                X.col(mmm) = NODE.row(elem_id(mmm)).transpose();
                u.col(mmm) = xm_u3.row(elem_id(mmm)).transpose();
            }
            x = X + u;
            
            // Transpose matrix [e1; e2; e3]
            Matrix<double,3,3> T0, T;
            // eigen library has a roundoff error
            // dot_normal = dot_normal > 1.0 ? 1.0 : (dot_normal < -1.0 ? -1.0 : dot_normal);
            double dot_normal;
            T0.row(0) = X.col(1)-X.col(0);
            dot_normal = T0.row(0).norm();
            dot_normal = dot_normal > 1.0 ? 1.0 : (dot_normal < -1.0 ? -1.0 : dot_normal);
            T0.row(0) /= dot_normal; //T0.row(0).norm();
            
            T0.row(2) = T0.row(0).cross(X.col(2)-X.col(0));
            dot_normal = T0.row(2).norm();
            dot_normal = dot_normal > 1.0 ? 1.0 : (dot_normal < -1.0 ? -1.0 : dot_normal);
            
            T0.row(2) /= dot_normal; //T0.row(2).norm();
            T0.row(1) = T0.row(2).cross(T0.row(0));

            T.row(0) = x.col(1)-x.col(0);
            dot_normal = T.row(0).norm();
            dot_normal = dot_normal > 1.0 ? 1.0 : (dot_normal < -1.0 ? -1.0 : dot_normal);
            T.row(0) /= dot_normal; //T.row(0).norm();
            T.row(2) = T.row(0).cross(x.col(2)-x.col(0));
            dot_normal = T.row(2).norm();
            dot_normal = dot_normal > 1.0 ? 1.0 : (dot_normal < -1.0 ? -1.0 : dot_normal);
            T.row(2) /= dot_normal;//T.row(2).norm();
            T.row(1) = T.row(2).cross(T.row(0));

            // filtering r.b.m
            Vector3d X_c, x_c;
            Matrix3d X_R, x_R;
            
            X_c = 1.0/3.0*X.rowwise().sum();
            x_c = 1.0/3.0*x.rowwise().sum();
            X_R = T0*(X-X_c.replicate(1,3));
            x_R = T *(x-x_c.replicate(1,3));
            
            Matrix3d u_d = x_R - X_R; 
            // Map<RowVectorXd> u_d(x_R.data()-X_R.data(),9);
            // u_d = Map<MatrixXd>(x_R.data() - X_R.data(),9,1);

            Matrix<double,9,1> th_d;// = Matrix<double,9,1>::Zero();
            for (int mmm = 0; mmm < 3; ++mmm){
                Matrix<double,3,3> Ra = xm_R.middleRows(elem_id(mmm)*3,3), R_d;
                R_d = T*Ra*T0.transpose();
                th_d.middleRows(mmm*3,3) = rot2vec(R_d);            
            }

            // Elastic material stiffness / forces K_el, f_el
            Matrix<double,2,3> xycoord = x_R.topRows(2);

            // Matreiral of local element
            Matrix<double,3,3> Q;
            Q << T0(0,0)*T0(0,0), T0(0,1)*T0(0,1), T0(0,0)*T0(0,1),
                T0(1,0)*T0(1,0), T0(1,1)*T0(1,1), T0(1,0)*T0(1,1),
                2*T0(0,0)*T0(1,0), 2*T0(0,1)*T0(1,1), T0(0,0)*T0(1,1)+T0(0,1)*T0(1,0);
            
            Matrix<double,6,1> FNM_r;
            FNM_r.topRows(3) = (FNM.block(ee,0,1,3)*Q.transpose()).transpose();
            FNM_r.bottomRows(3) = (FNM.block(ee,3,1,3)*Q.transpose()).transpose();
            Material_ABD Mater_e;
            Mater_e.Amat = Q*Amat*Q.transpose();
            Mater_e.Dmat = Q*Dmat*Q.transpose();
            Mater_e.Bmat = Q*Bmat*Q.transpose();
            // std::cout << "Q" << Q << std::endl;       

            FilteredP p_d;
            p_d.membrane << u_d(0), u_d(1), th_d(2), u_d(3), u_d(4), th_d(5), u_d(6), u_d(7), th_d(8);
            p_d.bending  << u_d(2), th_d(0), th_d(1), u_d(5), th_d(3), th_d(4), u_d(8), th_d(6), th_d(7);
            
            // CoreElement coreelem = f_core_element(xycoord,Mater_e,FNM_r,p_d);
            CoreElement coreelem(xycoord, Mater_e, FNM_r, p_d);
            coreelem.get_filteredmat(ri, si, wi);
            
            // std::cout << "coreelem.Km\n" << coreelem.Km << std::endl;Map<VectorXd>(GF_dof.data(),nDOF)
            // std::cout << "coreelem.Kb\n" << coreelem.Kb << std::endl;
            // std::cout << "coreelem.Kmb\n" << coreelem.Kmb << std::endl;
            // std::cout << "coreelem.Fm\n" << coreelem.Fm << std::endl;
            // std::cout << "coreelem.Fb\n" << coreelem.Fb << std::endl;
            // std::cout << "coreelem.Fmf\n" << coreelem.Fmf << std::endl;
            // std::cout << "coreelem.Fbf\n" << coreelem.Fbf << std::endl;

            MatrixXd K_el = MatrixXd::Zero(18,18);
            VectorXd f_el = VectorXd::Zero(18), f_el_f = VectorXd::Zero(18);

            if (isOverlaid == false){
                for (int mmm = 0; mmm < 9 ; ++mmm){
                    for (int nnn = 0; nnn < 9; ++nnn){
                        K_el(mdof(mmm),mdof(nnn)) += coreelem.Km(mmm,nnn);
                        K_el(bdof(mmm),bdof(nnn)) += coreelem.Kb(mmm,nnn);
                        K_el(mdof(mmm),bdof(nnn)) += coreelem.Kmb(mmm,nnn);
                        K_el(bdof(mmm),mdof(nnn)) += coreelem.Kmb(nnn,mmm);
                    }
                    f_el(mdof(mmm)) += coreelem.Fm(mmm);
                    f_el(bdof(mmm)) += coreelem.Fb(mmm);

                    f_el_f(mdof(mmm)) += coreelem.Fmf(mmm);
                    f_el_f(bdof(mmm)) += coreelem.Fbf(mmm);
                }
            }
            else{
                for (int mmm = 0; mmm < 9 ; ++mmm){
                    for (int nnn = 0; nnn < 9; ++nnn){
                        K_el(mdof(mmm),mdof(nnn)) += coreelem.Km(mmm,nnn)/2;
                        K_el(bdof(mmm),bdof(nnn)) += coreelem.Kb(mmm,nnn)/2;
                        K_el(mdof(mmm),bdof(nnn)) += coreelem.Kmb(mmm,nnn)/2;
                        K_el(bdof(mmm),mdof(nnn)) += coreelem.Kmb(nnn,mmm)/2;
                    }
                    f_el(mdof(mmm)) += coreelem.Fm(mmm)/2;
                    f_el(bdof(mmm)) += coreelem.Fb(mmm)/2;

                    f_el_f(mdof(mmm)) += coreelem.Fmf(mmm)/2;
                    f_el_f(bdof(mmm)) += coreelem.Fbf(mmm)/2;
                }
            }
            
            // Auxilary matrices of EICR
            AuxMat aux = f_EICR(x_R,th_d,f_el,f_el_f,T);

            // element stiffness computation
            MatrixXd KT_int, KT_ext, LKT, Fi;
            KT_int.noalias() =  aux.Te.transpose()*(aux.P_.transpose()*aux.H_.transpose()*K_el*aux.H_*aux.P_)*aux.Te;
            KT_int.noalias() -= aux.Te.transpose()*(aux.Fh_nm*aux.G_ + aux.G_.transpose()*aux.F_n.transpose()*aux.P_)*aux.Te;
            KT_int.noalias() += aux.Te.transpose()*(aux.P_.transpose()*aux.M_*aux.P_)*aux.Te;

            KT_ext.noalias() = aux.Te.transpose()*(aux.M_ext*aux.P_ - aux.F_nm_ext*aux.G_)*aux.Te;

            LKT = KT_int - KT_ext;
            // std::cout << "aux.Te\n" << aux.Te << std::endl;
            // std::cout << "aux.P_\n" << aux.P_ << std::endl;
            // std::cout << "aux.H_\n" << aux.H_ << std::endl;
            // std::cout << "aux.G_\n" << aux.G_ << std::endl;
            // std::cout << "K_el\n" << K_el << std::endl; 
            // std::cout << "aux.F_n\n" << aux.F_n << std::endl;  
            // std::cout << "aux.F_nm_ext\n" << aux.F_nm_ext << std::endl;  
            // std::cout << "aux.Fh_nm\n" << aux.Fh_nm << std::endl;  
            // std::cout << "aux.M_\n" << aux.M_ << std::endl; 
            // std::cout << "aux.M_ext\n" << aux.M_ext << std::endl; 
            // std::cout << "LKT\n" << LKT << std::endl; 
            // std::cout << "KT_ext\n" << KT_ext << std::endl; 

            Fi.noalias() = aux.Te.transpose()*aux.P_.transpose()*aux.H_.transpose()*f_el;
            Fi.noalias() -= aux.Te.transpose()*aux.H_.transpose()*f_el_f;

            // assembly
            Matrix<int,18,1> dof_sum(18); 
            dof_sum <<  VectorXi::LinSpaced(6,elem_id(0)*6,elem_id(0)*6+5), 
                        VectorXi::LinSpaced(6,elem_id(1)*6,elem_id(1)*6+5),
                        VectorXi::LinSpaced(6,elem_id(2)*6,elem_id(2)*6+5);
            
            for (int mmm = 0; mmm < 18; ++mmm){
                if (BCid.cwiseEqual(dof_sum(mmm)).any() > 0){
                    GF_dof_map(dof_sum(mmm)) = 0;
                    continue;
                }
                GFi(dof_sum(mmm)) += Fi(mmm);   
                for (int nnn = 0; nnn < 18; ++nnn){
                    if (BCid.cwiseEqual(dof_sum(nnn)).any() > 0) continue;
                    GKT.push_back(Triplet<double>(dof_sum(mmm),dof_sum(nnn),LKT(mmm,nnn)));
                    // GKT_mat(dof_sum(mmm),dof_sum(nnn)) += LKT(mmm,nnn);
                    // std::cout << "GKT(" << dof_sum(mmm) << "," << dof_sum(nnn) << "): "<< LKT(mmm,nnn) << std::endl;
                }
            } 
        }       
    }
    // std::cout <<" ==== end of gkt ====" << std::endl;
    // std::cout << "Rnorm:" << Res.norm() << std::endl;
    // VectorXd Res = Map<VectorXd>(GF_dof.data(),nDOF) - GFi;
    Res = GF_dof_map - GFi; //Checked.
    // if (p_sGKT == NULL){
    //     SparseMatrix<double> sGKT(nDOF,nDOF);
    //     SparseMatrix<double>* p_sGKT = &sGKT;
    //     VectorXd Res(nDOF,1);
    //     VectorXd* p_Res = &Res;
    // }    
    // p_sGKT->setFromTriplets(GKT.begin(),GKT.end());
    // *p_Res = Map<VectorXd>(GF_dof.data(),nDOF) - GFi;
    
    sGKT.setFromTriplets(GKT.begin(),GKT.end());
    // Boundary conditions
    // for (int bb = 0; bb < BCid.size(); ++bb){
    //     for (int pp = 0; pp < nDOF; ++pp){
    //         // p_sGKT->coeffRef(BCid(bb),pp) = 0;
    //         // p_sGKT->coeffRef(pp,BCid(bb)) = 0;
    //         sGKT.coeffRef(BCid(bb),pp) = 0;
    //         sGKT.coeffRef(pp,BCid(bb)) = 0;
    //         // GKT_mat(BCid(bb),pp) = 0;
    //         // GKT_mat(pp,BCid(bb)) = 0;
    //     }        
    //     // p_sGKT->coeffRef(BCid(bb),BCid(bb)) = 1;
    //     // GKT_mat(BCid(bb),BCid(bb)) = 1;
    //     sGKT.coeffRef(BCid(bb),BCid(bb)) = 1;
    //     Res(BCid(bb),0) = 0;
    // }
    // std::cout << "====Res====" << Res << "\n ============\n" << std::endl;
    // saveMarket(sGKT, "sGKT.mtx");
    // return GKT_mat;

}

AuxMat f_EICR(Matrix3d & x_R, Matrix<double,9,1> & th_d, VectorXd & f_el, VectorXd & f_ph, Matrix3d & T){

    Matrix3d zero3 = Matrix3d::Zero(), eye3 = Matrix3d::Identity();
    MatrixXd Te = MatrixXd::Zero(18,18);
    Te << T, zero3, zero3, zero3, zero3, zero3,
        zero3, T, zero3, zero3, zero3, zero3, 
        zero3, zero3, T, zero3, zero3, zero3,
        zero3, zero3, zero3, T, zero3, zero3,
        zero3, zero3, zero3, zero3, T, zero3,
        zero3, zero3, zero3, zero3, zero3, T;
    
    Matrix<double,3,3> sx1_, sx2_, sx3_;
    sx1_ << 0, -x_R(2,0), x_R(1,0), x_R(2,0), 0, -x_R(0,0), -x_R(1,0), x_R(0,0), 0;
    sx2_ << 0, -x_R(2,1), x_R(1,1), x_R(2,1), 0, -x_R(0,1), -x_R(1,1), x_R(0,1), 0;
    sx3_ << 0, -x_R(2,2), x_R(1,2), x_R(2,2), 0, -x_R(0,2), -x_R(1,2), x_R(0,2), 0;

    MatrixXd St_ = MatrixXd::Zero(3,18), S_ = MatrixXd::Zero(18,3);
    St_ << sx1_, eye3, sx2_, eye3, sx3_, eye3;
    S_ = St_.transpose();

    MatrixXd G_ = MatrixXd::Zero(3,18);
    double A = getArea(x_R);
    G_.middleCols(0,3)  << 0,0,x_R(0,2)-x_R(0,1), 0,0,x_R(1,2)-x_R(1,1), -0.5*(x_R(0,2)-x_R(0,1)), -0.5*(x_R(1,2)-x_R(1,1)),0;
    G_.middleCols(6,3)  << 0,0,x_R(0,0)-x_R(0,2), 0,0,x_R(1,0)-x_R(1,2), -0.5*(x_R(0,0)-x_R(0,2)), -0.5*(x_R(1,0)-x_R(1,2)),0;
    G_.middleCols(12,3) << 0,0,x_R(0,1)-x_R(0,0), 0,0,x_R(1,1)-x_R(1,0), -0.5*(x_R(0,1)-x_R(0,0)), -0.5*(x_R(1,1)-x_R(1,0)),0;
    G_ *= 0.5/A;

    MatrixXd Pw;
    Pw = S_*G_;

    MatrixXd Uab0(6,6);
    Uab0 << eye3, zero3, zero3, zero3;
    Uab0 *= 1.0/3.0;
    MatrixXd Pu0 = MatrixXd::Zero(18,18), Pu = MatrixXd::Zero(18,18), P_ = MatrixXd::Zero(18,18);
    Pu0 << Uab0, Uab0, Uab0, Uab0, Uab0, Uab0, Uab0, Uab0, Uab0;
    Pu = MatrixXd::Identity(18,18) - Pu0;
    P_ = Pu - Pw;

    MatrixXd H_ = MatrixXd::Identity(18,18);
    Vector3d the;
    for (int mmm = 0; mmm < 3; ++mmm){
        the = th_d.middleRows(mmm*3,3);
        MatrixXd s_the(3,3);
        s_the << 0, -the(2), the(1), the(2), 0, -the(0), -the(1), the(0), 0;
        double n_the = the.norm(), eta;
       
        if (n_the < 0.001){
            eta = 1.0/12.0 + 1./720*(n_the*n_the) + 1./30240*std::pow(n_the,4);
        }
        else{
            eta = (1-0.5*n_the*1./std::tan(0.5*n_the))/(n_the*n_the);
        }
        H_.block(mmm*6+3,mmm*6+3,3,3).noalias() = eye3 - 0.5*s_the + eta*s_the*s_the;
    }

    // Pull back to global stiffness / forces
    MatrixXd Fh_nm = MatrixXd::Zero(18,3);
    MatrixXd F_n = MatrixXd::Zero(18,3);
    MatrixXd F_nm_ext = MatrixXd::Zero(18,3);
    MatrixXd M_ = MatrixXd::Zero(18,18);
    MatrixXd M_ext = MatrixXd::Zero(18,18);

    for (int mmm = 0; mmm < 3; ++mmm){
        // setup for M(=d(theta)/d(w)) matrix
        the = th_d.middleRows(mmm*3,3);
        Matrix<double,3,3> s_the;
        s_the << 0, -the(2), the(1), the(2), 0, -the(0), -the(1), the(0), 0;
        double n_the = the.norm(), eta;

        double mu;
        if (n_the < 0.001){
            eta = 1./12 + 1./720*(n_the*n_the) + 1./30240*std::pow(n_the,4);
            mu = 1./360 + 1./7560*n_the*n_the + 1./201600*std::pow(n_the,4) + 1./5987520*std::pow(n_the,6);
        }
        else{
            eta = (1-0.5*n_the*1./std::tan(0.5*n_the))/(n_the*n_the);
            mu  = (n_the*n_the+4*std::cos(n_the)+n_the*std::sin(n_the)-4)/(4*std::pow(n_the,4)*std::sin(0.5*n_the)*std::sin(0.5*n_the));
        }
        MatrixXd H_a(3,3), sm(3,3);
        Vector3d m;

        H_a = H_.block(mmm*6+3,mmm*6+3,3,3);
        m = f_el.middleRows(mmm*6+3,3); 

        sm << 0, -m(2), m(1), m(2), 0, -m(0), -m(1), m(0), 0;

        M_.block(6*mmm+3, 6*mmm+3,3,3).noalias() = (eta*((the.transpose()*m)*eye3 + the*m.transpose() - 2*m*the.transpose()) + mu*s_the*s_the*the*m.transpose() - 1./2*sm)*H_a;
        
        // -------------------------------
        
        MatrixXd PHf  = P_.transpose()*H_.transpose()*f_el;
        MatrixXd nh = PHf.middleRows(6*mmm,3);
        MatrixXd mh = PHf.middleRows(6*mmm+3,3);
        
        MatrixXd snh(3,3), smh(3,3);
        snh << 0, -nh(2), nh(1), nh(2), 0, -nh(0), -nh(1), nh(0), 0;
        smh << 0, -mh(2), mh(1), mh(2), 0, -mh(0), -mh(1), mh(0), 0;
                
        // Fh_nm.middleRows(6*mmm,3)= snh;
        // Fh_nm.middleRows(6*mmm+3,3) = smh;

        Fh_nm.middleRows(6*mmm,3) <<snh(0,0), snh(0,1), snh(0,2),
                                    snh(1,0), snh(1,1), snh(1,2),
                                    snh(2,0), snh(2,1), snh(2,2);

        Fh_nm.middleRows(6*mmm+3,3) <<  smh(0,0), smh(0,1), smh(0,2),
                                        smh(1,0), smh(1,1), smh(1,2),
                                        smh(2,0), smh(2,1), smh(2,2);
        
        // -------------------------------
        
        MatrixXd Hf = P_.transpose()*f_el;
        Vector3d n_a = Hf.middleRows(6*mmm,3);
        MatrixXd s_na(3,3);
        s_na << 0, -n_a(2), n_a(1), n_a(2), 0, -n_a(0), -n_a(1), n_a(0), 0;
                
        F_n.middleRows(6*mmm,3) <<  s_na(0,0), s_na(0,1), s_na(0,2),
                                    s_na(1,0), s_na(1,1), s_na(1,2),
                                    s_na(2,0), s_na(2,1), s_na(2,2);
        
        // ----------------------
        // additional SENSITIVITY DUE TO FOLLOWING FORCE
        // ----------------------
        
        Vector3d m_ph = f_ph.middleRows(6*mmm+3,3);
        
        MatrixXd sm_ph(3,3);
        sm_ph << 0, -m_ph(2), m_ph(1), m_ph(2), 0, -m_ph(0), -m_ph(1), m_ph(0), 0;
        
        M_ext.block(6*mmm+3,6*mmm+3,3,3) = 
            (eta*((the.transpose()*m_ph)*eye3 + the*m_ph.transpose() - 2*m_ph*the.transpose()) + 
            mu*s_the*s_the*the*m_ph.transpose() - 1./2*sm_ph)*H_a;
        
        // ------------------------------
        VectorXd Hf_ph = H_.transpose()*f_ph;
        VectorXd na_ph = Hf_ph.middleRows(6*mmm,3);
        VectorXd ma_ph = Hf_ph.middleRows(6*mmm+3,3);
        
        MatrixXd sna_ph(3,3), sma_ph(3,3);
        sna_ph << 0, -na_ph(2), na_ph(1), na_ph(2), 0, -na_ph(0), -na_ph(1), na_ph(0), 0;
        sma_ph << 0, -ma_ph(2), ma_ph(1), ma_ph(2), 0, -ma_ph(0), -ma_ph(1), ma_ph(0), 0;
        
        /*
        // NEVER USE MiddleROWS for filling in (only a first col goes into it)
        F_nm_ext.middleRows(6*mmm,3) = sna_ph; 
        F_nm_ext.middleRows(6*mmm+3,3) = sma_ph;
        */
        F_nm_ext.middleRows(6*mmm,3) <<sna_ph(0,0), sna_ph(0,1), sna_ph(0,2),
                                    sna_ph(1,0), sna_ph(1,1), sna_ph(1,2),
                                    sna_ph(2,0), sna_ph(2,1), sna_ph(2,2);

        F_nm_ext.middleRows(6*mmm+3,3) <<  sma_ph(0,0), sma_ph(0,1), sma_ph(0,2),
                                        sma_ph(1,0), sma_ph(1,1), sma_ph(1,2),
                                        sma_ph(2,0), sma_ph(2,1), sma_ph(2,2);
    } 
    AuxMat aux;
    aux.Te = Te; aux.P_ = P_; aux.G_ = G_; aux.H_ = H_;
    aux.Fh_nm = Fh_nm; aux.F_n = F_n; aux.F_nm_ext = F_nm_ext;
    aux.M_ext = M_ext; aux.M_ = M_;
    return aux;
}

double getArea(MatrixXd x){
    // calculate Area of triangular element
    MatrixXd x21 = (x.col(1)-x.col(0)), x31 = (x.col(2)-x.col(0));
    Vector3d crossx2x3; 
    crossx2x3 << x21(1)*x31(2)-x21(2)*x31(1),x21(2)*x31(0)-x21(0)*x31(2),x21(0)*x31(1)-x21(1)*x31(0);
    double Area = 0.5*crossx2x3.norm();
    return Area;
}

Vector3d rot2vec(MatrixXd R){
    double ang = std::abs(std::acos((R(0,0)+R(1,1)+R(2,2)-1)/2));
    double taylors;
    if (ang == PI){
        throw("rot_vec() becomes instable as rot = PI");
        taylors = -0.5; // added on 04/24/17
     }

    if (ang == 2*PI){
       throw("rot_vec() becomes instable as rot = PI");
       taylors = 0.5; // added on 04/24/17
    }

    if (ang < 0.001){
        taylors = 0.5 + std::pow(ang,2)/12 + (7*std::pow(ang,4))/720; 
    }
    else{
        taylors = ang/(2*sin(ang));
    }

    Vector3d m_rot_vec;
    m_rot_vec << R(2,1)-R(1,2), R(0,2)-R(2,0), R(1,0)-R(0,1); 
    m_rot_vec *= taylors;

    return m_rot_vec;
}

MatrixXd fstore2mat(VectorXd Rv){
    int num_el = Rv.rows()/9;
    Matrix<double,9,1> x;
    MatrixXd store2mat = MatrixXd::Zero(num_el*3,3);
    for (int ii = 0; ii < num_el; ++ii){
        // std::cout << Rv.middleRows(9*ii,9) << "-----------\n" << std::endl;
        x = Rv.middleRows(9*ii,9);
        store2mat.middleRows(ii*3,3) << x(0), x(3), x(6), 
                                        x(1), x(4), x(7), 
                                        x(2), x(5), x(8); 
        // = Map<MatrixXd>(x.data(),3,3).transpose();
    }
    return store2mat;
    
}

VectorXd fmat2store(MatrixXd R){
    int num_el = R.rows()/3;
    VectorXd mat2store = VectorXd::Zero(num_el*9);
    for (int ii = 0; ii < num_el; ++ii){
        mat2store.middleRows(ii*9,9) << R(ii*3, 0), R(ii*3+1, 0), R(ii*3+2, 0),
                                        R(ii*3, 1), R(ii*3+1, 1), R(ii*3+2, 1),
                                        R(ii*3, 2), R(ii*3+1, 2), R(ii*3+2, 2);
        // Map<MatrixXd>(R.middleRows(ii*3,3).transpose().data(),9,1);
    }
    return mat2store;
}
