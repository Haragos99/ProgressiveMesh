// -*- mode: c++ -*-
#pragma once

#include <string>
#include <Eigen/Eigen>
#include <QGLViewer/qglviewer.h>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QGLViewer/quaternion.h>
#include "Openfiler.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <OpenMesh/Core/IO/MeshIO.hh>
#include <OpenMesh/Tools/Smoother/JacobiLaplaceSmootherT.hh>
#include <QtGui/QKeyEvent>
#include <QtWidgets>
#include <QGLViewer/quaternion.h>
#include <map>
#include <algorithm>

using qglviewer::Vec;


class MyViewer : public QGLViewer {
    Q_OBJECT

public:
    explicit MyViewer(QWidget* parent);
    virtual ~MyViewer();
    inline double getCutoffRatio() const;
    inline void setCutoffRatio(double ratio);
    inline double getMeanMin() const;
    inline void setMeanMin(double min);
    inline double getMeanMax() const;
    inline void setMeanMax(double max);
    inline const double* getSlicingDir() const;
    inline void setSlicingDir(double x, double y, double z);
    inline double getSlicingScaling() const;
    inline void setSlicingScaling(double scaling);
    bool openMesh(const std::string& filename, bool update_view = true);

    bool openBezier(const std::string& filename, bool update_view = true);
    bool saveBezier(const std::string& filename);



    void wierframe() {
        show_wireframe = !show_wireframe;
        update();
    }


   



    void Epsil() {
        auto dlg = std::make_unique<QDialog>(this);
        auto* hb1 = new QHBoxLayout;
        auto* vb = new QVBoxLayout;
        auto* ok = new QPushButton(tr("Ok"));
        connect(ok, SIGNAL(pressed()), dlg.get(), SLOT(accept()));
        ok->setDefault(true);
        QLabel* text;
        auto* sb_H = new QDoubleSpinBox;
        sb_H->setDecimals(4);
        sb_H->setSingleStep(0.0001);
        sb_H->setRange(0.0001, 1);
        hb1->addWidget(sb_H);
        hb1->addWidget(ok);
        vb->addLayout(hb1);
        dlg->setWindowTitle(tr("Skalar"));
        dlg->setLayout(vb);
        if (dlg->exec() == QDialog::Accepted) {
            epsilon = sb_H->value();
            update();
        }


    }

    


    bool transparent = false;

    float& getFrameSecond() { return FrameSecond; }
    double epsilon = 0.001;

    int wi = 2;





    Vec angels;
    Vec ang;

signals:
    void startComputation(QString message);
    void midComputation(int percent);
    void endComputation();
    void displayMessage(const QString& message);

protected:
    virtual void init() override;
    virtual void draw() override;
    virtual void drawWithNames() override;
    virtual void postSelection(const QPoint& p) override;
    virtual void keyPressEvent(QKeyEvent* e) override;
    virtual void mouseMoveEvent(QMouseEvent* e) override;
    virtual QString helpString() const override;
private:
    struct MyTraits : public OpenMesh::DefaultTraits {
        using Point = OpenMesh::Vec3d; // the default would be Vec3f
        using Normal = OpenMesh::Vec3d;
        VertexTraits{
          OpenMesh::Vec3d original;
          double mean;              // approximated mean curvature
          std::vector<double> weigh;
          std::vector<double> distance;
          int idx_of_closest_bone;

        };
        VertexAttributes(OpenMesh::Attributes::Normal |
            OpenMesh::Attributes::Color | OpenMesh::Attributes::Status);
    };
    using MyMesh = OpenMesh::TriMesh_ArrayKernelT<MyTraits>;
    using Vector = OpenMesh::VectorT<double, 3>;

    // Mesh
    void updateMesh(bool update_mean_range = true);
    void updateVertexNormals();
#ifdef USE_JET_FITTING
    void updateWithJetFit(size_t neighbors);
#endif
    void localSystem(const Vector& normal, Vector& u, Vector& v);
    double voronoiWeight(MyMesh::HalfedgeHandle in_he);
    void updateMeanMinMax();
    void updateMeanCurvature();

    // Bezier
    static void bernsteinAll(size_t n, double u, std::vector<double>& coeff);
    void generateMesh(size_t resolution);





    // Visualization
    void setupCamera();
    Vec meanMapColor(double d) const;
    void drawControlNet() const;
    void drawAxes() const;
    void drawAxesWithNames() const;
    static Vec intersectLines(const Vec& ap, const Vec& ad, const Vec& bp, const Vec& bd);

    // Other
    void fairMesh();

    //////////////////////
    // Member variables //
    //////////////////////

