/*
 *  This file is a part of KNOSSOS.
 *
 *  (C) Copyright 2007-2016
 *  Max-Planck-Gesellschaft zur Foerderung der Wissenschaften e.V.
 *
 *  KNOSSOS is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 of
 *  the License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *  For further information, visit https://knossostool.org
 *  or contact knossos-team@mpimf-heidelberg.mpg.de
 */

#include "viewport.h"
#include <iostream>
#include "functions.h"
#include "GuiConstants.h"
#include "profiler.h"
#include "mesh/mesh.h"
#include "scriptengine/scripting.h"
#include "segmentation/cubeloader.h"
#include "segmentation/segmentation.h"
#include "skeleton/skeletonizer.h"
#include "viewer.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QMenu>
#include <QOpenGLFramebufferObject>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

bool ViewportBase::oglDebug = false;
bool Viewport3D::showBoundariesInUm = false;
bool ViewportOrtho::showNodeComments = false;

RenderOptions::RenderOptions() : drawCrosshairs(state->viewerState->drawVPCrosshairs && state->viewerState->showOnlyRawData == false),
                                 drawOverlay(Segmentation::enabled && state->viewerState->showOnlyRawData == false) {}

RenderOptions RenderOptions::nodePickingRenderOptions(RenderOptions::SelectionPass pass) {
    RenderOptions options;
    options.drawBoundaryAxes = options.drawBoundaryBox = options.drawCrosshairs = options.drawOverlay = options.drawMesh = false;
    options.drawViewportPlanes = options.highlightActiveNode = options.highlightSelection = false;
    options.nodePicking = true;
    options.selectionPass = pass;
    return options;
}

RenderOptions RenderOptions::meshPickingRenderOptions() {
    RenderOptions options;
    options.drawBoundaryAxes = options.drawBoundaryBox = options.drawCrosshairs = options.drawOverlay = options.drawSkeleton = options.drawViewportPlanes = false;
    options.drawMesh = options.meshPicking = true;
    return options;
}

RenderOptions RenderOptions::snapshotRenderOptions(const bool drawBoundaryAxes, const bool drawBoundaryBox, const bool drawOverlay, const bool drawMesh, const bool drawSkeleton, const bool drawViewportPlanes) {
    RenderOptions options;
    options.drawBoundaryAxes = drawBoundaryAxes;
    options.drawBoundaryBox = drawBoundaryBox;
    options.drawCrosshairs = false;
    options.drawOverlay = drawOverlay;
    options.drawMesh = drawMesh;
    options.drawSkeleton = drawSkeleton;
    options.drawViewportPlanes = drawViewportPlanes;
    options.enableLoddingAndLinesAndPoints = false;
    options.enableTextScaling = true;
    options.highlightActiveNode = false;
    options.highlightSelection = false;
    return options;
}

void ResizeButton::mouseMoveEvent(QMouseEvent * event) {
    emit vpResize(event->globalPos());
}

QViewportFloatWidget::QViewportFloatWidget(QWidget *parent, ViewportBase *vp) : QDialog(parent), vp(vp) {
    const std::array<const char * const, ViewportBase::numberViewports> VP_TITLES{{"XY", "XZ", "ZY", "Arbitrary", "3D"}};
    setWindowTitle(VP_TITLES[vp->viewportType]);
    new QVBoxLayout(this);
}

void QViewportFloatWidget::closeEvent(QCloseEvent *event) {
    vp->setDock(true);
    QWidget::closeEvent(event);
}

void QViewportFloatWidget::moveEvent(QMoveEvent *event) {
    // Entering/leaving fullscreen mode and docking/undocking cause moves and resizes.
    // These should not overwrite the non maximized position and sizes.
    if (vp->isDocked == false && fullscreen == false) {
        nonMaximizedPos = pos();
    }
    QWidget::moveEvent(event);
}

QSize vpSizeFromWindowSize(const QSize & size) {
    const int len = ((size.height() <= size.width()) ? size.height() : size.width()) - 2 * DEFAULT_VP_MARGIN;
    return {len, len};
}

void QViewportFloatWidget::resizeEvent(QResizeEvent *event) {
    if (vp->isDocked == false) {
        vp->resize(vpSizeFromWindowSize(size()));
        if (fullscreen == false) {
            nonMaximizedSize = size();
        }
    }
    QWidget::resizeEvent(event);
}

void QViewportFloatWidget::showEvent(QShowEvent *event) {
    vp->resize(vpSizeFromWindowSize(size()));
    QWidget::showEvent(event);
}

