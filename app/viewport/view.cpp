#include <Python.h>

#include <cmath>
#include <algorithm>

#include <QOpenGLWidget>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QMenu>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QPainter>
#include <QFontMetrics>

#include "viewport/view.h"
#include "viewport/scene.h"
#include "viewport/image.h"
#include "viewport/control/control.h"
#include "viewport/control/control_instance.h"

#include "app/colors.h"

#include "graph/proxy/datum.h"
#include "graph/constructor/populate.h"

namespace {

struct CubeFaceDef {
    const char* name;
    QVector3D normal;
    QVector3D vec_right;
    QVector3D vec_up;
    QVector3D corners[4];
    float target_yaw;
    float target_pitch;
};

const float S = 44.0f;

const CubeFaceDef CUBE_FACES[6] = {
    // TOP (+Z)
    {"TOP", QVector3D(0, 0, 1),
     QVector3D(1, 0, 0), QVector3D(0, 1, 0),
     {QVector3D(-S, -S,  S), QVector3D( S, -S,  S), QVector3D( S,  S,  S), QVector3D(-S,  S,  S)},
     0.0f, 0.0f},

    // BOTTOM (-Z)
    {"BOTTOM", QVector3D(0, 0, -1),
     QVector3D(1, 0, 0), QVector3D(0, -1, 0),
     {QVector3D(-S,  S, -S), QVector3D( S,  S, -S), QVector3D( S, -S, -S), QVector3D(-S, -S, -S)},
     0.0f, -float(M_PI)},

    // FRONT (-Y)
    {"FRONT", QVector3D(0, -1, 0),
     QVector3D(1, 0, 0), QVector3D(0, 0, 1),
     {QVector3D(-S, -S, -S), QVector3D( S, -S, -S), QVector3D( S, -S,  S), QVector3D(-S, -S,  S)},
     0.0f, -float(M_PI) / 2.0f},

    // BACK (+Y)
    {"BACK", QVector3D(0, 1, 0),
     QVector3D(-1, 0, 0), QVector3D(0, 0, 1),
     {QVector3D( S,  S, -S), QVector3D(-S,  S, -S), QVector3D(-S,  S,  S), QVector3D( S,  S,  S)},
     -float(M_PI), -float(M_PI) / 2.0f},

    // RIGHT (+X)
    {"RIGHT", QVector3D(1, 0, 0),
     QVector3D(0, 1, 0), QVector3D(0, 0, 1),
     {QVector3D( S, -S, -S), QVector3D( S,  S, -S), QVector3D( S,  S,  S), QVector3D( S, -S,  S)},
     -float(M_PI) / 2.0f, -float(M_PI) / 2.0f},

    // LEFT (-X)
    {"LEFT", QVector3D(-1, 0, 0),
     QVector3D(0, -1, 0), QVector3D(0, 0, 1),
     {QVector3D(-S,  S, -S), QVector3D(-S, -S, -S), QVector3D(-S, -S,  S), QVector3D(-S,  S,  S)},
     float(M_PI) / 2.0f, -float(M_PI) / 2.0f}
};

} // namespace

