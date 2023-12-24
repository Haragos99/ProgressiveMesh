#include <algorithm>
#include <cmath>
#include <map>



#ifdef BETTER_MEAN_CURVATURE
#include "Eigen/Eigenvalues"
#include "Eigen/Geometry"
#include "Eigen/LU"
#include "Eigen/SVD"
#endif

#ifdef USE_JET_FITTING
#include "jet-wrapper.h"
#endif

#include "MyViewer.h"

#ifdef _WIN32
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_BGRA 0x80E1
#endif

MyViewer::MyViewer(QWidget *parent) :
  QGLViewer(parent), model_type(ModelType::NONE),
  mean_min(0.0), mean_max(0.0), cutoff_ratio(0.05),
  show_control_points(true), show_solid(true), show_wireframe(false), show_skelton(false),
  visualization(Visualization::PLAIN), slicing_dir(0, 0, 1), slicing_scaling(1),
  last_filename("")
{
    //QDir* logdir = new QDir();
    //logdir->mkdir("logs");
    //QString log_path("logs/log_");
    //log_path += QDateTime::currentDateTime().toString("yyyy-MM-dd__hh-mm-ss-zzz") + ".txt";
    //std::ofstream of = std::ofstream(log_path.toStdString());
    of = std::ofstream("log.txt");
    std::cerr.rdbuf(of.rdbuf());
    omerr().rdbuf(of.rdbuf());
  setSelectRegionWidth(10);
  setSelectRegionHeight(10);
  axes.shown = false;


  //generateBSMesh(50);

}

MyViewer::~MyViewer() {
  glDeleteTextures(1, &isophote_texture);
  glDeleteTextures(1, &environment_texture);
  glDeleteTextures(1, &slicing_texture);
}

void MyViewer::updateMeanMinMax() {
  size_t n = mesh.n_vertices();
  if (n == 0)
    return;

  std::vector<double> mean;
  mean.reserve(n);
  for (auto v : mesh.vertices())
    mean.push_back(mesh.data(v).mean);

  std::sort(mean.begin(), mean.end());
  size_t k = (double)n * cutoff_ratio;
  mean_min = std::min(mean[k ? k-1 : 0], 0.0);
  mean_max = std::max(mean[k ? n-k : n-1], 0.0);
}

void MyViewer::localSystem(const MyViewer::Vector &normal,
                           MyViewer::Vector &u, MyViewer::Vector &v) {
  // Generates an orthogonal (u,v) coordinate system in the plane defined by `normal`.
  int maxi = 0, nexti = 1;
  double max = std::abs(normal[0]), next = std::abs(normal[1]);
  if (max < next) {
    std::swap(max, next);
    std::swap(maxi, nexti);
  }
  if (std::abs(normal[2]) > max) {
    nexti = maxi;
    maxi = 2;
  } else if (std::abs(normal[2]) > next)
    nexti = 2;

  u.vectorize(0.0);
  u[nexti] = -normal[maxi];
  u[maxi] = normal[nexti];
  u /= u.norm();
  v = normal % u;
}

double MyViewer::voronoiWeight(MyViewer::MyMesh::HalfedgeHandle in_he) {
  // Returns the area of the triangle bounded by in_he that is closest
  // to the vertex pointed to by in_he.
  if (mesh.is_boundary(in_he))
    return 0;
  auto next = mesh.next_halfedge_handle(in_he);
  auto prev = mesh.prev_halfedge_handle(in_he);
  double c2 = mesh.calc_edge_vector(in_he).sqrnorm();
  double b2 = mesh.calc_edge_vector(next).sqrnorm();
  double a2 = mesh.calc_edge_vector(prev).sqrnorm();
  double alpha = mesh.calc_sector_angle(in_he);

  if (a2 + b2 < c2)                // obtuse gamma
    return 0.125 * b2 * std::tan(alpha);
  if (a2 + c2 < b2)                // obtuse beta
    return 0.125 * c2 * std::tan(alpha);
  if (b2 + c2 < a2) {              // obtuse alpha
    double b = std::sqrt(b2), c = std::sqrt(c2);
    double total_area = 0.5 * b * c * std::sin(alpha);
    double beta  = mesh.calc_sector_angle(prev);
    double gamma = mesh.calc_sector_angle(next);
    return total_area - 0.125 * (b2 * std::tan(gamma) + c2 * std::tan(beta));
  }

  double r2 = 0.25 * a2 / std::pow(std::sin(alpha), 2); // squared circumradius
  auto area = [r2](double x2) {
    return 0.125 * std::sqrt(x2) * std::sqrt(std::max(4.0 * r2 - x2, 0.0));
  };
  return area(b2) + area(c2);
}