ViewportBase::ViewportBase(QWidget *parent, ViewportType viewportType) :
    QOpenGLWidget(parent), dockParent(parent), resizeButton(this), viewportType(viewportType), floatParent(parent, this), edgeLength(width()) {
    setCursor(Qt::CrossCursor);
    setMouseTracking(true);
    setFocusPolicy(Qt::WheelFocus);

    QObject::connect(&snapshotAction, &QAction::triggered, [this]() { emit snapshotTriggered(this->viewportType); });
    QObject::connect(&floatingWindowAction, &QAction::triggered, [this]() { setDock(!isDocked); });
    QObject::connect(&fullscreenAction, &QAction::triggered, [this]() {
        if (isDocked) {
            // Currently docked and normal
            // Undock and go fullscreen from docked
            setDock(false);
            floatParent.setWindowState(Qt::WindowFullScreen);
            floatParent.fullscreen = true;
            isFullOrigDocked = true;
        }
        else {
            // Currently undocked
            if (floatParent.isFullScreen()) {
                // Currently fullscreen
                // Go normal and back to original docking state
                floatParent.setWindowState(Qt::WindowNoState);
                floatParent.fullscreen = false;
                if (isFullOrigDocked) {
                    setDock(isFullOrigDocked);
                }
            }
            else {
                // Currently not fullscreen
                // Go fullscreen from undocked
                floatParent.setWindowState(Qt::WindowFullScreen);
                floatParent.fullscreen = true;
                isFullOrigDocked = false;
            }
        }
    });
    QObject::connect(&hideAction, &QAction::triggered, this, &ViewportBase::hideVP);
    menuButton.setText("…");
    menuButton.setCursor(Qt::ArrowCursor);
    menuButton.setMinimumSize(35, 20);
    menuButton.setMaximumSize(menuButton.minimumSize());
    menuButton.addAction(&snapshotAction);
    menuButton.addAction(&floatingWindowAction);
    menuButton.addAction(&fullscreenAction);
    menuButton.addAction(&hideAction);
    menuButton.setPopupMode(QToolButton::InstantPopup);
    resizeButton.setCursor(Qt::SizeFDiagCursor);
    resizeButton.setIcon(QIcon(":/resources/icons/resize.gif"));
    resizeButton.setMinimumSize(20, 20);
    resizeButton.setMaximumSize(resizeButton.minimumSize());
    QObject::connect(&resizeButton, &ResizeButton::vpResize, [this](const QPoint & globalPos) {
        raise();//we come from the resize button
        //»If you move the widget as a result of the mouse event, use the global position returned by globalPos() to avoid a shaking motion.«
        const auto desiredSize = mapFromGlobal(globalPos);
        sizeAdapt(desiredSize);
        state->viewer->setDefaultVPSizeAndPos(false);
    });

    vpLayout.setMargin(0);//attach buttons to vp border
    vpLayout.addStretch(1);
    vpHeadLayout.setAlignment(Qt::AlignTop | Qt::AlignRight);
    vpHeadLayout.addWidget(&menuButton);
    vpHeadLayout.setSpacing(0);
    vpLayout.insertLayout(0, &vpHeadLayout);
    vpLayout.addWidget(&resizeButton, 0, Qt::AlignBottom | Qt::AlignRight);
    setLayout(&vpLayout);
}

ViewportBase::~ViewportBase() {
    if (oglDebug && oglLogger.isLogging()) {
        makeCurrent();
        oglLogger.stopLogging();
    }
}

void ViewportBase::setDock(bool isDock) {
    isDocked = isDock;
    if (isDock) {
        setParent(dockParent);
        floatParent.hide();
        move(dockPos);
        resize(dockSize);
    } else {
        state->viewer->setDefaultVPSizeAndPos(false);
        if (floatParent.nonMaximizedPos == boost::none) {
            floatParent.nonMaximizedSize = QSize( size().width() + 2 * DEFAULT_VP_MARGIN, size().height() + 2 * DEFAULT_VP_MARGIN );
            floatParent.nonMaximizedPos = mapToGlobal(QPoint(0, 0));
        }
        setParent(&floatParent);
        if (floatParent.fullscreen) {
            floatParent.setWindowState(Qt::WindowFullScreen);
        } else {
            floatParent.move(floatParent.nonMaximizedPos.get());
            floatParent.resize(floatParent.nonMaximizedSize.get());
        }
        move(QPoint(DEFAULT_VP_MARGIN, DEFAULT_VP_MARGIN));
        floatParent.show();
    }
    floatingWindowAction.setVisible(isDock);
    resizeButton.setVisible(isDock && state->viewerState->showVpDecorations);
    show();
    setFocus();
}

void ViewportBase::moveVP(const QPoint & globalPos) {
    if (!isDocked) {
        // Moving viewports is relevant only when docked
        return;
    }
    //»If you move the widget as a result of the mouse event, use the global position returned by globalPos() to avoid a shaking motion.«
    const auto position = mapFromGlobal(globalPos);
    const auto desiredX = x() + position.x() - mouseDown.x();
    const auto desiredY = y() + position.y() - mouseDown.y();
    posAdapt({desiredX, desiredY});
    state->viewer->setDefaultVPSizeAndPos(false);
}

void ViewportBase::hideVP() {
    if (isDocked) {
        hide();
    } else {
        floatParent.hide();
        setParent(dockParent);
        hide();
    }
    state->viewer->setDefaultVPSizeAndPos(false);
}

void ViewportBase::moveEvent(QMoveEvent *event) {
    if (isDocked) {
        dockPos = pos();
    }
    QOpenGLWidget::moveEvent(event);
}

void ViewportBase::resizeEvent(QResizeEvent *event) {
    if (isDocked) {
        dockSize = size();
    }
    QOpenGLWidget::resizeEvent(event);
}

ViewportOrtho::ViewportOrtho(QWidget *parent, ViewportType viewportType) : ViewportBase(parent, viewportType) {
    // v2 is negative because it goes from top to bottom on screen
    // n is positive if we want to look towards 0 and negative to look towards infinity
    switch(viewportType) {
    case VIEWPORT_XZ:
        v1 = {1, 0,  0};
        v2 = {0, 0, -1};
        n  = {0, 1,  0};
        break;
    case VIEWPORT_ZY:
        v1 = {0,  0, 1};
        v2 = {0, -1, 0};
        n  = {1,  0, 0};
        break;
    case VIEWPORT_XY:
    case VIEWPORT_ARBITRARY:
        v1 = {1,  0,  0};
        v2 = {0, -1,  0};
        n  = {0,  0, -1};
        break;
    default:
        throw std::runtime_error("ViewportOrtho::ViewportOrtho unknown vp");
    }
}

