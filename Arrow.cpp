#include "MyViewer.h"


void MyViewer::drawAxes() const {
    const Vec& p = axes.position;
    glColor3d(1.0, 0.0, 0.0);
    drawArrow(p, p + Vec(axes.size, 0.0, 0.0), axes.size / 50.0);
    glColor3d(0.0, 1.0, 0.0);
    drawArrow(p, p + Vec(0.0, axes.size, 0.0), axes.size / 50.0);
    glColor3d(0.0, 0.0, 1.0);
    drawArrow(p, p + Vec(0.0, 0.0, axes.size), axes.size / 50.0);
    glEnd();
}

void MyViewer::drawWithNames() {
    if (axes.shown)
        return drawAxesWithNames();

    switch (model_type) {
    case ModelType::NONE: break;
    case ModelType::MESH:
        if (!show_wireframe)
            return;
        for (auto v : mesh.vertices()) {
            glPushName(v.idx());
            glRasterPos3dv(mesh.point(v).data());
            glPopName();
        }
        break;
    case ModelType::Bspline:
        bs.arrow();
        break;
    case ModelType::BEZIER_SURFACE:
        if (!show_control_points)
            return;
       
        
        break;

    case  ModelType::INVERZ:
        target.drawarrow();
        break;
    case  ModelType::SKELTON:
        sk.drawarrow(sk);
        break;
    }
}

void MyViewer::drawAxesWithNames() const {
    const Vec& p = axes.position;
    glPushName(0);
    drawArrow(p, p + Vec(axes.size, 0.0, 0.0), axes.size / 50.0);
    glPopName();
    glPushName(1);
    drawArrow(p, p + Vec(0.0, axes.size, 0.0), axes.size / 50.0);
    glPopName();
    glPushName(2);
    drawArrow(p, p + Vec(0.0, 0.0, axes.size), axes.size / 50.0);
    glPopName();
}

void MyViewer::postSelection(const QPoint& p) {
    int sel = selectedName();
    if (sel == -1) {
        axes.shown = false;
        return;
    }

    if (axes.shown) {
        axes.selected_axis = sel;
        bool found;
        axes.grabbed_pos = camera()->pointUnderPixel(p, found);
        axes.original_pos = axes.position;
        if (!found)
            axes.shown = false;
        return;
    }

    selected_vertex = sel;

    if (model_type == ModelType::Bspline)
        axes.position = bs.control_points[sel];
    if (model_type == ModelType::MESH)
        axes.position = Vec(mesh.point(MyMesh::VertexHandle(sel)).data());
    if (model_type == ModelType::BEZIER_SURFACE)
        axes.position = control_points[sel];
    if (model_type == ModelType::SKELTON)
    {
        sk.makefalse(sk);
        Tree* t = sk.searchbyid(sk, sel);
        sk.maketrue(*t);
        axes.position = t->point;
        drawSkleton();
       
    }
    if (model_type == ModelType::INVERZ)
    {
        axes.position = target.position;
    }
    double depth = camera()->projectedCoordinatesOf(axes.position)[2];
    Vec q1 = camera()->unprojectedCoordinatesOf(Vec(0.0, 0.0, depth));
    Vec q2 = camera()->unprojectedCoordinatesOf(Vec(width(), height(), depth));
    axes.size = (q1 - q2).norm() / 10.0;
    axes.shown = true;
    axes.selected_axis = -1;
}