#ifndef BETTER_MEAN_CURVATURE
void MyViewer::updateMeanCurvature() {
  std::map<MyMesh::FaceHandle, double> face_area;
  std::map<MyMesh::VertexHandle, double> vertex_area;

  for (auto f : mesh.faces())
    face_area[f] = mesh.calc_sector_area(mesh.halfedge_handle(f));

  // Compute triangle strip areas
  for (auto v : mesh.vertices()) {
    vertex_area[v] = 0;
    mesh.data(v).mean = 0;
    for (auto f : mesh.vf_range(v))
      vertex_area[v] += face_area[f];
    vertex_area[v] /= 3.0;
  }

  // Compute mean values using dihedral angles
  for (auto v : mesh.vertices()) {
    for (auto h : mesh.vih_range(v)) {
      auto vec = mesh.calc_edge_vector(h);
      double angle = mesh.calc_dihedral_angle(h); // signed; returns 0 at the boundary
      mesh.data(v).mean += angle * vec.norm();
    }
    mesh.data(v).mean *= 0.25 / vertex_area[v];
  }
}
#else // BETTER_MEAN_CURVATURE
void MyViewer::updateMeanCurvature() {
  // As in the paper:
  //   S. Rusinkiewicz, Estimating curvatures and their derivatives on triangle meshes.
  //     3D Data Processing, Visualization and Transmission, IEEE, 2004.

  std::map<MyMesh::VertexHandle, Vector> efgp; // 2nd principal form
  std::map<MyMesh::VertexHandle, double> wp;   // accumulated weight

  // Initial setup
  for (auto v : mesh.vertices()) {
    efgp[v].vectorize(0.0);
    wp[v] = 0.0;
  }

  for (auto f : mesh.faces()) {
    // Setup local edges, vertices and normals
    auto h0 = mesh.halfedge_handle(f);
    auto h1 = mesh.next_halfedge_handle(h0);
    auto h2 = mesh.next_halfedge_handle(h1);
    auto e0 = mesh.calc_edge_vector(h0);
    auto e1 = mesh.calc_edge_vector(h1);
    auto e2 = mesh.calc_edge_vector(h2);
    auto n0 = mesh.normal(mesh.to_vertex_handle(h1));
    auto n1 = mesh.normal(mesh.to_vertex_handle(h2));
    auto n2 = mesh.normal(mesh.to_vertex_handle(h0));

    Vector n = mesh.normal(f), u, v;
    localSystem(n, u, v);

    // Solve a LSQ equation for (e,f,g) of the face
    Eigen::MatrixXd A(6, 3);
    A << (e0 | u), (e0 | v),    0.0,
            0.0,   (e0 | u), (e0 | v),
         (e1 | u), (e1 | v),    0.0,
            0.0,   (e1 | u), (e1 | v),
         (e2 | u), (e2 | v),    0.0,
            0.0,   (e2 | u), (e2 | v);
    Eigen::VectorXd b(6);
    b << ((n2 - n1) | u),
         ((n2 - n1) | v),
         ((n0 - n2) | u),
         ((n0 - n2) | v),
         ((n1 - n0) | u),
         ((n1 - n0) | v);
    Eigen::Vector3d x = A.fullPivLu().solve(b);

    Eigen::Matrix2d F;          // Fundamental matrix for the face
    F << x(0), x(1),
         x(1), x(2);

    for (auto h : mesh.fh_range(f)) {
      auto p = mesh.to_vertex_handle(h);

      // Rotate the (up,vp) local coordinate system to be coplanar with that of the face
      Vector np = mesh.normal(p), up, vp;
      localSystem(np, up, vp);
      auto axis = (np % n).normalize();
      double angle = std::acos(std::min(std::max(n | np, -1.0), 1.0));
      auto rotation = Eigen::AngleAxisd(angle, Eigen::Vector3d(axis.data()));
      Eigen::Vector3d up1(up.data()), vp1(vp.data());
      up1 = rotation * up1;    vp1 = rotation * vp1;
      up = Vector(up1.data()); vp = Vector(vp1.data());

      // Compute the vertex-local (e,f,g)
      double e, f, g;
      Eigen::Vector2d upf, vpf;
      upf << (up | u), (up | v);
      vpf << (vp | u), (vp | v);
      e = upf.transpose() * F * upf;
      f = upf.transpose() * F * vpf;
      g = vpf.transpose() * F * vpf;

      // Accumulate the results with Voronoi weights
      double w = voronoiWeight(h);
      efgp[p] += Vector(e, f, g) * w;
      wp[p] += w;
    }
  }

  // Compute the principal curvatures
  for (auto v : mesh.vertices()) {
    auto &efg = efgp[v];
    efg /= wp[v];
    Eigen::Matrix2d F;
    F << efg[0], efg[1],
         efg[1], efg[2];
    auto k = F.eigenvalues();   // always real, because F is a symmetric real matrix
    mesh.data(v).mean = (k(0).real() + k(1).real()) / 2.0;
  }
}
#endif