ViewportView::ViewportView(QWidget* parent, ViewportScene* scene)
    : QGraphicsView(new QGraphicsScene(), parent), gl(new QOpenGLWidget(this)),
      scale(100), pitch(0), yaw(0), view_scene(scene)
{
    setStyleSheet("QGraphicsView { border-style: none; }");
    setRenderHints(QPainter::Antialiasing);

    setViewport(gl.context);

    setSceneRect(-width()/2, -height()/2, width(), height());
    setMouseTracking(true);

    QAbstractScrollArea::setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QAbstractScrollArea::setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

////////////////////////////////////////////////////////////////////////////////

QMatrix4x4 ViewportView::getMatrix(int params) const
{
    QMatrix4x4 M;

    // Remember that these operations are applied in reverse order.
    if (params & SCALE)
    {
        M.scale(scale, -scale, scale);
    }

    if (params & ROT)
    {
        M.rotate(pitch * 180 / M_PI, QVector3D(1, 0, 0));
        M.rotate(yaw  *  180 / M_PI, QVector3D(0, 0, 1));
    }

    if (params & MOVE)
    {
        M.translate(-center.x(), -center.y(), -center.z());
    }

    return M;
}

QVector3D ViewportView::sceneToWorld(QPointF p) const
{
    return getMatrix().inverted() * QVector3D(p.x(), p.y(), 0);
}

void ViewportView::installImage(DepthImage* d)
{
    connect(this, &ViewportView::getDepth,
            d, &DepthImage::getDepth);
    connect(this, &ViewportView::paintImage,
            d, &DepthImage::paint);
}

////////////////////////////////////////////////////////////////////////////////

void ViewportView::lockAngle(float y, float p)
{
    yaw = y;
    pitch = p;
    angle_locked = true;
}

////////////////////////////////////////////////////////////////////////////////

void ViewportView::drawBackground(QPainter* painter, const QRectF& rect)
{
    QGraphicsView::drawBackground(painter, rect);

    painter->beginNativePainting();
    if (!gl_initialized)
    {
        initializeOpenGLFunctions();
        gl_initialized = true;
    }
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Get bounds from all child images
    float zmin = INFINITY;
    float zmax = -INFINITY;
    auto m = getMatrix();
    emit(getDepth(m, &zmin, &zmax));

    // Paint all images
    emit(paintImage(m, zmin, zmax));

    painter->endNativePainting();
}

void ViewportView::drawAxes(QPainter* painter) const
{
    // First, draw the axes.
    auto m = getMatrix();
    QVector3D o = m * QVector3D(0, 0, 0);
    QVector3D x = m * QVector3D(1, 0, 0);
    QVector3D y = m * QVector3D(0, 1, 0);
    QVector3D z = m * QVector3D(0, 0, 1);

    QList<QPair<QVector3D, QColor>> pts = {
        {x, Colors::red},
        {y, Colors::green},
        {z, Colors::blue}};

    // Sort the axes to fake proper z clipping
    std::sort(pts.begin(), pts.end(),
            [](QPair<QVector3D, QColor> a, QPair<QVector3D, QColor> b)
            { return a.first.z() < b.first.z(); });

    for (auto p : pts)
    {
        painter->setPen(QPen(p.second, 2));
        painter->drawLine(o.toPointF(), p.first.toPointF());
    }
}

void ViewportView::drawCoords(QPainter* painter) const
{
    QPointF mouse_pos = mapToScene(mapFromGlobal(QCursor::pos()));
    if (!sceneRect().contains(mouse_pos))
    {
        return;
    }

    // Get rotate-only transform matrix
    QMatrix4x4 M = getMatrix(ROT);
    const float threshold = 0.98;

    const auto a = M.inverted() * QVector3D(0, 0, 1);

    QList<QPair<char, QVector3D>> axes = {
        {'x', QVector3D(1, 0, 0)},
        {'y', QVector3D(0, 1, 0)},
        {'z', QVector3D(0, 0, 1)}};

    char axis = 0;
    float opacity = 0;
    for (const auto v : axes)
    {
        float dot = fabs(QVector3D::dotProduct(a, v.second));
        if (dot > threshold)
        {
            axis = v.first;
            opacity = (dot - threshold) / (1 - threshold);
        }
    }

    auto p = sceneToWorld(mouse_pos);

    int value = opacity * 200;

    painter->setPen(QPen(QColor(255, 255, 255, value)));
    QString txt;
    if (axis == 'z')
    {
        txt = QString("X: %1\nY: %2").arg(p.x()).arg(p.y());
    }
    else if (axis == 'y')
    {
        txt = QString("X: %1\nZ: %2").arg(p.x()).arg(p.z());
    }
    else if (axis == 'x')
    {
        txt = QString("Y: %1\nZ: %2").arg(p.y()).arg(p.z());
    }

    painter->drawText({-width()/2.0 + 10, -height()/2.0 + 10, 300, 200}, txt);
}

void ViewportView::drawForeground(QPainter* painter, const QRectF& rect)
{
    QGraphicsView::drawForeground(painter, rect);
    drawAxes(painter);
    drawCoords(painter);
    drawViewCube(painter);
}

void ViewportView::drawViewCube(QPainter* painter) const
{
    if (ui_hidden || width() < 240 || height() < 240)
    {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    const QPointF cube_center(width() / 2.0f - 110.0f, -height() / 2.0f + 110.0f);
    const float disc_radius = 92.0f;

    // 1. Draw backdrop disc with subtle gradient and rim
    QRadialGradient bg(cube_center, disc_radius);
    bg.setColorAt(0.0, QColor(36, 40, 48, 205));
    bg.setColorAt(0.85, QColor(24, 27, 32, 215));
    bg.setColorAt(1.0, QColor(18, 20, 24, 225));
    painter->setBrush(bg);
    painter->setPen(QPen(QColor(255, 255, 255, 30), 1.5));
    painter->drawEllipse(cube_center, disc_radius, disc_radius);

    // Subtle inner compass ring & cardinal tick marks
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(QColor(255, 255, 255, 14), 1.0));
    painter->drawEllipse(cube_center, 80.0f, 80.0f);

    painter->setPen(QPen(QColor(255, 255, 255, 45), 2.0));
    painter->drawLine(QPointF(cube_center.x(), cube_center.y() - disc_radius + 3),
                     QPointF(cube_center.x(), cube_center.y() - disc_radius + 10));
    painter->drawLine(QPointF(cube_center.x(), cube_center.y() + disc_radius - 10),
                     QPointF(cube_center.x(), cube_center.y() + disc_radius - 3));
    painter->drawLine(QPointF(cube_center.x() - disc_radius + 3, cube_center.y()),
                     QPointF(cube_center.x() - disc_radius + 10, cube_center.y()));
    painter->drawLine(QPointF(cube_center.x() + disc_radius - 10, cube_center.y()),
                     QPointF(cube_center.x() + disc_radius - 3, cube_center.y()));

    // 2. Transform cube faces
    const QMatrix4x4 R = getMatrix(ROT);

    struct FaceRender {
        int index;
        float z;
        QPolygonF poly;
        QPointF center;
        QVector3D n_cam;
    };
    QVector<FaceRender> visible;

    for (int i = 0; i < 6; ++i)
    {
        QVector3D n_cam = R * CUBE_FACES[i].normal;
        if (n_cam.z() > 0.02f)
        {
            QPolygonF poly;
            QPointF p_sum(0, 0);
            for (int c = 0; c < 4; ++c)
            {
                QVector3D vc = R * CUBE_FACES[i].corners[c];
                QPointF pt(cube_center.x() + vc.x(), cube_center.y() - vc.y());
                poly << pt;
                p_sum += pt;
            }
            visible.append({i, n_cam.z(), poly, p_sum * 0.25f, n_cam});
        }
    }

    // Sort ascending by z so back faces are drawn first, closest drawn last (painter's algorithm)
    std::sort(visible.begin(), visible.end(),
              [](const FaceRender& a, const FaceRender& b) {
                  return a.z < b.z;
              });

    const QVector3D light_dir = QVector3D(0.35f, 0.45f, 0.82f).normalized();

    QFont font = painter->font();
    font.setPointSize(10);
    font.setBold(true);
    painter->setFont(font);
    QFontMetrics fm(font);

    // 3. Draw each visible face
    for (const auto& vf : visible)
    {
        bool is_hovered = (vf.index == hovered_cube_face);
        float dot = qMax(0.0f, QVector3D::dotProduct(vf.n_cam.normalized(), light_dir));
        float factor = 0.52f + 0.48f * dot;

        QColor face_color;
        QPen edge_pen;

        if (is_hovered)
        {
            // Vibrant cyan/azure highlight on hover
            int r = qBound(0, int(30 * factor + 20), 255);
            int g = qBound(0, int(135 * factor + 50), 255);
            int b = qBound(0, int(220 * factor + 35), 255);
            face_color = QColor(r, g, b, 245);
            edge_pen = QPen(QColor(170, 235, 255, 255), 3.0);
        }
        else
        {
            // Sleek CAD slate with directional lighting
            int r = qBound(0, int(50 * factor), 255);
            int g = qBound(0, int(58 * factor), 255);
            int b = qBound(0, int(70 * factor), 255);
            face_color = QColor(r, g, b, 230);
            edge_pen = QPen(QColor(110, 125, 145, 220), 2.0);
        }

        painter->setPen(edge_pen);
        painter->setBrush(face_color);
        painter->drawPolygon(vf.poly);

        // Smoothly fade label as face turns away
        float text_alpha = qBound(0.0f, (vf.z - 0.15f) / 0.25f, 1.0f);
        if (text_alpha > 0.05f)
        {
            const auto& face_def = CUBE_FACES[vf.index];
            QVector3D r_cam = R * face_def.vec_right;
            QVector3D u_cam = R * face_def.vec_up;

            QPointF r_screen(r_cam.x(), -r_cam.y());
            QPointF u_screen(u_cam.x(), -u_cam.y());

            painter->save();
            painter->translate(vf.center);
            QTransform rot(
                r_screen.x(),  r_screen.y(),
                -u_screen.x(), -u_screen.y(),
                0, 0
            );
            painter->setTransform(rot, true);

            const QString name = face_def.name;
            QColor text_color = is_hovered ? QColor(255, 255, 255)
                                           : QColor(220, 230, 240);
            text_color.setAlphaF(text_color.alphaF() * text_alpha);

            painter->setPen(text_color);
            QRectF text_box(-S, -S, S * 2, S * 2);
            painter->drawText(text_box, Qt::AlignCenter, name);

            painter->restore();
        }
    }

    painter->restore();
}

int ViewportView::viewCubeFaceAt(QPointF scene_pos) const
{
    if (ui_hidden || width() < 240 || height() < 240)
    {
        return -1;
    }

    const QPointF cube_center(width() / 2.0f - 110.0f, -height() / 2.0f + 110.0f);
    const QPointF delta = scene_pos - cube_center;
    if (delta.x() * delta.x() + delta.y() * delta.y() > 100.0f * 100.0f)
    {
        return -1;
    }

    const QMatrix4x4 R = getMatrix(ROT);

    struct VisibleFace {
        int index;
        float z;
        QPolygonF poly;
    };
    QVector<VisibleFace> visible;

    for (int i = 0; i < 6; ++i)
    {
        QVector3D n_cam = R * CUBE_FACES[i].normal;
        if (n_cam.z() > 0.05f)
        {
            QPolygonF poly;
            for (int c = 0; c < 4; ++c)
            {
                QVector3D vc = R * CUBE_FACES[i].corners[c];
                poly << QPointF(cube_center.x() + vc.x(), cube_center.y() - vc.y());
            }
            visible.append({i, n_cam.z(), poly});
        }
    }

    // Sort descending by z (closest to camera first)
    std::sort(visible.begin(), visible.end(),
              [](const VisibleFace& a, const VisibleFace& b) {
                  return a.z > b.z;
              });

    for (const auto& vf : visible)
    {
        if (vf.poly.containsPoint(scene_pos, Qt::OddEvenFill))
        {
            return vf.index;
        }
    }

    return -1;
}

////////////////////////////////////////////////////////////////////////////////

void ViewportView::update()
{
    scene()->invalidate();
    emit(changed(getMatrix(), geometry()));

    emit(centerChanged(center));
    emit(scaleChanged(scale));
}

////////////////////////////////////////////////////////////////////////////////

void ViewportView::installControl(Control* c)
{
    c->makeInstanceFor(this);
}

void ViewportView::installDatum(BaseDatumProxy* d)
{
    d->addViewport(this);
}

////////////////////////////////////////////////////////////////////////////////

void ViewportView::setScale(float s)
{
    scale = s;

    // Update the view without calling ViewportView::update
    // (because that would lead to infinite recursion)
    scene()->invalidate();
    emit(changed(getMatrix(), geometry()));
}

void ViewportView::setCenter(QVector3D c)
{
    center = c;

    // Update the view without calling ViewportView::update
    // (because that would lead to infinite recursion)
    scene()->invalidate();
    emit(changed(getMatrix(), geometry()));
}

////////////////////////////////////////////////////////////////////////////////

void ViewportView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !ui_hidden && !angle_locked)
    {
        int face = viewCubeFaceAt(mapToScene(event->pos()));
        if (face != -1)
        {
            spinTo(CUBE_FACES[face].target_yaw, CUBE_FACES[face].target_pitch);
            event->accept();
            return;
        }
    }

    QGraphicsView::mousePressEvent(event);

    // If the event hasn't been accepted, record click position for
    // panning / rotation on mouse drag.
    if (!event->isAccepted())
    {
        if (event->button() == Qt::LeftButton)
        {
            click_pos = mapToScene(event->pos());
            click_pos_world = sceneToWorld(click_pos);
        }
        else
        {
            click_pos = event->pos();
        }
        dragged = false;
    }
}