ViewportOrtho::~ViewportOrtho() {
    makeCurrent();
    if (texture.texHandle != 0) {
        glDeleteTextures(1, &texture.texHandle);
    }
    if (texture.overlayHandle != 0) {
        glDeleteTextures(1, &texture.overlayHandle);
    }
    texture.texHandle = texture.overlayHandle = 0;
}

void ViewportOrtho::mouseMoveEvent(QMouseEvent *event) {
    ViewportBase::mouseMoveEvent(event);
    Segmentation::singleton().brush.setView(static_cast<brush_t::view_t>(viewportType), v1, v2, n);
}

void ViewportOrtho::mousePressEvent(QMouseEvent *event) {
    Segmentation::singleton().brush.setView(static_cast<brush_t::view_t>(viewportType), v1, v2, n);
    ViewportBase::mousePressEvent(event);
}

void ViewportOrtho::resetTexture() {
    const auto size = texture.size;
    if (texture.texHandle != 0) {
        glBindTexture(GL_TEXTURE_2D, texture.texHandle);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    }
    if (texture.overlayHandle != 0) {
        glBindTexture(GL_TEXTURE_2D, texture.overlayHandle);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
}

Viewport3D::Viewport3D(QWidget *parent, ViewportType viewportType) : ViewportBase(parent, viewportType) {
    wiggleButton.setCheckable(true);
    wiggleButton.setToolTip("Wiggle stereoscopy (Hold W)");

    for (auto * button : {&xyButton, &xzButton, &zyButton}) {
        button->setMinimumSize(30, 20);
    }
    wiggleButton.setMinimumSize(35, 20);
    r90Button.setMinimumSize(35, 20);
    r180Button.setMinimumSize(40, 20);
    resetButton.setMinimumSize(45, 20);

    for (auto * button : {&resetButton, &r180Button, &r90Button, &zyButton, &xzButton, &xyButton, &wiggleButton}) {
        button->setMaximumSize(button->minimumSize());
        button->setCursor(Qt::ArrowCursor);
        vpHeadLayout.insertWidget(0, button);
    }

    connect(&xyButton, &QPushButton::clicked, []() {
        if(state->skeletonState->rotationcounter == 0) {
            state->skeletonState->definedSkeletonVpView = SKELVP_XY_VIEW;
        }
    });
    connect(&xzButton, &QPushButton::clicked, []() {
        if(state->skeletonState->rotationcounter == 0) {
            state->skeletonState->definedSkeletonVpView = SKELVP_XZ_VIEW;
        }
    });
    connect(&zyButton, &QPushButton::clicked, []() {
        if(state->skeletonState->rotationcounter == 0) {
            state->skeletonState->definedSkeletonVpView = SKELVP_ZY_VIEW;
        }
    });
    connect(&r90Button, &QPushButton::clicked, []() {
        if(state->skeletonState->rotationcounter == 0) {
            state->skeletonState->definedSkeletonVpView = SKELVP_R90;
        }
    });
    connect(&r180Button, &QPushButton::clicked, []() {
        if(state->skeletonState->rotationcounter == 0) {
            state->skeletonState->definedSkeletonVpView = SKELVP_R180;
        }
    });
    QObject::connect(&wiggleButton, &QPushButton::clicked, [this](bool checked){
        if (checked) {
            wiggletimer.start();
        } else {
            resetWiggle();
        }
    });
    connect(&resetButton, &QPushButton::clicked, []() {
        if(state->skeletonState->rotationcounter == 0) {
            state->skeletonState->definedSkeletonVpView = SKELVP_RESET;
        }
    });

    wiggletimer.setInterval(30);
    QObject::connect(&wiggletimer, &QTimer::timeout, [this](){
        const auto inc = wiggleDirection ? 1 : -1;
        state->skeletonState->rotdx += inc;
        state->skeletonState->rotdy += inc;
        wiggle += inc;
        if (wiggle == -2 || wiggle == 2) {
            wiggleDirection = !wiggleDirection;
        }
    });
}

Viewport3D::~Viewport3D() {
    makeCurrent();
    if (Segmentation::singleton().volume_tex_id != 0) {
        glDeleteTextures(1, &Segmentation::singleton().volume_tex_id);
    }
    Segmentation::singleton().volume_tex_id = 0;
}

void ViewportBase::initializeGL() {
    static bool printed = false;
    if (!printed) {
        qDebug() << reinterpret_cast<const char*>(::glGetString(GL_VERSION))
                 << reinterpret_cast<const char*>(::glGetString(GL_VENDOR))
                 << reinterpret_cast<const char*>(::glGetString(GL_RENDERER));
        printed = true;
    }
    if (!initializeOpenGLFunctions()) {
        qDebug() << "initializeOpenGLFunctions failed";
    }
    QObject::connect(&oglLogger, &QOpenGLDebugLogger::messageLogged, [](const QOpenGLDebugMessage & msg){
        if (msg.type() == QOpenGLDebugMessage::ErrorType) {
            qWarning() << msg;
        } else {
            qDebug() << msg;
        }
    });
    if (oglDebug && oglLogger.initialize()) {
        oglLogger.startLogging(QOpenGLDebugLogger::SynchronousLogging);
    }

    meshShader.addShaderFromSourceCode(QOpenGLShader::Vertex, R"shaderSource(
        #version 110
        attribute vec3 vertex;
        attribute vec3 normal;
        attribute vec4 color;

        uniform mat4 modelview_matrix;
        uniform mat4 projection_matrix;

        varying vec4 frag_color;
        varying vec3 frag_normal;
        varying mat4 mvp_matrix;

        void main() {
            mvp_matrix = projection_matrix * modelview_matrix;
            gl_Position = mvp_matrix * vec4(vertex, 1.0);
            frag_color = color;
            frag_normal = normal;
        }
    )shaderSource");
    meshShader.addShaderFromSourceCode(QOpenGLShader::Fragment, R"shaderSource(
        #version 110

        uniform mat4 modelview_matrix;
        uniform mat4 projection_matrix;
        uniform vec4 tree_color;

        varying vec4 frag_color;
        varying vec3 frag_normal;
        varying mat4 mvp_matrix;

        void main() {
            vec3 specular_color = vec3(1.0, 1.0, 1.0);
            float specular_exp = 3.0;
            vec3 view_dir = vec3(0.0, 0.0, 1.0);

            // diffuse lighting
            vec3 main_light_dir = normalize((/*modelview_matrix **/ vec4(0.0, -1.0, 0.0, 0.0)).xyz);
            float main_light_power = max(0.0, dot(-main_light_dir, frag_normal));
            vec3 sub_light_dir = vec3(0.0, 1.0, 0.0);
            float sub_light_power = max(0.0, dot(-sub_light_dir, frag_normal));

            // pseudo ambient lighting
            vec3 pseudo_ambient_dir = view_dir;
            float pseudo_ambient_power = pow(abs(max(0.0, dot(pseudo_ambient_dir, frag_normal)) - 1.0), 3.0);

            // specular
            float specular_power = 0.0;
            if (dot(frag_normal, -main_light_dir) >= 0.0) {
                specular_power = pow(max(0.0, dot(reflect(-main_light_dir, frag_normal), view_dir)), specular_exp);
            }

            vec3 fcolor = frag_color.rgb;
            gl_FragColor = vec4((0.25 * fcolor                                 // ambient
                        + 0.75 * fcolor * main_light_power                     // diffuse(main)
                        + 0.25 * fcolor * sub_light_power         // diffuse(sub)
                        // + 0.3 * vec3(1.0, 1.0, 1.0) * pseudo_ambient_power // pseudo ambient lighting
                        // + specular_color * specular_power                  // specular
                        ) //* ambient_occlusion_power
                        , frag_color.a);

            // gl_FragColor = //vec4((frag_normal+1.0)/2.0, 1.0); // display normals
        }
    )shaderSource");
    meshShader.link();

    meshTreeColorShader.addShaderFromSourceCode(QOpenGLShader::Vertex, R"shaderSource(
        #version 110
        attribute vec3 vertex;
        attribute vec3 normal;
        attribute vec4 color;

        uniform mat4 modelview_matrix;
        uniform mat4 projection_matrix;

        varying vec3 frag_normal;
        varying mat4 mvp_matrix;

        void main() {
            mvp_matrix = projection_matrix * modelview_matrix;
            gl_Position = mvp_matrix * vec4(vertex, 1.0);
            frag_normal = normal;
        }
    )shaderSource");
    meshTreeColorShader.addShaderFromSourceCode(QOpenGLShader::Fragment, R"shaderSource(
        #version 110

        uniform mat4 modelview_matrix;
        uniform mat4 projection_matrix;
        uniform vec4 tree_color;

        varying vec4 frag_color;
        varying vec3 frag_normal;
        varying mat4 mvp_matrix;

        void main() {
            vec3 specular_color = vec3(1.0, 1.0, 1.0);
            float specular_exp = 3.0;
            vec3 view_dir = vec3(0.0, 0.0, 1.0);

            // diffuse lighting
            vec3 main_light_dir = normalize((/*modelview_matrix **/ vec4(0.0, -1.0, 0.0, 0.0)).xyz);
            float main_light_power = max(0.0, dot(-main_light_dir, frag_normal));
            vec3 sub_light_dir = vec3(0.0, 1.0, 0.0);
            float sub_light_power = max(0.0, dot(-sub_light_dir, frag_normal));

            vec3 fcolor = tree_color.rgb;
            gl_FragColor = vec4((0.25 * fcolor             // ambient
                        + 0.75 * fcolor * main_light_power // diffuse(main)
                        + 0.25 * fcolor * sub_light_power  // diffuse(sub)
                        )
                        , tree_color.a);
        }
    )shaderSource");
    meshTreeColorShader.link();

    meshIdShader.addShaderFromSourceCode(QOpenGLShader::Vertex, R"shaderSource(
        #version 130

        in vec3 vertex;
        in vec4 color;

        uniform mat4 modelview_matrix;
        uniform mat4 projection_matrix;

        flat out vec4 frag_color;
        out mat4 mvp_matrix;

        void main() {
            mvp_matrix = projection_matrix * modelview_matrix;
            gl_Position = mvp_matrix * vec4(vertex, 1.0);
            frag_color = color;
        }
    )shaderSource");

    meshIdShader.addShaderFromSourceCode(QOpenGLShader::Fragment, R"shaderSource(
        #version 130

        uniform mat4 modelview_matrix;
        uniform mat4 projection_matrix;

        flat in vec4 frag_color;
        in mat4 mvp_matrix;

        out vec4 fragColorOut;

        void main() {
            fragColorOut = frag_color;
        }
    )shaderSource");

    meshIdShader.link();
    for (auto * shader : {&meshShader, &meshTreeColorShader, &meshIdShader}) {
        if (!shader->log().isEmpty()) {
            qDebug() << shader->log();
        }
    }
}

void ViewportOrtho::initializeGL() {
    ViewportBase::initializeGL();
    glGenTextures(1, &texture.texHandle);

    glBindTexture(GL_TEXTURE_2D, texture.texHandle);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture.textureFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture.textureFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    // loads an empty texture into video memory - during user movement, this
    // texture is updated via glTexSubImage2D in vpGenerateTexture
    // We need GL_RGB as texture internal format to color the textures

    std::vector<char> texData(4 * std::pow(state->viewerState->texEdgeLength, 2));
    glTexImage2D(GL_TEXTURE_2D,
                0,
                GL_RGB,
                texture.size,
                texture.size,
                0,
                GL_RGB,
                GL_UNSIGNED_BYTE,
                texData.data());

    createOverlayTextures();

    if (state->gpuSlicer) {
        if (viewportType == ViewportType::VIEWPORT_XY) {
//            state->viewer->gpucubeedge = 128;
            state->viewer->layers.emplace_back(*context());
            state->viewer->layers.back().createBogusCube(state->cubeEdgeLength, state->viewer->gpucubeedge);
            state->viewer->layers.emplace_back(*context());
//            state->viewer->layers.back().enabled = false;
            state->viewer->layers.back().isOverlayData = true;
            state->viewer->layers.back().createBogusCube(state->cubeEdgeLength, state->viewer->gpucubeedge);
        }

        glEnable(GL_TEXTURE_3D);

        auto vertex_shader_code = R"shaderSource(
        #version 110
        uniform mat4 model_matrix;
        uniform mat4 view_matrix;
        uniform mat4 projection_matrix;
        attribute vec3 vertex;
        attribute vec3 texCoordVertex;
        varying vec3 texCoordFrag;
        void main() {
            mat4 mvp_mat = projection_matrix * view_matrix * model_matrix;
            gl_Position = mvp_mat * vec4(vertex, 1.0);
            texCoordFrag = texCoordVertex;
        })shaderSource";

        raw_data_shader.addShaderFromSourceCode(QOpenGLShader::Vertex, vertex_shader_code);
        raw_data_shader.addShaderFromSourceCode(QOpenGLShader::Fragment, R"shaderSource(
        #version 110
        uniform float textureOpacity;
        uniform sampler3D texture;
        varying vec3 texCoordFrag;//in
        //varying vec4 gl_FragColor;//out
        void main() {
            gl_FragColor = vec4(vec3(texture3D(texture, texCoordFrag).r), textureOpacity);
        })shaderSource");

        raw_data_shader.link();

        overlay_data_shader.addShaderFromSourceCode(QOpenGLShader::Vertex, vertex_shader_code);
        overlay_data_shader.addShaderFromSourceCode(QOpenGLShader::Fragment, R"shaderSource(
        #version 110
        uniform float textureOpacity;
        uniform sampler3D indexTexture;
        uniform sampler1D textureLUT;
        uniform float lutSize;//float(textureSize1D(textureLUT, 0));
        uniform float factor;//expand float to uint8 range
        varying vec3 texCoordFrag;//in
        void main() {
            float index = texture3D(indexTexture, texCoordFrag).r;
            index *= factor;
            gl_FragColor = texture1D(textureLUT, (index + 0.5) / lutSize);
            gl_FragColor.a = textureOpacity;
        })shaderSource");

        overlay_data_shader.link();

        glDisable(GL_TEXTURE_3D);
    }
}

