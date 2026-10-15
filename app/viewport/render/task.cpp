#include <boost/python.hpp>
#include <boost/format.hpp>

#include "viewport/render/task.h"
#include "viewport/render/instance.h"

#include "fab/types/shape.h"
#include "fab/util/region.h"
#include "fab/tree/render.h"

RenderTask::RenderTask(RenderInstance* parent, PyObject* s, QMatrix4x4 M,
                       QVector2D clip, int refinement, float dpr)
    : shape(s), M(M), clip(clip), refinement(refinement), dpr(dpr)
{
    Py_INCREF(shape);

    future = QtConcurrent::run(this, &RenderTask::async);
    watcher.setFuture(future);

    connect(&watcher, &decltype(watcher)::finished,
            parent, &RenderInstance::onTaskFinished);
}

RenderTask::~RenderTask()
{
    Py_DECREF(shape);
}

void RenderTask::halt()
{
    halt_flag = 1;
}

RenderTask* RenderTask::getNext(RenderInstance* parent) const
{
    return refinement > 1
        ? new RenderTask(parent, shape, M, clip, refinement - 1, dpr)
        : NULL;
}

void RenderTask::async()
{
    QElapsedTimer timer;
    timer.start();

    boost::python::extract<const Shape&> get_shape(shape);

    Q_ASSERT(get_shape.check());
    const Shape& s = get_shape();

    if (!std::isinf(s.bounds.xmin) && !std::isinf(s.bounds.xmax) &&
        !std::isinf(s.bounds.ymin) && !std::isinf(s.bounds.ymax))
    {
        if (std::isinf(s.bounds.zmin) || std::isinf(s.bounds.zmax))
        {
            render2d(s);
        }
        else
        {
            render3d(s);
        }
    }

    // Set color from shape or to white
    color = (s.r != -1 && s.g != -1 && s.g != -1)
        ? QColor(s.r, s.g, s.b) : QColor(255, 255, 255);

    // Compensate for screen scale
    float scale = sqrt(pow(M(0, 0), 2) +
                       pow(M(0, 1), 2) +
                       pow(M(0, 2), 2));
    size /= scale;

    render_time = timer.elapsed();
}

void RenderTask::render3d(const Shape& s)
{
    Transform T = getTransform(M);
    Shape transformed = s.map(T);

    Bounds b = render(&transformed, transformed.bounds, dpr / float(refinement));

    {   // Apply a transform-less mapping to the bounds
        auto m = M;
        m.setColumn(3, {0, 0, 0, m(3,3)});
        pos = m.inverted() * QVector3D(
                (b.xmin + b.xmax)/2,
                (b.ymin + b.ymax)/2,
                (b.zmin + b.zmax)/2);
    }

    size = {b.xmax - b.xmin,
            b.ymax - b.ymin,
            b.zmax - b.zmin};

    flat = false;
}

void RenderTask::render2d(const Shape& s)
{
    QMatrix4x4 matrix_flat = M;
    matrix_flat(0, 2) = 0;
    matrix_flat(1, 2) = 0;
    matrix_flat(2, 0) = 0;
    matrix_flat(2, 1) = 0;
    matrix_flat(2, 2) = 1;

    Shape s_flat(s.math, Bounds(s.bounds.xmin, s.bounds.ymin, 0,
                                s.bounds.xmax, s.bounds.ymax, 0));

    Transform T_flat = getTransform(matrix_flat);
    Shape transformed = s_flat.map(T_flat);

    // Render the flattened shape, but with bounds equivalent to the shape's
    // position in a 3D bounding box.
    Bounds b3d_ = Bounds(s.bounds.xmin, s.bounds.ymin, 0,
                         s.bounds.xmax, s.bounds.ymax, 0.0001).
                 map(getTransform(M));

    Bounds b3d = render(&transformed, b3d_, dpr / float(refinement));

    {   // Apply a transform-less mapping to the bounds
        auto m = M;
        m.setColumn(3, {0, 0, 0, m(3, 3)});
        pos = m.inverted() *
              QVector3D((b3d.xmin + b3d.xmax)/2,
                        (b3d.ymin + b3d.ymax)/2,
                        (b3d.zmin + b3d.zmax)/2);
    }

    size = {b3d.xmax - b3d.xmin,
            b3d.ymax - b3d.ymin,
            b3d.zmax - b3d.zmin};

    // Apply a gradient to the depth-map based on tilt
    if (M(1,2))
    {
        bool direction = M(2,2) > 0;
        for (int j=0; j < depth.height(); ++j)
        {
            uint8_t* row = depth.scanLine(j);
            for (int i=0; i < depth.width(); ++i)
            {
                uint8_t a = row[i * 4 + 3];
                if (a)
                {
                    uint8_t pix = row[i * 4 + 0];
                    if (direction)
                        pix = uint8_t(pix * (j / float(depth.height())));
                    else
                        pix = uint8_t(pix * (1.0f - j / float(depth.height())));
                    row[i * 4 + 0] = pix;
                    row[i * 4 + 1] = pix;
                    row[i * 4 + 2] = pix;
                }
            }
        }
    }


    {   // Set normals to a flat value (rather than derivatives)
        float xy = sqrt(pow(M(0,2),2) + pow(M(1,2),2));
        float z = fabs(M(2,2));
        float len = sqrt(pow(xy, 2) + pow(z, 2));
        xy /= len;
        z /= len;
        uint8_t norm_r = uint8_t(xy * 255.0f);
        uint8_t norm_g = 0;
        uint8_t norm_b = uint8_t(z * 255.0f);
        for (int j=0; j < shaded.height(); ++j)
        {
            uint8_t* row_d = depth.scanLine(j);
            uint8_t* row_s = shaded.scanLine(j);
            for (int i=0; i < shaded.width(); ++i)
            {
                row_s[i * 4 + 0] = norm_r;
                row_s[i * 4 + 1] = norm_g;
                row_s[i * 4 + 2] = norm_b;
                row_s[i * 4 + 3] = row_d[i * 4 + 3];
            }
        }
    }

    flat = true;
}