void ViewportView::mouseMoveEvent(QMouseEvent* event)
{
    QGraphicsView::mouseMoveEvent(event);

    current_pos = event->pos();
    if (scene()->mouseGrabberItem() == NULL)
    {
        if (event->buttons() == Qt::LeftButton)
        {
            center += click_pos_world - sceneToWorld(mapToScene(event->pos()));
            update();
        }
        else if (event->buttons() == Qt::RightButton && !angle_locked)
        {
            QPointF d = click_pos - event->pos();
            pitch = fmin(0, fmax(-M_PI, pitch - 0.01 * d.y()));
            yaw = fmod(yaw + M_PI - 0.01 * d.x(), M_PI*2) - M_PI;

            click_pos = event->pos();
            update();
        }
        dragged = true;
    }

    if (event->buttons() == Qt::NoButton && !ui_hidden && !angle_locked)
    {
        int face = viewCubeFaceAt(mapToScene(event->pos()));
        if (face != hovered_cube_face)
        {
            hovered_cube_face = face;
            if (hovered_cube_face != -1)
            {
                setCursor(Qt::PointingHandCursor);
            }
            else
            {
                unsetCursor();
            }
            scene()->invalidate(QRect(), QGraphicsScene::ForegroundLayer);
        }
    }
    else if (event->buttons() != Qt::NoButton && hovered_cube_face != -1)
    {
        hovered_cube_face = -1;
        unsetCursor();
        scene()->invalidate(QRect(), QGraphicsScene::ForegroundLayer);
    }

    // Redraw to update cursor position
    scene()->invalidate(QRect(), QGraphicsScene::ForegroundLayer);
}