static Vec HSV2RGB(Vec hsv) {
  // As in Wikipedia
  double c = hsv[2] * hsv[1];
  double h = hsv[0] / 60;
  double x = c * (1 - std::abs(std::fmod(h, 2) - 1));
  double m = hsv[2] - c;
  Vec rgb(m, m, m);
  if (h <= 1)
    return rgb + Vec(c, x, 0);
  if (h <= 2)
    return rgb + Vec(x, c, 0);
  if (h <= 3)
    return rgb + Vec(0, c, x);
  if (h <= 4)
    return rgb + Vec(0, x, c);
  if (h <= 5)
    return rgb + Vec(x, 0, c);
  if (h <= 6)
    return rgb + Vec(c, 0, x);
  return rgb;
}


static Vec RGB2HSV(Vec rgb) {

    Vec hsv;
    double min, max, delta;
    double r, g, b;
    r = rgb[0];
    g = rgb[1];
    b = rgb[2];
    min = r < g ? r : g;
    min = min < b ? min : b;

    max = r > g ? r : g;
    max = max > b ? max : b;

    delta = max - min;
    hsv.z = max;
    if (delta < 0.00001)
    {
        hsv.x = 0;
        hsv.y = 0;
        return hsv;
    }
    if (max > 0.0)
    {
        hsv.y = (delta / max);
    }
    else
    {
        hsv.y = 0;
        hsv.x = NAN;
        return hsv;
    }
    if (r >= max)
    {
        hsv.x = (g - b) / delta;
    }
    else
        if (g >= max)
            hsv.x = 2.0 + (b - r) / delta; 
        else
            hsv.x = 4.0 + (r - g) / delta; 

    hsv.x *= 60.0;                            

    if (hsv.x < 0.0)
        hsv.x += 360.0;

    return hsv;
}


Vec MyViewer::meanMapColor(double d) const {
  double red = 0, green = 120, blue = 240; // Hue
  if (d < 0) {
    double alpha = mean_min ? std::min(d / mean_min, 1.0) : 1.0;
    return HSV2RGB({green * (1 - alpha) + blue * alpha, 1, 1});
  }
  double alpha = mean_max ? std::min(d / mean_max, 1.0) : 1.0;
  return HSV2RGB({green * (1 - alpha) + red * alpha, 1, 1});
}

void MyViewer::fairMesh() {
  if (model_type != ModelType::MESH)
    return;

  emit startComputation(tr("Fairing mesh..."));
  OpenMesh::Smoother::JacobiLaplaceSmootherT<MyMesh> smoother(mesh);
  smoother.initialize(OpenMesh::Smoother::SmootherT<MyMesh>::Normal, // or: Tangential_and_Normal
                      OpenMesh::Smoother::SmootherT<MyMesh>::C1);
  for (size_t i = 1; i <= 10; ++i) {
    smoother.smooth(10);
    emit midComputation(i * 10);
  }
  updateMesh(false);
  emit endComputation();
}

#ifdef USE_JET_FITTING

