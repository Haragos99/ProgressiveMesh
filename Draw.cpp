#include "MyViewer.h"



void MyViewer::draw() {

   /* drawText(10, int(1.5 * ((QApplication::font().pixelSize() > 0)
        ? QApplication::font().pixelSize()
        : QApplication::font().pointSize())),
        QString("Frame:") + QString(std::to_string(FrameSecond).c_str()));*/

    if (model_type == ModelType::BEZIER_SURFACE && show_control_points)
        drawControlNet();




    glDisable(GL_LIGHTING);
    glColor3d(1, 0, 0);
    glPointSize(50.0);
    glBegin(GL_POINTS);
    glVertex3dv(nt.data());
    glEnd();
    glEnable(GL_LIGHTING);

    if (model_type == ModelType::Bspline&& show_control_points)
        bs.draw();

    if (transparent) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else {
        glDisable(GL_BLEND);
    }

    if (show_skelton)
        if (!axes.shown)
        {
            sk.makefalse(sk);
        }
    
    if (model_type == ModelType::SKELTON|| model_type == ModelType::INVERZ)
    {
        drawSkleton();
        target.draw();
    }
    glPolygonMode(GL_FRONT_AND_BACK, !show_solid && show_wireframe ? GL_LINE : GL_FILL);
    //glEnable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1, 1);
    glLineWidth(1.0);
    if (show_solid || show_wireframe) {
        if (visualization == Visualization::PLAIN)
            glColor3d(1.0, 1.0, 1.0);
        else if (visualization == Visualization::ISOPHOTES) {
            glBindTexture(GL_TEXTURE_2D, current_isophote_texture);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
            glEnable(GL_TEXTURE_2D);
            glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
            glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
            glEnable(GL_TEXTURE_GEN_S);
            glEnable(GL_TEXTURE_GEN_T);
        }
        else if (visualization == Visualization::SLICING) {
            glBindTexture(GL_TEXTURE_1D, slicing_texture);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
            glEnable(GL_TEXTURE_1D);
        }

        for (auto f : mesh.faces()) {
            glBegin(GL_POLYGON);
            for (auto v : mesh.fv_range(f)) {
                if (visualization == Visualization::MEAN)
                    glColor3dv(meanMapColor(mesh.data(v).mean));
                else if (visualization == Visualization::SLICING)
                    glTexCoord1d(mesh.point(v) | slicing_dir * slicing_scaling);
                else if (visualization == Visualization::WEIGH) //Itt adjuk meg a s�lyokat
                {

                    Vec color = Vec(0, 0, 0);
                    for (int i = 0; i < b.size(); i++)
                    {
                        if (mesh.data(v).weigh[i] != 0)
                        {
                            color += (mesh.data(v).weigh[i] * b[i].getColor());

                        }



                    }
                    if (transparent) {
                        glColor4d(color.x, color.y, color.z, 0.5);
                    }
                    else {
                        glColor3d(color.x,color.y,color.z);
                    }
                }
                else if (visualization == Visualization::WEIGH2)
                {
                    if (wi == b.size())
                    {
                        wi = 0;
                    }
                    Vec color = mesh.data(v).weigh[wi] * (b[wi].getColor());

                    if (transparent) {
                        glColor4d(color.x, color.y, color.z, 0.5);
                    }
                    else {
                        glColor3d(color.x, color.y, color.z);
                    }
                }
                if (_homework)
                {
                    bool found = false;
                    for (const auto& entry : sortedMap) {
                        if (entry.first == f) {
                            found = true;
                            glColor3d(1,0,0);
                            break; // Value found, exit the loop
                        }
                        else
                        {
                            glColor3d(0, 1, 0);
                        }
                    }
                }
                glNormal3dv(mesh.normal(v).data());
                glVertex3dv(mesh.point(v).data());


            }
            glEnd();
        }
        if (visualization == Visualization::ISOPHOTES) {
            glDisable(GL_TEXTURE_GEN_S);
            glDisable(GL_TEXTURE_GEN_T);
            glDisable(GL_TEXTURE_2D);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        }
        else if (visualization == Visualization::SLICING) {
            glDisable(GL_TEXTURE_1D);
        }
    }

    if (show_solid && show_wireframe) {
        glPolygonMode(GL_FRONT, GL_LINE);
        glColor3d(0.0, 0.0, 0.0);
        glDisable(GL_LIGHTING);
        for (auto f : mesh.faces()) {
            glBegin(GL_POLYGON);
            for (auto v : mesh.fv_range(f))
                glVertex3dv(mesh.point(v).data());
            glEnd();
        }
        glEnable(GL_LIGHTING);
    }


    if (axes.shown)
        drawAxes();
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glColor3d(1.0, 1.0, 1.0);
    drawText(10, int(1.5 * ((QApplication::font().pixelSize() > 0)
        ? QApplication::font().pixelSize()
        : QApplication::font().pointSize())),
        QString("Index:") + QString(std::to_string(kell).c_str()));
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
}

/// <summary>
/// ezt rajzoljuk ku
/// </summary>
void MyViewer::drawSkleton()
{

    glPointSize(30.0);
    glColor3d(0.3, 0.0, 1.0);
    glLineWidth(200.0);
    int i = 0;

    for (const auto& p : b)
    {
        if (i < b.size())
        {
            glBegin(GL_LINES);
            if (transparent)
            {
                double color = 0.8;
                glColor3d(p.x* color, p.y* color, p.z* color);
            }
            else
            {
                double color = 0.8;
                glColor3d(p.x, p.y, p.z);
            }
           
            glVertex3dv(p.start);
            glVertex3dv(p.End);
            glEnd();
        }
        i++;

    }

    sk.drawchild(sk);

}


void MyViewer::drawControlNet() const {
    glDisable(GL_LIGHTING);
    glLineWidth(3.0);
    glColor3d(0.3, 0.3, 1.0);
    size_t m = degree[1] + 1;
    for (size_t k = 0; k < 2; ++k)
        for (size_t i = 0; i <= degree[k]; ++i) {
            glBegin(GL_LINE_STRIP);
            for (size_t j = 0; j <= degree[1 - k]; ++j) {
                size_t const index = k ? j * m + i : i * m + j;
                const auto& p = control_points[index];
                glVertex3dv(p);
            }
            glEnd();
        }
    glLineWidth(1.0);
    glPointSize(8.0);
    glColor3d(1.0, 0.0, 1.0);
    glBegin(GL_POINTS);
    for (const auto& p : control_points)
        glVertex3dv(p);
    glEnd();
    glPointSize(1.0);
    glEnable(GL_LIGHTING);
}