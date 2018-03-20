// BEGINLICENSE
//
// This file is part of helPME, which is distributed under the BSD 3-clause license,
// as described in the LICENSE file in the top level directory of this project.
//
// Author: Andrew C. Simmonett
//
// ENDLICENSE
#include "helpme.h"

#include <iostream>

// C Wrappers

extern "C" {

typedef enum { XAligned = 0, ShapeMatrix = 1 } LatticeType;

PMEInstanceD* helpme_createD() {
    try {
        return new PMEInstanceD();
    } catch (std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    } catch (...) {
        std::cerr << "An unknown error occured in helpme_createD" << std::endl;
        exit(1);
    }
}

PMEInstanceF* helpme_createF() {
    try {
        return new PMEInstanceF();
    } catch (std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    } catch (...) {
        std::cerr << "An unknown error occured in helpme_createF" << std::endl;
        exit(1);
    }
}
void helpme_setupD(PMEInstanceD* pme, short rPower, double kappa, int splineOrder, int aDim, int bDim, int cDim,
                   double scaleFactor, int nThreads) {
    try {
        pme->setup(rPower, kappa, splineOrder, aDim, bDim, cDim, scaleFactor, nThreads);
    } catch (std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    } catch (...) {
        std::cerr << "An unknown error occured in helpme_setupD" << std::endl;
        exit(1);
    }
}

void helpme_setupF(PMEInstanceF* pme, short rPower, float kappa, int splineOrder, int aDim, int bDim, int cDim,
                   float scaleFactor, int nThreads) {
    try {
        pme->setup(rPower, kappa, splineOrder, aDim, bDim, cDim, scaleFactor, nThreads);
    } catch (std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    } catch (...) {
        std::cerr << "An unknown error occured in helpme_setupF" << std::endl;
        exit(1);
    }
}

void helpme_set_lattice_vectorsD(PMEInstanceD* pme, double A, double B, double C, double alpha, double beta,
                                 double gamma, LatticeType latticeType) {
    try {
        pme->setLatticeVectors(A, B, C, alpha, beta, gamma, PMEInstanceD::LatticeType(latticeType));
    } catch (std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    } catch (...) {
        std::cerr << "An unknown error occured in helpme_set_lattice_vectorsD" << std::endl;
        exit(1);
    }
}

void helpme_set_lattice_vectorsF(PMEInstanceF* pme, float A, float B, float C, float alpha, float beta, float gamma,
                                 LatticeType latticeType) {
    try {
        pme->setLatticeVectors(A, B, C, alpha, beta, gamma, PMEInstanceF::LatticeType(latticeType));
    } catch (std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    } catch (...) {
        std::cerr << "An unknown error occured in helpme_set_lattice_vectorsF" << std::endl;
        exit(1);
    }
}

double helpme_compute_EF_recD(PMEInstanceD* pme, int nAtoms, int parameterAngMom, double* parameters,
                              double* coordinates, double* forces) {
    try {
        int nParam = helpme::nCartesian(parameterAngMom);
        helpme::Matrix<double> paramMat(parameters, nAtoms, nParam);
        helpme::Matrix<double> coordMat(coordinates, nAtoms, 3);
        helpme::Matrix<double> forceMat(forces, nAtoms, 3);
        return pme->computeEFRec(parameterAngMom, paramMat, coordMat, forceMat);
    } catch (std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    } catch (...) {
        std::cerr << "An unknown error occured in helpme_run_pmeD" << std::endl;
        exit(1);
    }
}

float helpme_compute_EF_recF(PMEInstanceF* pme, int nAtoms, int parameterAngMom, float* parameters, float* coordinates,
                             float* forces) {
    try {
        int nParam = helpme::nCartesian(parameterAngMom);
        helpme::Matrix<float> paramMat(parameters, nAtoms, nParam);
        helpme::Matrix<float> coordMat(coordinates, nAtoms, 3);
        helpme::Matrix<float> forceMat(forces, nAtoms, 3);
        return pme->computeEFRec(parameterAngMom, paramMat, coordMat, forceMat);
    } catch (std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    } catch (...) {
        std::cerr << "An unknown error occured in helpme_run_pmeF" << std::endl;
        exit(1);
    }
}
}