void MyViewer::updateWithJetFit(size_t neighbors) {
  std::vector<Vector> points;
  for (auto v : mesh.vertices())
    points.push_back(mesh.point(v));

  auto nearest = JetWrapper::Nearest(points, neighbors);

  for (auto v : mesh.vertices()) {
    auto jet = JetWrapper::fit(mesh.point(v), nearest, 2);
    if ((mesh.normal(v) | jet.normal) < 0) {
      mesh.set_normal(v, -jet.normal);
      mesh.data(v).mean = (jet.k_min + jet.k_max) / 2;
    } else {
      mesh.set_normal(v, jet.normal);
      mesh.data(v).mean = -(jet.k_min + jet.k_max) / 2;
    }
  }
}

#endif // USE_JET_FITTING

void MyViewer::updateVertexNormals() {
  // Weights according to:
  //   N. Max, Weights for computing vertex normals from facet normals.
  //     Journal of Graphics Tools, Vol. 4(2), 1999.
  for (auto v : mesh.vertices()) {
    Vector n(0.0, 0.0, 0.0);
    for (auto h : mesh.vih_range(v)) {
      if (mesh.is_boundary(h))
        continue;
      auto in_vec  = mesh.calc_edge_vector(h);
      auto out_vec = mesh.calc_edge_vector(mesh.next_halfedge_handle(h));
      double w = in_vec.sqrnorm() * out_vec.sqrnorm();
      n += (in_vec % out_vec) / (w == 0.0 ? 1.0 : w);
    }
    double len = n.length();
    if (len != 0.0)
      n /= len;
    mesh.set_normal(v, n);
  }
}

void MyViewer::updateMesh(bool update_mean_range) {
  if (model_type == ModelType::BEZIER_SURFACE)
    generateMesh(50);

  mesh.request_face_normals(); mesh.request_vertex_normals();
  mesh.update_face_normals();
#ifdef USE_JET_FITTING
  mesh.update_vertex_normals();
  updateWithJetFit(20);
#else // !USE_JET_FITTING
  updateVertexNormals();
  updateMeanCurvature();
#endif
  if (update_mean_range)
    updateMeanMinMax();
}


void MyViewer::setupCameraBone() {
    // Set camera on the model
    Vector box_min, box_max;
    box_min = box_max = Vector(points.front().x, points.front().y, points.front().z);
    for (auto v : points) {
        box_min.minimize(Vector(v.x, v.y, v.z));
        box_max.maximize(Vector(v.x, v.y, v.z));
    }
    camera()->setSceneBoundingBox(Vec(box_min.data()), Vec(box_max.data()));
    camera()->showEntireScene();

    slicing_scaling = 20 / (box_max - box_min).max();

    setSelectedName(-1);
    axes.shown = false;

    update();
}


void MyViewer::setupCamera() {
  // Set camera on the model
  Vector box_min, box_max;
  box_min = box_max = mesh.point(*mesh.vertices_begin());
  for (auto v : mesh.vertices()) {
    box_min.minimize(mesh.point(v));
    box_max.maximize(mesh.point(v));
  }
  camera()->setSceneBoundingBox(Vec(box_min.data()), Vec(box_max.data()));
  camera()->showEntireScene();

  slicing_scaling = 20 / (box_max - box_min).max();

  setSelectedName(-1);
  axes.shown = false;

  update();
}



void MyViewer::init() {
  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);

  QImage img(":/isophotes.png");
  glGenTextures(1, &isophote_texture);
  glBindTexture(GL_TEXTURE_2D, isophote_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.width(), img.height(), 0, GL_BGRA,
               GL_UNSIGNED_BYTE, img.convertToFormat(QImage::Format_ARGB32).bits());

  QImage img2(":/environment.png");
  glGenTextures(1, &environment_texture);
  glBindTexture(GL_TEXTURE_2D, environment_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img2.width(), img2.height(), 0, GL_BGRA,
               GL_UNSIGNED_BYTE, img2.convertToFormat(QImage::Format_ARGB32).bits());

  glGenTextures(1, &slicing_texture);
  glBindTexture(GL_TEXTURE_1D, slicing_texture);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  static const unsigned char slicing_img[] = { 0b11111111, 0b00011100 };
  glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 2, 0, GL_RGB, GL_UNSIGNED_BYTE_3_3_2, &slicing_img);
}












float MyViewer::ErrorDistance(MyMesh::VertexHandle p1, MyMesh::VertexHandle p2, MyMesh::Point newp)
{
    float result = Calculate_Min(p1, newp) + Calculate_Min(p2, newp);
    return result;
}

