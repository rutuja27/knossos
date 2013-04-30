#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/*
 *  This file is a part of KNOSSOS.
 *
 *  (C) Copyright 2007-2013
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
 */

/*
 * For further information, visit http://www.knossostool.org or contact
 *     Joergen.Kornfeld@mpimf-heidelberg.mpg.de or
 *     Fabian.Svara@mpimf-heidelberg.mpg.de
 */


#define FILE_DIALOG_HISTORY_MAX_ENTRIES 10
#include <QMainWindow>
#include <QQueue>

namespace Ui {
class MainWindow;
}

class QLabel;
class QToolBar;
class QSettings;
class QPushButton;
class QSpinBox;
class QMessageBox;
class QGridLayout;
class QFileDialog;
class QFile;

class Console;
class TracingTimeWidget;
class CommentsWidget;
class DataSavingWidget;
class SplashScreenWidget;
class SynchronizationWidget;
class ToolsWidget;
class ViewportSettingsWidget;
class ZoomAndMultiresWidget;
class Viewport;
class NavigationWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void closeEvent(QCloseEvent *event);
    void updateTitlebar(bool useFilename);

    static bool cpBaseDirectory(char *target, char *path, size_t len);
    static bool addRecentFile(char *path, uint32_t pos);
    bool addRecentFile(QString fileName);

    //static void saveSkelCallback(AG_Event *event);
    static void UI_saveSkeleton(int32_t increment);
    static void UI_saveSettings();
    void loadSkeleton();

    static void reloadDataSizeWin();

    static void treeColorAdjustmentsChanged();
    static void datasetColorAdjustmentsChanged();

    void showSplashScreen();
    void showAboutScreen();
    void createConsoleWidget();
    void createTracingTimeWidget();
    void createCommentsWidget();
    void createZoomAndMultiresWidget();
    void createNavigationWidget();
    void createViewportSettingsWidget();
    void createToolWidget();
    void createDataSavingWidget();
    void createSychronizationWidget();

    void createXYViewport();
    void createXZViewport();
    void createYZViewport();
    void createSkeletonViewport();
    bool eventFilter(QObject *obj, QEvent *event);

    void loadDefaultPrefs();
signals:
    void changeDatasetMagSignal(int32_t serverMovement);
    void recalcTextureOffsetsSignal();
    void updatePositionSignal(int32_t serverMovement);
    void refreshViewportsSignal();

protected:


private:
    Ui::MainWindow *ui;

    QToolBar *toolBar;
    QPushButton *copyButton;
    QPushButton *pasteButton;
    QLabel *xLabel, *yLabel, *zLabel;
    QSpinBox *xField, *yField, *zField;

    QMessageBox *prompt;

    QWidget *mainWidget;
    QGridLayout *gridLayout;
    Viewport **viewports;

    /* Dialogs */
    Console *console;
    TracingTimeWidget *tracingTimeWidget;
    CommentsWidget *commentsWidget;
    ZoomAndMultiresWidget *zoomAndMultiresWidget;
    NavigationWidget *navigationWidget;
    ToolsWidget *toolsWidget;
    ViewportSettingsWidget *viewportSettingsWidget;

    DataSavingWidget *dataSavingWidget;
    SynchronizationWidget *synchronizationWidget;
    SplashScreenWidget *splashWidget;

    /* file actions */
    QAction *recentFileAction;
    QAction **historyEntryActions;

    /* edit skeleton actions */
    QAction *addNodeAction;
    QAction *linkWithActiveNodeAction;
    QAction *dropNodesAction;
    QAction *skeletonStatisticsAction;
    QAction *clearSkeletonAction;

    /* view actions */
    QAction *workModeViewAction;
    QAction *dragDatasetAction;
    QAction *recenterOnClickAction;
    QAction *zoomAndMultiresAction;
    QAction *tracingTimeAction;

    /* preferences actions */
    QAction *loadCustomPreferencesAction;
    QAction *saveCustomPreferencesAction;
    QAction *defaultPreferencesAction;
    QAction *datasetNavigationAction;
    QAction *synchronizationAction;
    QAction *dataSavingOptionsAction;
    QAction *viewportSettingsAction;

    /* window actions */
    QAction *toolsAction;

    QAction *logAction;
    QAction *commentShortcutsAction;

    /* help actions */
    QAction *aboutAction;

    /* Qmenu-points */
    QMenu *fileMenu;
    QMenu *recentFileMenu;
    QMenu *editMenu;
    QMenu *workModeEditMenu;
    QMenu *viewMenu;
    QMenu *workModeViewMenu;
    QMenu *preferenceMenu;
    QMenu *windowMenu;
    QMenu *helpMenu;

    QFileDialog *skeletonFileDialog;
    QQueue<QString> *skeletonFileHistory;
    QFileDialog *saveFileDialog;
    QFile *loadedFile;
    void updateFileHistoryMenu();

    void createActions();
    void createMenus();
    void createCoordBarWin();

    QSettings *settings;
    void saveSettings();
    void loadSettings();

private slots:
    /* file menu */
    void openSlot();
    void recentFilesSlot(int index);
    void saveSlot();
    void saveAsSlot();
    void quitSlot();

    /* edit skeleton menu*/
    void addNodeSlot();
    void linkWithActiveNodeSlot();
    void dropNodesSlot();
    void skeletonStatisticsSlot();
    void clearSkeletonSlot();

    /* view menu */
    void dragDatasetSlot();
    void recenterOnClickSlot();
    void zoomAndMultiresSlot();
    void tracingTimeSlot();

    /* preferences menu */
    void loadCustomPreferencesSlot();
    void saveCustomPreferencesSlot();
    void defaultPreferencesSlot();
    void datatasetNavigationSlot();
    void synchronizationSlot();
    void dataSavingOptionsSlot();
    void viewportSettingsSlot();

    /* window menu */
    void toolsSlot();
    void logSlot();
    void commentShortcutsSlots();

    /* help menu */
    void aboutSlot();

    /* toolbar slots */
    void copyClipboardCoordinates();
    void pasteClipboardCoordinates();
    void xCoordinateChanged(int value);
    void yCoordinateChanged(int value);
    void zCoordinateChanged(int value);

    void zoomOrthogonals(float step);
    void uncheckToolsAction();
    void uncheckViewportSettingAction();
    void uncheckCommentShortcutsAction();
    void uncheckConsoleAction();
    void uncheckTracingTimeAction();
    void uncheckZoomAndMultiresAction();
    void uncheckDataSavingAction();

    void uncheckSynchronizationAction();
    void uncheckNavigationAction();

};

#endif // MAINWINDOW_H