void ViewportOrtho::createOverlayTextures() {
    glGenTextures(1, &texture.overlayHandle);

    glBindTexture(GL_TEXTURE_2D, texture.overlayHandle);

    //Set the parameters for the texture.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    const auto size = texture.size;
    std::vector<char> texData(4 * std::pow(state->viewerState->texEdgeLength, 2));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData.data());
}

void ViewportBase::resizeGL(int width, int height) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    GLfloat x = (GLfloat)width / height;

    glFrustum(-x, +x, -1.0, + 1.0, 0.1, 10.0);
    glMatrixMode(GL_MODELVIEW);

    edgeLength = width;
    state->viewer->recalcTextureOffsets();
}

void Viewport3D::paintGL() {
    glClear(GL_DEPTH_BUFFER_BIT);
    renderViewport();
    renderViewportFrontFace();
}

void ViewportOrtho::paintGL() {

    glClear(GL_DEPTH_BUFFER_BIT);
    if (state->gpuSlicer && state->viewer->gpuRendering) {
        renderViewportFast();
    } else {
        state->viewer->vpGenerateTexture(*this);
        renderViewport();
    }
    renderViewportFrontFace();
}

void ViewportBase::enterEvent(QEvent *) {
    hasCursor = true;
}

void ViewportBase::leaveEvent(QEvent *) {
    hasCursor = false;
    emit cursorPositionChanged(Coordinate(), VIEWPORT_UNDEFINED);
}