Transform RenderTask::getTransform(QMatrix4x4 m)
{
    QMatrix4x4 mf = m.inverted();
    QMatrix4x4 mi = mf.inverted();

    Transform T = Transform(
                (boost::format("++*Xf%g*Yf%g*Zf%g") %
                    mf(0,0) % mf(0,1) % mf(0,2)).str(),
                (boost::format("++*Xf%g*Yf%g*Zf%g") %
                    mf(1,0) % mf(1,1) % mf(1,2)).str(),
                (boost::format("++*Xf%g*Yf%g*Zf%g") %
                    mf(2,0) % mf(2,1) % mf(2,2)).str(),
                (boost::format("++*Xf%g*Yf%g*Zf%g") %
                    mi(0,0) % mi(0,1) % mi(0,2)).str(),
                (boost::format("++*Xf%g*Yf%g*Zf%g") %
                    mi(1,0) % mi(1,1) % mi(1,2)).str(),
                (boost::format("++*Xf%g*Yf%g*Zf%g") %
                    mi(2,0) % mi(2,1) % mi(2,2)).str());

    return T;
}

////////////////////////////////////////////////////////////////////////////////

Bounds RenderTask::render(Shape* shape, Bounds b_, float scale)
{
    // Screen-space clipping:
    // x and y are clipped to the window;
    // z is clipped assuming a voxel depth of max(width, height)
    const float xmin = -M(0,3) - clip.x() / 2;
    const float xmax = -M(0,3) + clip.x() / 2;
    const float ymin = -M(1,3) - clip.y() / 2;
    const float ymax = -M(1,3) + clip.y() / 2;
    const float zmin = -M(2,3) - fmax(clip.x(), clip.y()) / 2;
    const float zmax = -M(2,3) + fmax(clip.x(), clip.y()) / 2;

    Bounds b(fmax(xmin, b_.xmin), fmax(ymin, b_.ymin),  fmax(zmin, b_.zmin),
             fmin(xmax, b_.xmax), fmin(ymax, b_.ymax),  fmin(zmax, b_.zmax));

    int img_w = std::max(1, int(ceil((b.xmax - b.xmin) * scale)));
    int img_h = std::max(1, int(ceil((b.ymax - b.ymin) * scale)));

    depth = QImage(img_w, img_h, QImage::Format_RGBA8888);
    shaded = QImage(img_w, img_h, QImage::Format_RGBA8888);

    depth.fill(0);
    shaded.fill(0);

    uint16_t* d16(new uint16_t[depth.width() * depth.height()]);
    uint16_t** d16_rows(new uint16_t*[depth.height()]);
    uint8_t (*s8)[3] = new uint8_t[depth.width() * depth.height()][3];
    uint8_t (**s8_rows)[3] = new decltype(s8)[depth.height()];

    for (int i=0; i < depth.height(); ++i)
    {
        d16_rows[i] = &d16[depth.width() * i];
        s8_rows[i] = &s8[depth.width() * i];
    }
    memset(d16, 0, depth.width() * depth.height() * 2);
    memset(s8, 0, depth.width() * depth.height() * 3);

    Region r = (Region) {
            .imin=0, .jmin=0, .kmin=0,
            .ni=(uint32_t)depth.width(), .nj=(uint32_t)depth.height(),
            .nk=uint32_t(fmax(1, (b.zmax - b.zmin) * scale))
    };

    build_arrays(&r, b.xmin, b.ymin, b.zmin,
                     b.xmax, b.ymax, b.zmax);
    render16(shape->tree.get(), r, d16_rows, &halt_flag, nullptr);
    shaded8(shape->tree.get(), r, d16_rows, s8_rows, &halt_flag, nullptr);

    free_arrays(&r);

    // Copy from bitmap arrays into a QImage
    for (int j=0; j < depth.height(); ++j)
    {
        uint8_t* depth_row = depth.scanLine(j);
        uint8_t* shaded_row = shaded.scanLine(j);
        for (int i=0; i < depth.width(); ++i)
        {
            uint16_t pix16 = d16_rows[j][i];
            uint8_t pix8 = pix16 >> 8;
            uint8_t* norm = s8_rows[j][i];
            if (pix8)
            {
                depth_row[i * 4 + 0] = pix8;
                depth_row[i * 4 + 1] = pix8;
                depth_row[i * 4 + 2] = pix8;
                depth_row[i * 4 + 3] = 255;

                if (pix16 < UINT16_MAX)
                {
                    shaded_row[i * 4 + 0] = norm[0];
                    shaded_row[i * 4 + 1] = norm[1];
                    shaded_row[i * 4 + 2] = norm[2];
                    shaded_row[i * 4 + 3] = 255;
                }
                else
                {
                    shaded_row[i * 4 + 0] = 0;
                    shaded_row[i * 4 + 1] = 0;
                    shaded_row[i * 4 + 2] = 255;
                    shaded_row[i * 4 + 3] = 255;
                }
            }
        }
    }

    delete [] s8;
    delete [] s8_rows;
    delete [] d16;
    delete [] d16_rows;

    return b;
}
