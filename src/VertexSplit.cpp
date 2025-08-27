#include "MyViewer.h"



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

    }
    int w = 0;
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