    enum class ModelType { NONE, MESH, BEZIER_SURFACE, SKELTON, INVERZ, Bspline } model_type;
    enum class SkelltonType { MAN, WRIST, ARM, FACE } skellton_type;
    // Mesh
    MyMesh mesh;







    double distance(Vec p, Vec p1)
    {
        double len = sqrt(pow(p.x - p1.x, 2) + pow(p.y - p1.y, 2) + pow(p.z - p1.z, 2));

        return len;
    }



    struct ControlPoint {
        Vec position;
        Vec color;
        ControlPoint() {}
        ControlPoint(Vec _position)
        {
            position = _position;
            color = Vec(1, 0, 0);

        }

        void drawarrow()
        {
            Vec const& p = position;
            glPushName(0);
            glRasterPos3fv(p);
            glPopName();

        }
        void draw()
        {
            glDisable(GL_LIGHTING);
            glColor3d(color.x, color.y, color.z);
            glPointSize(50.0);
            glBegin(GL_POINTS);
            glVertex3dv(position);
            glEnd();
            glEnable(GL_LIGHTING);
        }


    };

    ControlPoint target;

    void ininitSkelton();
    void createL(Eigen::SparseMatrix<double>& L);


    float FrameSecond = 0.0;
    QHBoxLayout* hb1 = new QHBoxLayout;
    QLabel* text_ = new QLabel;
    QVBoxLayout* vBox = new QVBoxLayout;

    //std::map<int, double> faceAreaMap;
    std::vector<std::pair<int, double>> sortedVector;
    std::vector<std::pair<int, double>> finalarea;
    std::map< MyMesh::FaceHandle, int> sortedMap;

    // Custom comparator function to sort by values (double) in ascending order
    static bool sortByValue(const std::pair<int, double>& a, const std::pair<int, double>& b) {
        return a.second < b.second;
    }



    struct Ecolleps {
        int id;
        float error;
        MyMesh::HalfedgeHandle h;
        MyMesh::VertexHandle v;
        MyMesh::VertexHandle v2;
        MyMesh::Point p;
        MyMesh::Point p_deleted;
        std::vector<MyMesh::VertexHandle> vh;
        MyMesh::VertexHandle vl;
        MyMesh::VertexHandle vr;
        bool used = false;

        Ecolleps(int id_, float error_, MyMesh::HalfedgeHandle h_, MyMesh::VertexHandle _v,
            MyMesh::Point _v2, MyMesh::Point _p, std::vector<MyMesh::VertexHandle> _vh)
        {
            id = id_;
            error = error_;
            h = h_;
            v = _v;
            p = _v2;
            p_deleted = _p;
            vh = _vh;
        }
    };

    static bool sortByError(const Ecolleps& a, const Ecolleps& b) {
        return a.error < b.error;
    }


 

    float Calculate_spline(MyMesh::VertexHandle p1, MyMesh::Point newp);


    bool _homework = false;
    double median_of_area = 0.0;

    void homework()
    {
        _homework = true;
        int size = 0;
        for (auto f : mesh.faces()) {
            double area = mesh.calc_sector_area(mesh.halfedge_handle(f));
            sortedVector.push_back({ f.idx(), area });
            size++;
            auto d = mesh.calc_face_normal(f);
            auto r = d | d;
            MyMesh::Normal n;
            if (n < n) {}

        }
        std::sort(sortedVector.begin(), sortedVector.end(), sortByValue);
        int partSize = sortedVector.size() / 4;

        std::vector<std::pair<int, double>>::iterator begin = sortedVector.begin();
        std::vector<std::pair<int, double>>::iterator end = sortedVector.begin() + partSize;

        std::vector<std::pair<int, double>> part1(begin, end);

        begin = end;
        end = sortedVector.begin() + 2 * partSize;

        std::vector<std::pair<int, double>> part2(begin, end);

        begin = end;
        end = sortedVector.begin() + 3 * partSize;

        std::vector<std::pair<int, double>> part3(begin, end);

        std::vector<std::pair<int, double>> part4(end, sortedVector.end());

        finalarea.reserve(part2.size() + part3.size());

        finalarea.insert(finalarea.end(), part2.begin(), part2.end());

        finalarea.insert(finalarea.end(), part3.begin(), part3.end());

        median_of_area = median(sortedVector);
        for (const auto& entry : finalarea) {
            sortedMap[mesh.face_handle(entry.first)] = entry.first;
        }
    }

    double median(const std::vector<std::pair<int, double>>& numbers)
    {
        int size = numbers.size();
        if (size % 2 == 0) {
            double middle1 = numbers[size / 2 - 1].second;
            double middle2 = numbers[size / 2].second;
            return (middle1 + middle2) / 2.0;
        }
        else {
            return numbers[size / 2].second;
        }
    }