void ViewportBase::keyPressEvent(QKeyEvent *event) {
    const auto ctrl = event->modifiers().testFlag(Qt::ControlModifier);
    const auto alt = event->modifiers().testFlag(Qt::AltModifier);
    if (ctrl && alt) {
        setCursor(Qt::OpenHandCursor);
    } else if (ctrl) {
        setCursor(Qt::ArrowCursor);
    } else {
        setCursor(Qt::CrossCursor);
    }
    handleKeyPress(event);
}

void ViewportBase::mouseMoveEvent(QMouseEvent *event) {
    const auto mouseBtn = event->buttons();
    const auto penmode = state->viewerState->penmode;

    if((!penmode && mouseBtn == Qt::LeftButton) || (penmode && mouseBtn == Qt::RightButton)) {
        Qt::KeyboardModifiers modifiers = event->modifiers();
        bool ctrl = modifiers.testFlag(Qt::ControlModifier);
        bool alt = modifiers.testFlag(Qt::AltModifier);

        if(ctrl && alt) { // drag viewport around
            moveVP(event->globalPos());
        } else {
            handleMouseMotionLeftHold(event);
        }
    } else if(mouseBtn == Qt::MidButton) {
        handleMouseMotionMiddleHold(event);
    } else if( (!penmode && mouseBtn == Qt::RightButton) || (penmode && mouseBtn == Qt::LeftButton)) {
        handleMouseMotionRightHold(event);
    }
    handleMouseHover(event);

    prevMouseMove = event->pos();
}

