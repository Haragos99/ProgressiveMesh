#pragma once
#include <QGLViewer/qglviewer.h>
#include <vector>
#include <Eigen/Eigen>
#include <fstream>
#include <iostream>
using qglviewer::Vec;



using DoubleMatrix = std::vector<std::vector<double>>;
using VectorMatrix = std::vector<std::vector<Vec>>;

struct BSBasis {
public:
    // Constructors
    BSBasis();
    BSBasis(const BSBasis&) = default;
    BSBasis(size_t degree, const std::vector<double>& knots);
    BSBasis& operator=(const BSBasis&) = default;

    // Properties
    size_t degree() const;
    void setDegree(size_t degree);
    const std::vector<double>& knots() const;
    std::vector<double>& knots();
    double low() const;
    double high() const;
    
    // Utilities
    void reverse();
    void normalize();
    size_t findSpan(double u) const;
    size_t findSpanWithMultiplicity(double u, size_t& multi) const;
    void basisFunctions(size_t i, double u, std::vector<double>& coeff) const;
    void basisFunctionsAll(size_t i, double u, DoubleMatrix& coeff) const;
    void basisFunctionDerivatives(size_t i, double u, size_t nr_der, DoubleMatrix& coeff) const;

private:
    size_t p_;
    std::vector<double> knots_;
};





struct BSpline {
    size_t nu;
    size_t nv;
    float f=0.5;
    BSBasis basis_u_, basis_v_;
    std::vector<double> knots;
    std::vector<Vec> control_points;
    BSpline();
    BSpline(const BSpline&) = default;
    BSpline(size_t deg_u, size_t deg_v, const std::vector<Vec>& cpts);
    BSpline(size_t deg_u, size_t deg_v, const std::vector<double>& knots_u, const std::vector<double>& knots_v,
        const std::vector<Vec>& cpts, int _nv, int _nu);
    Vec eval(double u, double v) const;
    Vec eval(double u, double v, size_t nr_der, VectorMatrix& der) const;
    BSpline open(const std::string& filname);
    void draw();
    void arrow();
    void calculate_points();
    const std::vector<Vec>&
       controlPoints() const {
        return control_points;
    }

    std::array<size_t, 2> numControlPoints() const {
        return { nu + 1, nv + 1 };
    }
    Vec controlPoint(size_t i, size_t j) const {
        return control_points[i * (nv + 1) + j];
    }

    int index(size_t i, size_t j) {
        return i * (nv + 1) + j;
    }

    Vec&
        BcontrolPoint(size_t i, size_t j) {
        return control_points[i * (nv + 1) + j];
    }

    std::vector<Vec>&controlPoints() {
        return control_points;
    }

    int _index(size_t i, size_t j) {
        return i * (5 + 1) + j;
    }


    float yi(int i)
    {
        float result=0.0;
        for (int k = 1; k <= basis_u_.degree(); k++)
        {
            result += basis_u_.knots()[i+k];
        }
        return result/ basis_u_.degree();
    }



    double distance(Vec p, Vec p1)
    {
        double len = sqrt(pow(p.x - p1.x, 2) + pow(p.y - p1.y, 2) + pow(p.z - p1.z, 2));

        return len;
    }


    float yj(int j)
    {
        float result = 0.0;
        for (int k = 1; k <= basis_v_.degree(); k++)
        {
            result += basis_v_.knots()[j+k];
        }
        return result/ basis_v_.degree();
    }


};