float MyViewer::Calculate_Min(MyMesh::VertexHandle p1, MyMesh::Point newp)
{
   
    
    std::vector<float> min;
    for (MyMesh::VertexFaceIter vf_it = mesh.vf_iter(p1); vf_it.is_valid(); ++vf_it) {
        MyMesh::FaceHandle face_handle = *vf_it;
        MyMesh::Normal face_normal = mesh.normal(face_handle);
        MyMesh::Point p = roundPoint(mesh.point(p1));
        MyMesh::Point r = p - roundPoint(newp);
        double min_value = abs(r | face_normal);
        min.push_back(min_value);
        
    }

    auto result = std::min_element(min.begin(), min.end());

    return *result;
}

void MyViewer::collapseEdge(MyMesh::HalfedgeHandle h, int i)
{
    MyMesh::VertexHandle v = mesh.to_vertex_handle(h);
    MyMesh::VertexHandle v2 = mesh.from_vertex_handle(h);
    MyMesh::Point point1 = mesh.point(v);
    MyMesh::Point point2 = mesh.point(v2);
    MyMesh::Point newPoint;
    newPoint = (point1 + point2) / 2.0f;

    if (mesh.is_boundary(v2))
    {
        newPoint = point2;
    }
    //Edgecolleps[i].vh = getVertex(v2, v);
    Edgecolleps[i].v = v;
    for (auto v : Edgecolleps[i].vh)
    {
        auto p = mesh.point(v);
        Edgecolleps[i].po.push_back(p);
    }
    Edgecolleps[i].v2 = v2;
    putVertexes(v2, v, i);
    mesh.collapse(h);
    mesh.set_point(v, newPoint);
}

void MyViewer::Calculate_collapses(MyMesh::HalfedgeHandle h)
{
    MyMesh::VertexHandle v = mesh.to_vertex_handle(h);
    MyMesh::VertexHandle v2 = mesh.from_vertex_handle(h);
    MyMesh::Point point1 = mesh.point(v);
    MyMesh::Point point2 = mesh.point(v2);
    MyMesh::Point newPoint;
    newPoint = (point1 + point2) / 2.0f;
    float error_distance = ErrorDistance(v, v2, newPoint);

    Edgecolleps.push_back(Ecolleps(h.idx(), error_distance, h, getFaces(v2, v), v, point1, point2, getVertex(v2, v)));

}

void MyViewer::VertexSplit(Ecolleps e)
{
    MyMesh::VertexHandle v = e.v;
    mesh.set_point(v, e.p);
    MyMesh::VertexHandle v2 = mesh.add_vertex(e.p_deleted);
    if (e.vh.size() == 1)
    {
        MyMesh::VertexHandle s;
        s.reset();
        e.vh.push_back(s);
    }

    mesh.vertex_split(e.v2, v, e.vl, e.vr);
    //auto faces = e.conected;





}

void  MyViewer::putVertexes(MyMesh::VertexHandle p1, MyMesh::VertexHandle p2, int i) {
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

        int ds = 0;

    }
    int w = 0;
    if (i == 18)
    {
        int u = 0;
    }
    for (auto v : result)
    {
        MyMesh::HalfedgeHandle he = mesh.find_halfedge(p2, v);
        if (mesh.face_handle(he) == result_faces[w]) {
            he = mesh.find_halfedge(p2, v);
            auto v2 = mesh.to_vertex_handle(he);
            auto v3 = mesh.from_vertex_handle(he);

            if (v2 == v)
            {
                Edgecolleps[i].vl = v;
            }

            if (v3 == v)
            {
                Edgecolleps[i].vr = v;
            }
        }
        else {
            he = mesh.find_halfedge(v, p2);
            auto v2 = mesh.to_vertex_handle(he);
            auto v3 = mesh.from_vertex_handle(he);

            if (v2 == v)
            {
                Edgecolleps[i].vl = v;
            }

            if (v3 == v)
            {
                Edgecolleps[i].vr = v;
            }
        }
        w++;
    }


}