void ViewportBase::mousePressEvent(QMouseEvent *event) {
    raise(); //bring this viewport to front
    mouseDown = event->pos();
    auto penmode = state->viewerState->penmode;
    if((penmode && event->button() == Qt::RightButton) || (!penmode && event->button() == Qt::LeftButton)) {
        Qt::KeyboardModifiers modifiers = event->modifiers();
        const auto ctrl = modifiers.testFlag(Qt::ControlModifier);
        const auto alt = modifiers.testFlag(Qt::AltModifier);

        if(ctrl && alt) { // user wants to drag vp
            setCursor(Qt::ClosedHandCursor);
        } else {
            handleMouseButtonLeft(event);
        }
    } else if(event->button() == Qt::MiddleButton) {
        handleMouseButtonMiddle(event);
    } else if((penmode && event->button() == Qt::LeftButton) || (!penmode && event->button() == Qt::RightButton)) {
        handleMouseButtonRight(event);
    }
}

void ViewportBase::mouseReleaseEvent(QMouseEvent *event) {
    EmitOnCtorDtor eocd(&SignalRelay::Signal_Viewort_mouseReleaseEvent, state->signalRelay, this, event);

    Qt::KeyboardModifiers modifiers = event->modifiers();
    const auto ctrl = modifiers.testFlag(Qt::ControlModifier);
    const auto alt = modifiers.testFlag(Qt::AltModifier);

    if (ctrl && alt) {
        setCursor(Qt::OpenHandCursor);
    } else if (ctrl) {
        setCursor(Qt::ArrowCursor);
    } else {
        setCursor(Qt::CrossCursor);
    }

    if(event->button() == Qt::LeftButton) {
        handleMouseReleaseLeft(event);
    }
    if(event->button() == Qt::RightButton) {
        handleMouseReleaseRight(event);
    }
    if(event->button() == Qt::MiddleButton) {
        handleMouseReleaseMiddle(event);
    }
}

void ViewportBase::keyReleaseEvent(QKeyEvent *event) {
    Qt::KeyboardModifiers modifiers = event->modifiers();
    const auto ctrl = modifiers.testFlag(Qt::ControlModifier);
    const auto alt = modifiers.testFlag(Qt::AltModifier);

    if (ctrl && alt) {
        setCursor(Qt::OpenHandCursor);
    } else if (ctrl) {
        setCursor(Qt::ArrowCursor);
    } else {
        setCursor(Qt::CrossCursor);
    }

    if (event->key() == Qt::Key_Shift) {
        state->viewerState->repeatDirection /= 10;// decrease movement speed
        Segmentation::singleton().brush.setInverse(false);
    } else if (!event->isAutoRepeat()) {// don’t let shift break keyrepeat
        state->viewer->userMoveRound();
        state->viewerState->keyRepeat = false;
        state->viewerState->notFirstKeyRepeat = false;
    }
    handleKeyRelease(event);
}

void ViewportOrtho::zoom(const float step) {
    state->viewer->zoom(step);
}

float Viewport3D::zoomStep() const {
    return std::pow(2, 0.25);
}

void Viewport3D::zoom(const float step) {
    auto & zoomLvl = zoomFactor;
    zoomLvl = std::min(std::max(zoomLvl * step, SKELZOOMMIN), SKELZOOMMAX);
    emit updateZoomWidget();
}

void Viewport3D::showHideButtons(bool isShow) {
    ViewportBase::showHideButtons(isShow);
    xyButton.setVisible(isShow);
    xzButton.setVisible(isShow);
    zyButton.setVisible(isShow);
    r90Button.setVisible(isShow);
    r180Button.setVisible(isShow);
    wiggleButton.setVisible(isShow);
    resetButton.setVisible(isShow);
}