    std::vector<Ecolleps> use;
    void move(std::vector<Vec> newp, std::vector<Vec> old);

    bool is_still_ok();

    int elapsedTime;
    std::vector<int>indexes;
    std::vector<Vec> points;



 
    Vec rotation;

    // Bezier
    size_t degree[2];
    std::vector<Vec> control_points;

    float currentTime() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::duration<float>>(duration).count();
    }


    std::vector<Vec> ik;
    void IK_matrices();
    double sum_len();
    /// <summary>
    /// 
    /// </summary>
    /// <param name="edgeHandle"></param>

    void collapseEdge(MyMesh::HalfedgeHandle h,int i);
    float ErrorDistance(MyMesh::VertexHandle p1, MyMesh::VertexHandle p2, MyMesh::Point newp);
    float Calculate_Min(MyMesh::VertexHandle p1, MyMesh::Point newp);
    std::vector<Ecolleps> Edgecolleps;
    int kell = 0;
    void Calculate_collapses(MyMesh::HalfedgeHandle h);
    void VertexSplit(Ecolleps e);
    std::vector<MyMesh::FaceHandle> getFaces(MyMesh::VertexHandle p1, MyMesh::VertexHandle p2) {
        // Get all faces connected to point1
        std::vector<MyMesh::FaceHandle> connected_faces;
        for (auto fv_it = mesh.vf_iter(p1); fv_it.is_valid(); ++fv_it) {
            connected_faces.push_back(fv_it.handle());
        }
        // Filter out faces connected to point2
        std::vector<MyMesh::FaceHandle> result_faces;
        for (const auto& face : connected_faces) {
            bool connected_to_point2 = false;
            for (auto fv_it = mesh.fv_iter(face); fv_it.is_valid(); ++fv_it) {
                if (fv_it.handle() == p2) {
                    connected_to_point2 = true;
                    break;
                }
            }

            if (!connected_to_point2) {
                result_faces.push_back(face);
            }
        }

        return result_faces;
    }






    std::vector<MyMesh::VertexHandle> getVertex(MyMesh::VertexHandle p1, MyMesh::VertexHandle p2) {
        // Get all faces connected to point1
        std::vector<MyMesh::FaceHandle> connected_faces;
        for (auto fv_it = mesh.vf_iter(p1); fv_it.is_valid(); ++fv_it) {
            connected_faces.push_back(fv_it.handle());
        }
        // Filter out faces connected to point2
        std::vector<MyMesh::FaceHandle> result_faces;
        for (const auto& face : connected_faces) {
            bool connected_to_point2 = false;
            for (auto fv_it = mesh.fv_iter(face); fv_it.is_valid(); ++fv_it) {
                if (fv_it.handle() == p2) {
                    connected_to_point2 = true;
                    break;
                }
            }

            if (connected_to_point2) {
                result_faces.push_back(face);
            }
        }
        std::vector<MyMesh::VertexHandle> result;

        for (auto f : result_faces)
        {
            for (auto v : mesh.fv_range(f))
            {
                if (v != p1 && v != p2)
                {
                    result.push_back(v);
                    break;
                }
            }
        }

        return result;
    }
    void putVertexes(MyMesh::VertexHandle p1, MyMesh::VertexHandle p2, int i);

    MyMesh::Point nt;
    MyMesh::Point roundPoint(MyMesh::Point p)
    {
        float x = p[0];
        x = (int)(x * 10000 + .5);
        x = x / 10000;

        float y = p[1];
        y = (int)(y * 10000 + .5);
        y = y / 10000;

        float z = p[2];
        z = (int)(z * 10000 + .5);
        z = z / 10000;
        MyMesh::Point res(x, y, z);
        return res;

    }
    void setupCameraBone();
    bool mehet = false;
    bool isweight = false;
    // Visualization
    double mean_min, mean_max, cutoff_ratio;
    bool show_control_points, show_solid, show_wireframe, show_skelton;
    enum class Visualization { PLAIN, MEAN, SLICING, ISOPHOTES, WEIGH, WEIGH2 } visualization;
    GLuint isophote_texture, environment_texture, current_isophote_texture, slicing_texture;
    Vector slicing_dir;
    double slicing_scaling;
    int selected_vertex;
    struct ModificationAxes {
        bool shown;
        float size;
        int selected_axis;
        Vec position, grabbed_pos, original_pos;
    } axes;
    std::string last_filename;
    std::ofstream of;
};
#include "MyViewer.hpp"