void MyViewer::keyPressEvent(QKeyEvent *e) {

    auto dlg = std::make_unique<QDialog>(this);
    auto* hb1 = new QHBoxLayout;
    auto* vb = new QVBoxLayout;
    QLabel* text;
    int sizek; 

  if (e->modifiers() == Qt::NoModifier)

    switch (e->key()) {
    case Qt::Key_R:
      if (model_type == ModelType::MESH)
        openMesh(last_filename, false);
      else if (model_type == ModelType::BEZIER_SURFACE)
        openBezier(last_filename, false);
      update();
      break;
    case Qt::Key_O:
      if (camera()->type() == qglviewer::Camera::PERSPECTIVE)
        camera()->setType(qglviewer::Camera::ORTHOGRAPHIC);
      else
        camera()->setType(qglviewer::Camera::PERSPECTIVE);
      update();
      break;
    case Qt::Key_7:
        Edgecolleps.clear();
        kell = 0;
        target = ControlPoint(Vec(0.1, 0, 0));
        for (auto h : mesh.halfedges()) {
            if (mesh.is_collapse_ok(h))

                Calculate_collapses(h);
            //if (h.idx() == 100) break;


        }
        std::sort(Edgecolleps.begin(), Edgecolleps.end(), sortByError);
        QMessageBox::information(this, "EColeps", "EdgeColleps is calculated ");
        break;

    case Qt::Key_6:
        sizek = Edgecolleps.size() / 5;

        for (int i = sizek - 1; i >= 0; i--)
        {
            if (Edgecolleps[i].used) {
                VertexSplit(Edgecolleps[i]);
                Edgecolleps[i].used = false;
            }
            
        }
        QMessageBox::information(this, "EColeps", "DONE ");
        mesh.request_face_normals();
        mesh.request_vertex_normals();
        mesh.update_face_normals();
        update();
        break;
    case Qt::Key_P:
      //visualization = Visualization::PLAIN;
        sizek = Edgecolleps.size() / 2;

        
        sizek = Edgecolleps.size() / 5;
        for (int i = 0; i < sizek;i++)
        {
            if (mesh.is_collapse_ok(Edgecolleps[i].h)){
                collapseEdge(Edgecolleps[i].h,i);
                Edgecolleps[i].used = true;
            }
        }
       // mesh.garbage_collection();
        mesh.request_face_normals();
        mesh.request_vertex_normals();
        mesh.update_face_normals();
        

        update();
      break;
    case Qt::Key_M:
      visualization = Visualization::MEAN;
      update();
      break;
    case Qt::Key_L:
      visualization = Visualization::SLICING;
 
      update();
      break;
    case Qt::Key_V:
        visualization = Visualization::SLICING;
        //transparent = !transparent;
        update();
        break;
 
    case Qt::Key_I:
      //visualization = Visualization::ISOPHOTES;
      model_type = ModelType::INVERZ;
      //current_isophote_texture = isophote_texture;
      update();
      break;
    case Qt::Key_E:
      visualization = Visualization::ISOPHOTES;
      current_isophote_texture = environment_texture;
      update();
      break;
    case Qt::Key_1:
        if (points.size() != 0 && mesh.n_vertices() != 0)
        {
            model_type = ModelType::SKELTON;
            visualization = Visualization::WEIGH2;
            wi++;
        }
        update();
        break;

    case Qt::Key_4:
        if (axes.shown) {
            //Rotate();
        }
        if (kell != -1)
        {

       
            if (Edgecolleps[kell].used  ) {
                VertexSplit(Edgecolleps[kell]);
                Edgecolleps[kell].used = false;
            
            }
            kell--;
        }
        update();
        break;
    case Qt::Key_B:
      show_skelton = !show_skelton;
      update();
      break;

    

   
    

    case Qt::Key_3:

        
        if (mesh.is_collapse_ok(Edgecolleps[kell].h)) {
            collapseEdge(Edgecolleps[kell].h, kell);
            Edgecolleps[kell].used = true;
            
        }
        kell++;


        //keyframe_add();
        //if (mesh.n_vertices() != 0)
        //{
        //    for (auto v : mesh.vertices()) {
        //        mesh.data(v).weigh.clear();
        //    }
        //    model_type = ModelType::MESH;
        //   cruv visualization = Visualization::PLAIN;
        //}
        update();
        break;
    case Qt::Key_C:
      show_control_points = !show_control_points;
      update();
      break;
    case Qt::Key_S:
      show_solid = !show_solid;
      update();
      break;



    case Qt::Key_W:
      show_wireframe = !show_wireframe;
      update();
      break;
    case Qt::Key_F:
      fairMesh();
      update();
      break;
    case Qt::Key_X:

        update();
        break;
    default:
      QGLViewer::keyPressEvent(e);
    }
  else if (e->modifiers() == Qt::KeypadModifier)
    switch (e->key()) {
    case Qt::Key_Plus:
      slicing_scaling *= 2;
      update();
      break;
    case Qt::Key_Minus:
      slicing_scaling /= 2;
      update();
      break;
    case Qt::Key_Asterisk:
      slicing_dir = Vector(static_cast<double *>(camera()->viewDirection()));
      update();
      break;
    } else
    QGLViewer::keyPressEvent(e);
}