void ViewportView::leaveEvent(QEvent* event)
{
    QGraphicsView::leaveEvent(event);
    if (hovered_cube_face != -1)
    {
        hovered_cube_face = -1;
        unsetCursor();
        scene()->invalidate(QRect(), QGraphicsScene::ForegroundLayer);
    }
}

void ViewportView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton && !dragged)
    {
        auto is = items(event->pos());
        if (is.size() > 1)
        {
            openRaiseMenu(is);
        }
        else if (is.size())
        {
            if (auto c = dynamic_cast<ControlInstance*>(is.front()))
            {
                c->openContextMenu();
            }
        }
        else
        {
            openAddMenu(true);
        }
    }
    else
    {
        QGraphicsView::mouseReleaseEvent(event);
    }
}

void ViewportView::wheelEvent(QWheelEvent* event)
{
    QVector3D a = sceneToWorld(mapToScene(event->position().toPoint()));
    scale *= pow(1.001, -event->angleDelta().y());
    QVector3D b = sceneToWorld(mapToScene(event->position().toPoint()));
    center += a - b;
    update();
}

void ViewportView::resizeEvent(QResizeEvent* e)
{
    Q_UNUSED(e);
    setSceneRect(-width()/2, -height()/2, width(), height());
    if (hovered_cube_face != -1)
    {
        hovered_cube_face = -1;
        unsetCursor();
    }
}