void Viewport3D::updateVolumeTexture() {
    auto& seg = Segmentation::singleton();
    int texLen = seg.volume_tex_len;
    if(seg.volume_tex_id == 0) {
        glGenTextures(1, &seg.volume_tex_id);
        glBindTexture(GL_TEXTURE_3D, seg.volume_tex_id);

        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, texLen, texLen, texLen, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    static Profiler tex_gen_profiler;
    static Profiler dcfetch_profiler;
    static Profiler colorfetch_profiler;
    static Profiler occlusion_profiler;
    static Profiler tex_transfer_profiler;

    tex_gen_profiler.start(); // ----------------------------------------------------------- profiling
    auto currentPosDc = state->viewerState->currentPosition / state->magnification / state->cubeEdgeLength;
    int cubeLen = state->cubeEdgeLength;
    int M = state->M;
    int M_radius = (M - 1) / 2;
    GLubyte* colcube = new GLubyte[4*texLen*texLen*texLen];
    std::tuple<uint64_t, std::tuple<uint8_t, uint8_t, uint8_t, uint8_t>> lastIdColor;

    state->protectCube2Pointer.lock();

    dcfetch_profiler.start(); // ----------------------------------------------------------- profiling
    uint64_t** rawcubes = new uint64_t*[M*M*M];
    for(int z = 0; z < M; ++z)
    for(int y = 0; y < M; ++y)
    for(int x = 0; x < M; ++x) {
        auto cubeIndex = z*M*M + y*M + x;
        Coordinate cubeCoordRelative{x - M_radius, y - M_radius, z - M_radius};
        rawcubes[cubeIndex] = reinterpret_cast<uint64_t*>(
            Coordinate2BytePtr_hash_get_or_fail(state->Oc2Pointer[int_log(state->magnification)],
            {currentPosDc.x + cubeCoordRelative.x, currentPosDc.y + cubeCoordRelative.y, currentPosDc.z + cubeCoordRelative.z}));
    }
    dcfetch_profiler.end(); // ----------------------------------------------------------- profiling

    colorfetch_profiler.start(); // ----------------------------------------------------------- profiling

    for(int z = 0; z < texLen; ++z)
    for(int y = 0; y < texLen; ++y)
    for(int x = 0; x < texLen; ++x) {
        Coordinate DcCoord{(x * M)/cubeLen, (y * M)/cubeLen, (z * M)/cubeLen};
        auto cubeIndex = DcCoord.z*M*M + DcCoord.y*M + DcCoord.x;
        auto& rawcube = rawcubes[cubeIndex];

        if(rawcube != nullptr) {
            auto indexInDc  = ((z * M)%cubeLen)*cubeLen*cubeLen + ((y * M)%cubeLen)*cubeLen + (x * M)%cubeLen;
            auto indexInTex = z*texLen*texLen + y*texLen + x;
            auto subobjectId = rawcube[indexInDc];
            if(subobjectId == std::get<0>(lastIdColor)) {
                auto idColor = std::get<1>(lastIdColor);
                colcube[4*indexInTex+0] = std::get<0>(idColor);
                colcube[4*indexInTex+1] = std::get<1>(idColor);
                colcube[4*indexInTex+2] = std::get<2>(idColor);
                colcube[4*indexInTex+3] = std::get<3>(idColor);
            } else if (seg.isSubObjectIdSelected(subobjectId)) {
                auto idColor = seg.colorObjectFromSubobjectId(subobjectId);
                std::get<3>(idColor) = 255; // ignore color alpha
                colcube[4*indexInTex+0] = std::get<0>(idColor);
                colcube[4*indexInTex+1] = std::get<1>(idColor);
                colcube[4*indexInTex+2] = std::get<2>(idColor);
                colcube[4*indexInTex+3] = std::get<3>(idColor);
                lastIdColor = std::make_tuple(subobjectId, idColor);
            } else {
                colcube[4*indexInTex+0] = 0;
                colcube[4*indexInTex+1] = 0;
                colcube[4*indexInTex+2] = 0;
                colcube[4*indexInTex+3] = 0;
            }
        } else {
            auto indexInTex = z*texLen*texLen + y*texLen + x;
            colcube[4*indexInTex+0] = 0;
            colcube[4*indexInTex+1] = 0;
            colcube[4*indexInTex+2] = 0;
            colcube[4*indexInTex+3] = 0;
        }
    }

    delete[] rawcubes;

    state->protectCube2Pointer.unlock();

    colorfetch_profiler.end(); // ----------------------------------------------------------- profiling

    occlusion_profiler.start(); // ----------------------------------------------------------- profiling
    for(int z = 1; z < texLen - 1; ++z)
    for(int y = 1; y < texLen - 1; ++y)
    for(int x = 1; x < texLen - 1; ++x) {
        auto indexInTex = (z)*texLen*texLen + (y)*texLen + x;
        if(colcube[4*indexInTex+3] != 0) {
            for(int xi = -1; xi <= 1; ++xi) {
                auto othrIndexInTex = (z)*texLen*texLen + (y)*texLen + x+xi;
                if((xi != 0) && colcube[4*othrIndexInTex+3] != 0) {
                    colcube[4*indexInTex+0] *= 0.95f;
                    colcube[4*indexInTex+1] *= 0.95f;
                    colcube[4*indexInTex+2] *= 0.95f;
                }
            }
        }
    }

    for(int z = 1; z < texLen - 1; ++z)
    for(int y = 1; y < texLen - 1; ++y)
    for(int x = 1; x < texLen - 1; ++x) {
        auto indexInTex = (z)*texLen*texLen + (y)*texLen + x;
        if(colcube[4*indexInTex+3] != 0) {
            for(int yi = -1; yi <= 1; ++yi) {
                auto othrIndexInTex = (z)*texLen*texLen + (y+yi)*texLen + x;
                if((yi != 0) && colcube[4*othrIndexInTex+3] != 0) {
                    colcube[4*indexInTex+0] *= 0.95f;
                    colcube[4*indexInTex+1] *= 0.95f;
                    colcube[4*indexInTex+2] *= 0.95f;
                }
            }
        }
    }

    for(int z = 1; z < texLen - 1; ++z)
    for(int y = 1; y < texLen - 1; ++y)
    for(int x = 1; x < texLen - 1; ++x) {
        auto indexInTex = (z)*texLen*texLen + (y)*texLen + x;
        if(colcube[4*indexInTex+3] != 0) {
            for(int zi = -1; zi <= 1; ++zi) {
                auto othrIndexInTex = (z+zi)*texLen*texLen + (y)*texLen + x;
                if((zi != 0) && colcube[4*othrIndexInTex+3] != 0) {
                    colcube[4*indexInTex+0] *= 0.95f;
                    colcube[4*indexInTex+1] *= 0.95f;
                    colcube[4*indexInTex+2] *= 0.95f;
                }
            }
        }
    }
    occlusion_profiler.end(); // ----------------------------------------------------------- profiling

    tex_transfer_profiler.start(); // ----------------------------------------------------------- profiling
    glBindTexture(GL_TEXTURE_3D, seg.volume_tex_id);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, texLen, texLen, texLen, 0, GL_RGBA, GL_UNSIGNED_BYTE, colcube);
    delete[] colcube;
    tex_transfer_profiler.end(); // ----------------------------------------------------------- profiling

    tex_gen_profiler.end(); // ----------------------------------------------------------- profiling

    // --------------------- display some profiling information ------------------------
    // qDebug() << "tex gen avg time: " << tex_gen_profiler.average_time()*1000 << "ms";
    // qDebug() << "    dc fetch    : " << dcfetch_profiler.average_time()*1000 << "ms";
    // qDebug() << "    color fetch : " << colorfetch_profiler.average_time()*1000 << "ms";
    // qDebug() << "    occlusion   : " << occlusion_profiler.average_time()*1000 << "ms";
    // qDebug() << "    tex transfer: " << tex_transfer_profiler.average_time()*1000 << "ms";
    // qDebug() << "---------------------------------------------";
}

