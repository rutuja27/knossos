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

#ifndef SESSION_H
#define SESSION_H

#include "coordinate.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPair>
#include <QString>
#include <QTimer>

enum class GUIMode {
    None,
    ProofReading
};

enum class AnnotationMode {
    NodeEditing = 1 << 0,
    LinkedNodes = 1 << 1,
    SkeletonCycles = 1 << 2,
    Brush = 1 << 3,
    ObjectSelection = 1 << 4,
    ObjectMerge = 1 << 5,

    Mode_Tracing = (1 << 6) | NodeEditing,
    Mode_TracingAdvanced = (1 << 7) | NodeEditing | SkeletonCycles,

    Mode_Paint = (1 << 8) | Brush | ObjectSelection,
    Mode_Merge = (1 << 9) | Brush | ObjectSelection | ObjectMerge,
    Mode_MergeSimple = (1 << 10) | Brush | ObjectMerge,

    Mode_MergeTracing = (1 << 11) | NodeEditing | LinkedNodes | SkeletonCycles,

    Mode_Selection = (1 << 12) | Mode_TracingAdvanced | ObjectSelection,
};

class Session : public QObject {
    Q_OBJECT
    class ActivityEventFilter;

    static const int TIME_SLICE_MS = 60 * 1000;

    int annotationTimeMilliseconds = 0;
    bool timeSliceActivity = false;
    QTimer annotationTimer;
    QElapsedTimer lastTimeSlice;
    void handleTimeSlice();

public:
    QTimer autoSaveTimer;
    bool autoFilenameIncrementBool = true;
    bool savePlyAsBinary{true};
    bool unsavedChanges = false;

    QPair<QString, QString> task;
    QFlags<AnnotationMode> annotationMode;
    GUIMode guiMode{GUIMode::None};

    Coordinate movementAreaMin; // Center of movement area
    Coordinate movementAreaMax; // Range around movement center for every dimension
    bool outsideMovementArea(const Coordinate &pos);
    void updateMovementArea(const Coordinate &min, const Coordinate &max);
    void resetMovementArea();

    QString annotationFilename;

    Session();
    static Session & singleton() {
        static Session session;
        return session;
    }
    decltype(annotationTimeMilliseconds) getAnnotationTime() const;
    void setAnnotationTime(const decltype(annotationTimeMilliseconds) & ms);
    decltype(annotationTimeMilliseconds) currentTimeSliceMs() const;

signals:
    void annotationTimeChanged(const QString & timeString);
    void autoSaveSignal();
    void movementAreaChanged();
};

#endif//SESSION_H