Vec MyViewer::intersectLines(const Vec &ap, const Vec &ad, const Vec &bp, const Vec &bd) {
  // always returns a point on the (ap, ad) line
  double a = ad * ad, b = ad * bd, c = bd * bd;
  double d = ad * (ap - bp), e = bd * (ap - bp);
  if (a * c - b * b < 1.0e-7)
    return ap;
  double s = (b * e - c * d) / (a * c - b * b);
  return ap + s * ad;
}

void MyViewer::bernsteinAll(size_t n, double u, std::vector<double> &coeff) {
  coeff.clear(); coeff.reserve(n + 1);
  coeff.push_back(1.0);
  double u1 = 1.0 - u;
  for (size_t j = 1; j <= n; ++j) {
    double saved = 0.0;
    for (size_t k = 0; k < j; ++k) {
      double tmp = coeff[k];
      coeff[k] = saved + tmp * u1;
      saved = tmp * u;
    }
    coeff.push_back(saved);
  }
}

void MyViewer::generateMesh(size_t resolution) {
  mesh.clear();
  std::vector<MyMesh::VertexHandle> handles, tri;
  size_t n = degree[0], m = degree[1];

  std::vector<double> coeff_u, coeff_v;
  for (size_t i = 0; i < resolution; ++i) {
    double u = (double)i / (double)(resolution - 1);
    bernsteinAll(n, u, coeff_u);
    for (size_t j = 0; j < resolution; ++j) {
      double v = (double)j / (double)(resolution - 1);
      bernsteinAll(m, v, coeff_v);
      Vec p(0.0, 0.0, 0.0);
      for (size_t k = 0, index = 0; k <= n; ++k)
        for (size_t l = 0; l <= m; ++l, ++index)
          p += control_points[index] * coeff_u[k] * coeff_v[l];
      handles.push_back(mesh.add_vertex(Vector(static_cast<double *>(p))));
    }
  }
  for (size_t i = 0; i < resolution - 1; ++i)
    for (size_t j = 0; j < resolution - 1; ++j) {
      tri.clear();
      tri.push_back(handles[i * resolution + j]);
      tri.push_back(handles[i * resolution + j + 1]);
      tri.push_back(handles[(i + 1) * resolution + j]);
      mesh.add_face(tri);
      tri.clear();
      tri.push_back(handles[(i + 1) * resolution + j]);
      tri.push_back(handles[i * resolution + j + 1]);
      tri.push_back(handles[(i + 1) * resolution + j + 1]);
      mesh.add_face(tri);
    }
}

