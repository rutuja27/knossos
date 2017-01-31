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

#include "coordinate.h"
#include "dataset.h"
#include "scriptengine/scripting.h"
#include "version.h"
#include "viewer.h"
#include "widgets/mainwindow.h"
#include "widgets/viewport.h"

#include <QApplication>
#include <QFileInfo>
#include <QScreen>
#include <QSplashScreen>
#include <QStandardPaths>
#include <QStyleFactory>

#include <iostream>
#include <fstream>

// obsolete with CMAKE_AUTOSTATICPLUGINS in msys2
//#if defined(Q_OS_WIN) && defined(QT_STATIC)
//#include <QtPlugin>
//Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
//#endif

class Splash : public QSplashScreen {
public:
    Splash(const QString & img_filename) : QSplashScreen(QPixmap(img_filename), Qt::WindowStaysOnTopHint) {
        show();
    }
};

void debugMessageHandler(QtMsgType type, const QMessageLogContext & context, const QString & msg) {
    QString intro;
    switch (type) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
    case QtInfoMsg:     intro = QString("Info: ");     break;
#endif
    case QtDebugMsg:    intro = QString("Debug: ");    break;
    case QtWarningMsg:  intro = QString("Warning: ");  break;
    case QtCriticalMsg: intro = QString("Critical: "); break;
    case QtFatalMsg:    intro = QString("Fatal: ");    break;
    }
    QString txt = QString("[%1:%2] \t%4%5").arg(QFileInfo(context.file).fileName()).arg(context.line).arg(intro).arg(msg);
    //open the file once
    static std::ofstream outFile(QStandardPaths::writableLocation(QStandardPaths::DataLocation).toStdString()+"/log.txt", std::ios_base::app);
    static int64_t file_debug_output_limit = 5000;
    if (type != QtDebugMsg || --file_debug_output_limit > 0) {
        outFile   << txt.toStdString() << std::endl;
    }
    std::cout << txt.toStdString() << std::endl;

    if (type == QtFatalMsg) {
        std::abort();
    }
}

Q_DECLARE_METATYPE(std::string)

int main(int argc, char *argv[]) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);//explicitly enable sharing for undocked viewports
#ifdef Q_OS_OSX
    const auto end = std::next(argv, argc);
    bool styleOverwrite = std::find_if(argv, end, [](const auto & argvv){
        return QString::fromUtf8(argvv).contains("-style");
    }) != end;
#endif
    QApplication a(argc, argv);

    QFile file(":resources/style.qss");
    file.open(QFile::ReadOnly);
    QString styleSheet = QLatin1String(file.readAll());
    file.close();
    a.setStyleSheet(QString(styleSheet));
#ifdef Q_OS_OSX
    if (!styleOverwrite) {// default to Fusion style on OSX if nothing contrary was specified (because the default theme looks bad)
        QApplication::setStyle(QStyleFactory::create("Fusion"));
    }
#endif
    qInstallMessageHandler(debugMessageHandler);
    /* On OSX there is the problem that the splashscreen nevers returns and it prevents the start of the application.
       I searched for the reason and found this here : https://bugreports.qt-project.org/browse/QTBUG-35169
       As I found out randomly that effect does not occur if the splash is invoked directly after the QApplication(argc, argv)
    */
#ifdef NDEBUG
    Splash splash(((QGuiApplication *)QApplication::instance())->primaryScreen()->devicePixelRatio() == 1 ? ":resources/splash.png" : ":resources/splash@2x.png");
#endif
    QCoreApplication::setOrganizationDomain("knossostool.org");
    QCoreApplication::setOrganizationName("MPIN");
    QCoreApplication::setApplicationName(QString("KNOSSOS %1").arg(KVERSION));
    QSettings::setDefaultFormat(QSettings::IniFormat);

    qRegisterMetaType<std::string>();
    qRegisterMetaType<Coordinate>();
    qRegisterMetaType<CoordOfCube>();
    qRegisterMetaType<floatCoordinate>();

    qRegisterMetaType<UserMoveType>();
    qRegisterMetaType<ViewportType>();

    stateInfo state;
    ::state = &state;
    Dataset::dummyDataset().applyToState();

    SignalRelay signalRelay;
    Viewer viewer;
    Scripting scripts;
    state.mainWindow->loadSettings();// load settings after viewer and window are accessible through state and viewer
    state.mainWindow->widgetContainer.datasetLoadWidget.loadDataset();// load last used dataset or show


    viewer.run();
#ifdef NDEBUG
    splash.finish(state.mainWindow);
#endif
    return a.exec();
}
