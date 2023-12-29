#include "MyViewer.h"




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
    Edgecolleps[i].v = v;
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
    Edgecolleps.push_back(Ecolleps(h.idx(), error_distance, h, v, point1, point2, getVertex(v2, v)));

}