void MyViewer::mouseMoveEvent(QMouseEvent *e) {
    if (!axes.shown ||
        (axes.selected_axis < 0 && !(e->modifiers() & Qt::ControlModifier)) ||
        !(e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) ||
        !(e->buttons() & Qt::LeftButton))
    {
        //sk.makefalse(sk);
        return QGLViewer::mouseMoveEvent(e);
    }
  Vec p;
  float d;
  Vec axis(axes.selected_axis == 0, axes.selected_axis == 1, axes.selected_axis == 2);
  Vec old_pos = axes.position;
  if (e->modifiers() & Qt::ControlModifier) {
    // move in screen plane
    double depth = camera()->projectedCoordinatesOf(axes.position)[2];
    axes.position = camera()->unprojectedCoordinatesOf(Vec(e->pos().x(), e->pos().y(), depth));
  } else {
    Vec from, dir;
    camera()->convertClickToLine(e->pos(), from, dir);
    p = intersectLines(axes.grabbed_pos, axis, from, dir);
    d = (p - axes.grabbed_pos) * axis;
    axes.position[axes.selected_axis] = axes.original_pos[axes.selected_axis] + d;
  }

  if (model_type == ModelType::MESH)
    mesh.set_point(MyMesh::VertexHandle(selected_vertex),
                   Vector(static_cast<double *>(axes.position)));
  if (model_type == ModelType::BEZIER_SURFACE)
    control_points[selected_vertex] = axes.position;

 



  if (model_type == ModelType::INVERZ)
  {
      target.position = axes.position;
      double size = Edgecolleps.size() / 5;

      for (int i = 0; i < size; i++)
      {
          auto pd = Edgecolleps[i].p;
          Vec d = Vec(pd[0], pd[1], pd[2]);
          auto dis = distance(d, target.position);
          if (dis > 4)
          {
              if (mesh.is_collapse_ok(Edgecolleps[i].h)) {
                  collapseEdge(Edgecolleps[i].h, i);
                  Edgecolleps[i].used = true;
                  use.push_back(Edgecolleps[i]);
              }
          }

      }
      std::vector<int> remov;

      for (int i = use.size() - 1; i >= 0; i--)
      {
          auto pd = use[i].p;
          Vec d = Vec(pd[0], pd[1], pd[2]);
          auto dis = distance(d, target.position);
          if (use[i].used)
          {
              int k = 0;
          }
          if (dis <= 4)
          {
              if (use[i].used) {
                  MyMesh::HalfedgeHandle vlv1, vrv1;
                  vlv1 = mesh.find_halfedge(use[i].v, use[i].vl);
                  vrv1 = mesh.find_halfedge(use[i].vr, use[i].v);
                  if (vrv1.is_valid() && vlv1.is_valid())
                  {
                      VertexSplit(use[i]);
                      use[i].used = false;
                      remov.push_back(i);
                  }
                 
              }

          }

      }

      
      
  }

  
  //updateMesh();
  update();
}




bool MyViewer::is_still_ok()
{
    double size = Edgecolleps.size() / 5;
    for (int i = 0; i < size; i++)
    {
        auto pd = Edgecolleps[i].p;
        Vec d = Vec(pd[0], pd[1], pd[2]);
        auto dis = distance(d, target.position);
        bool use = Edgecolleps[i].used;
        if (dis <= 6 && use)
        {
            return true;
        }

    }

    return false;
}





QString MyViewer::helpString() const {
  QString text("<h2>Sample Framework</h2>"
               "<p>This is a minimal framework for 3D mesh manipulation, which can be "
               "extended and used as a base for various projects, for example "
               "prototypes for fairing algorithms, or even displaying/modifying "
               "parametric surfaces, etc.</p>"
               "<p>The following hotkeys are available:</p>"
               "<ul>"
               "<li>&nbsp;R: Reload model</li>"
               "<li>&nbsp;O: Toggle orthographic projection</li>"
               "<li>&nbsp;P: Set plain map (no coloring)</li>"
               "<li>&nbsp;M: Set mean curvature map</li>"
               "<li>&nbsp;L: Set slicing map<ul>"
               "<li>&nbsp;+: Increase slicing density</li>"
               "<li>&nbsp;-: Decrease slicing density</li>"
               "<li>&nbsp;*: Set slicing direction to view</li></ul></li>"
               "<li>&nbsp;I: Set isophote line map</li>"
               "<li>&nbsp;E: Set environment texture</li>"
               "<li>&nbsp;C: Toggle control polygon visualization</li>"
               "<li>&nbsp;S: Toggle solid (filled polygon) visualization</li>"
               "<li>&nbsp;W: Toggle wireframe visualization</li>"
               "<li>&nbsp;F: Fair mesh</li>"
               "</ul>"
               "<p>There is also a simple selection and movement interface, enabled "
               "only when the wireframe/controlnet is displayed: a mesh vertex can be selected "
               "by shift-clicking, and it can be moved by shift-dragging one of the "
               "displayed axes. Pressing ctrl enables movement in the screen plane.</p>"
               "<p>Note that libQGLViewer is furnished with a lot of useful features, "
               "such as storing/loading view positions, or saving screenshots. "
               "OpenMesh also has a nice collection of tools for mesh manipulation: "
               "decimation, subdivision, smoothing, etc. These can provide "
               "good comparisons to the methods you implement.</p>"
               "<p>This software can be used as a sample GUI base for handling "
               "parametric or procedural surfaces, as well. The power of "
               "Qt and libQGLViewer makes it easy to set up a prototype application. "
               "Feel free to modify and explore!</p>"
               "<p align=\"right\">Peter Salvi</p>");
  return text;
}
