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

#ifndef DATASETLOADWIDGET_H
#define DATASETLOADWIDGET_H

#include "coordinate.h"
#include "loader.h"
#include "widgets/DialogVisibilityNotify.h"
#include "widgets/UserOrientableSplitter.h"
#include "widgets/viewport.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QTextDocument>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

class FOVSpinBox : public QSpinBox {
public:
    uint cubeEdge{128};

    virtual QValidator::State validate(QString &input, int &pos) const override {
        if (QSpinBox::validate(input, pos) == QValidator::Invalid) {
            return QValidator::Invalid;
        }
        auto number = valueFromText(input);
        return ((number % cubeEdge == 0) && ((number / cubeEdge) % 2 == 0)) ? QValidator::Acceptable : QValidator::Intermediate;
    }

    virtual void fixup(QString &input) const override {
        auto number = valueFromText(input);
        auto ratio = static_cast<int>(std::floor(static_cast<float>(number) / cubeEdge));
        if (ratio % 2 == 0) {
            input = textFromValue(ratio * cubeEdge);
        } else {
            input = textFromValue((ratio + 1) * cubeEdge);
        }
    }
};

class QScrollArea;
class QPushButton;

class DatasetLoadWidget : public DialogVisibilityNotify {
    Q_OBJECT

    QVBoxLayout mainLayout;
    UserOrientableSplitter splitter;
    QTableWidget tableWidget;
    QLabel infoLabel;
    QGroupBox datasetSettingsGroup;
    QFormLayout datasetSettingsLayout;
    FOVSpinBox fovSpin;
    QLabel superCubeSizeLabel;
    QLabel cubeEdgeLabel{"Cubesize"};
    QSpinBox cubeEdgeSpin;
    QCheckBox segmentationOverlayCheckbox{"load segmentation overlay"};
    QLabel reloadRequiredLabel{tr("Reload dataset for changes to take effect.")};
    QHBoxLayout buttonHLayout;
    QPushButton processButton{"Load Dataset"};
    QPushButton cancelButton{"Close"};
public:
    QUrl datasetUrl;//meh

    explicit DatasetLoadWidget(QWidget *parent = 0);
    void changeDataset(bool isGUI);
    bool loadDataset(const boost::optional<bool> loadOverlay = boost::none, QUrl path = {}, const bool silent = false);
    void saveSettings();
    void loadSettings();
    void applyGeometrySettings();
    void updateDatasetInfo();
    void insertDatasetRow(const QString & dataset, const int pos);
    void datasetCellChanged(int row, int col);
    QStringList getRecentPathItems();
    int change_seglevels(int lvl, QSpinBox* x, QSpinBox* y, QSpinBox* z);

signals:
    void updateDatasetCompression();
    void datasetChanged(bool showOverlays);
    void datasetSwitchZoomDefaults();
public slots:
    void adaptMemoryConsumption();
    void processButtonClicked();
};

#endif // DATASETLOADWIDGET_H