void ViewportView::keyPressEvent(QKeyEvent* event)
{
    QGraphicsView::keyPressEvent(event);
    if (event->isAccepted())
    {
        return;
    }
    if (event->key() == Qt::Key_A &&
        (event->modifiers() & Qt::ShiftModifier))
    {
        openAddMenu();
    }
}

////////////////////////////////////////////////////////////////////////////////

void ViewportView::openAddMenu(bool view_commands)
{
    QMenu* m = new QMenu(this);

    if (view_commands && !angle_locked)
    {
        auto sub = new QMenu("View");

        connect(sub->addAction("Top"), &QAction::triggered,
                [=]{ this->spinTo(0, 0); });
        connect(sub->addAction("Bottom"), &QAction::triggered,
                [=]{ this->spinTo(0, -M_PI); });
        connect(sub->addAction("Left"), &QAction::triggered,
                [=]{ this->spinTo(M_PI/2, -M_PI/2); });
        connect(sub->addAction("Right"), &QAction::triggered,
                [=]{ this->spinTo(-M_PI/2, -M_PI/2); });
        connect(sub->addAction("Front"), &QAction::triggered,
                [=]{ this->spinTo(0, -M_PI/2); });
        connect(sub->addAction("Back"), &QAction::triggered,
                [=]{ this->spinTo(-M_PI, -M_PI/2); });

        m->addMenu(sub);
        m->addSeparator();
    }

    populateNodeMenu(m, view_scene->getGraph());

    m->exec(QCursor::pos());
    m->deleteLater();
}