void ViewportBase::takeSnapshot(const QString & path, const int size, const bool withAxes, const bool withBox, const bool withOverlay, const bool withSkeleton, const bool withScale, const bool withVpPlanes) {
    if (isHidden()) {
        QMessageBox prompt;
        prompt.setIcon(QMessageBox::Question);
        prompt.setText(tr("Please enable the viewport for taking a snapshot of it."));
        const auto *accept = prompt.addButton("Activate && take snapshot", QMessageBox::AcceptRole);
        prompt.addButton("Cancel", QMessageBox::RejectRole);
        prompt.exec();
        if (prompt.clickedButton() == accept) {
            if (viewportType == VIEWPORT_ARBITRARY) {
                state->viewer->setEnableArbVP(true);
            }
            setVisible(true);
        } else {
            return;
        }
    }
    makeCurrent();
    glEnable(GL_MULTISAMPLE);
    glPushAttrib(GL_VIEWPORT_BIT); // remember viewport setting
    glViewport(0, 0, size, size);
    QOpenGLFramebufferObjectFormat format;
    format.setSamples(state->viewerState->sampleBuffers);
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    QOpenGLFramebufferObject fbo(size, size, format);
    const auto options = RenderOptions::snapshotRenderOptions(withAxes, withBox, withOverlay, true, withSkeleton, withVpPlanes);
    fbo.bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Qt does not clear it?
    renderViewport(options);
    if(withScale) {
        setFrontFacePerspective();
        renderScaleBar();
    }
    QImage fboImage(fbo.toImage());
    QImage image(fboImage.constBits(), fboImage.width(), fboImage.height(), QImage::Format_RGB32);
    qDebug() << tr("snapshot ") + (!image.save(path) ? "un" : "") + tr("successful.");
    glPopAttrib(); // restore viewport setting
    fbo.release();
}

void ViewportOrtho::sendCursorPosition() {
    if (hasCursor) {
        const auto cursorPos = mapFromGlobal(QCursor::pos());
        emit cursorPositionChanged(getCoordinateFromOrthogonalClick(cursorPos.x(), cursorPos.y(), *this), viewportType);
    }
}

float ViewportOrtho::displayedEdgeLenghtXForZoomFactor(const float zoomFactor) const {
    float FOVinDCs = ((float)state->M) - 1.f;
    float result = FOVinDCs * state->cubeEdgeLength / static_cast<float>(texture.size);
    return (std::floor((result * zoomFactor) / 2. / texture.texUnitsPerDataPx) * texture.texUnitsPerDataPx)*2;
}

float ViewportArb::displayedEdgeLenghtXForZoomFactor(const float zoomFactor) const {
    float result = displayedIsoPx / static_cast<float>(texture.size);
    return (std::floor((result * zoomFactor) / 2. / texture.texUnitsPerDataPx) * texture.texUnitsPerDataPx)*2;
}


ViewportArb::ViewportArb(QWidget *parent, ViewportType viewportType) : ViewportOrtho(parent, viewportType) {
    menuButton.addAction(&resetAction);
    connect(&resetAction, &QAction::triggered, [this]() {
        state->viewer->resetRotation();
    });
}

void ViewportArb::paintGL() {
    glClear(GL_DEPTH_BUFFER_BIT);
    if (state->gpuSlicer && state->viewer->gpuRendering) {
        state->viewer->arbCubes(*this);
    } else if (Segmentation::enabled && state->viewerState->showOnlyRawData == false) {
        updateOverlayTexture();
    }
    ViewportOrtho::paintGL();
}

void ViewportArb::hideVP() {
    ViewportBase::hideVP();
    state->viewer->setEnableArbVP(false);
}
void ViewportArb::showHideButtons(bool isShow) {
    ViewportBase::showHideButtons(isShow);
}

void ViewportArb::updateOverlayTexture() {
    if (!ocResliceNecessary) {
        return;
    }
    ocResliceNecessary = false;
    const int width = (state->M - 1) * state->cubeEdgeLength / std::sqrt(2);
    const int height = width;
    const auto begin = leftUpperPxInAbsPx_float;
    std::vector<char> texData(4 * std::pow(state->viewerState->texEdgeLength, 2));
    boost::multi_array_ref<uint8_t, 3> viewportView(reinterpret_cast<uint8_t *>(texData.data()), boost::extents[width][height][4]);
    for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
        const auto dataPos = static_cast<Coordinate>(begin + v1 * state->magnification * x - v2 * state->magnification * y);
        if (dataPos.x < 0 || dataPos.y < 0 || dataPos.z < 0) {
            viewportView[y][x][0] = viewportView[y][x][1] = viewportView[y][x][2] = viewportView[y][x][3] = 0;
        } else {
            const auto soid = readVoxel(dataPos);
            const auto color = Segmentation::singleton().colorObjectFromSubobjectId(soid);
            viewportView[y][x][0] = std::get<0>(color);
            viewportView[y][x][1] = std::get<1>(color);
            viewportView[y][x][2] = std::get<2>(color);
            viewportView[y][x][3] = std::get<3>(color);
        }
    }
    glBindTexture(GL_TEXTURE_2D, texture.overlayHandle);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, texData.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}