void ViewportView::openRaiseMenu(QList<QGraphicsItem*> items)
{
    QScopedPointer<QMenu> m(new QMenu(this));

    int found = 0;
    for (auto i : items)
    {
        if (auto c = dynamic_cast<ControlInstance*>(i))
        {
            auto a = new QAction(c->getName(), m.data());
            a->setData(QVariant::fromValue(c));
            m->addAction(a);
            found++;
        }
    }

    QAction* chosen = (found > 1) ? m->exec(QCursor::pos())
                                  : NULL;

    if (chosen)
    {
        if (raised)
        {
            raised->setZValue(0);
        }
        raised = chosen->data().value<ControlInstance*>();
        raised->setZValue(0.1);
    }
}

////////////////////////////////////////////////////////////////////////////////

void ViewportView::setYaw(float y)
{
    yaw = y;
    update();
}

void ViewportView::setPitch(float p)
{
    pitch = p;
    update();
}

void ViewportView::spinTo(float new_yaw, float new_pitch)
{
    for (auto child : findChildren<QPropertyAnimation*>())
    {
        if (child->propertyName() == "_yaw" || child->propertyName() == "_pitch")
        {
            child->stop();
        }
    }

    // Find shortest angular path for yaw
    float dyaw = new_yaw - yaw;
    while (dyaw > M_PI) dyaw -= 2 * M_PI;
    while (dyaw < -M_PI) dyaw += 2 * M_PI;
    float target_yaw = yaw + dyaw;

    QPropertyAnimation* a = new QPropertyAnimation(this, "_yaw", this);
    a->setDuration(220);
    a->setEasingCurve(QEasingCurve::InOutCubic);
    a->setStartValue(yaw);
    a->setEndValue(target_yaw);

    connect(a, &QPropertyAnimation::finished, [this]() {
        yaw = fmod(yaw + M_PI, M_PI * 2);
        if (yaw < 0) yaw += M_PI * 2;
        yaw -= M_PI;
    });

    QPropertyAnimation* b = new QPropertyAnimation(this, "_pitch", this);
    b->setDuration(220);
    b->setEasingCurve(QEasingCurve::InOutCubic);
    b->setStartValue(pitch);
    b->setEndValue(new_pitch);

    a->start(QPropertyAnimation::DeleteWhenStopped);
    b->start(QPropertyAnimation::DeleteWhenStopped);
}

////////////////////////////////////////////////////////////////////////////////

void ViewportView::zoomTo(Node* n)
{
    // Find all ControlInstances that are declared by this node
    QList<ControlInstance*> instances;
    for (auto i : items())
    {
        if (auto c = dynamic_cast<ControlInstance*>(i))
        {
            if (c->getNode() == n)
            {
                instances.push_back(c);
            }
        }
    }

    // Find a weighted sum of central points
    QVector3D pos;
    float area_sum = 0;
    for (auto i : instances)
    {
        const float area = i->boundingRect().width() *
                           i->boundingRect().height();
        pos += i->getControl()->pos() * area;
        area_sum += area;
    }
    pos /= area_sum;

    auto a = new QPropertyAnimation(this, "_center");
    a->setDuration(100);
    a->setStartValue(center);
    a->setEndValue(pos);

    a->start(QPropertyAnimation::DeleteWhenStopped);
}

void ViewportView::hideUI(bool b)
{
    ui_hidden = b;
    for (auto i : scene()->items())
    {
        if (auto c = dynamic_cast<ControlInstance*>(i))
        {
            if (b)
            {
                c->hide();
            }
            else
            {
                c->show();
            }
        }
    }
}
