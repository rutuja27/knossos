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

#ifndef SEGMENTATIONVIEW_H
#define SEGMENTATIONVIEW_H

#include "segmentation/segmentation.h"
#include "widgets/PreventDeferredDelete.h"
#include "widgets/UserOrientableSplitter.h"

#include <QAbstractListModel>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>
#include <QMenu>

#include <functional>

class CategoryDelegate : public QStyledItemDelegate {
    mutable PreventDeferredDelete<QComboBox> box;
public:
    CategoryDelegate(class CategoryModel & categoryModel);
    QWidget * createEditor(QWidget * parent, const QStyleOptionViewItem & option, const QModelIndex & index) const override;
};

class SegmentationObjectModel : public QAbstractListModel {
Q_OBJECT
    friend class SegmentationView;//selection
protected:
    //rutuja
    const std::vector<QString> header{"on-off","color", "Object ID", "Lock", "Category", "Comment", "#", "Subobject IDs"};
    const std::size_t MAX_SHOWN_SUBOBJECTS = 10;
public:
    virtual int rowCount(const QModelIndex & parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex & parent = QModelIndex()) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant objectGet(const Segmentation::Object & obj, const QModelIndex & index, int role) const;
    virtual QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
    bool objectSet(Segmentation::Object & obj, const QModelIndex & index, const QVariant & value, int role);
    virtual bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole) override;
    virtual Qt::ItemFlags flags(const QModelIndex & index) const override;
    void recreate();
    void appendRowBegin();
    void popRowBegin();
    void appendRow();
    void popRow();
    void changeRow(int idx);
};

class TouchedObjectModel : public SegmentationObjectModel {
    Q_OBJECT
public:
    std::vector<std::reference_wrapper<Segmentation::Object>> objectCache;
    virtual int rowCount(const QModelIndex & parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole) override;
    void recreate();
};

class CategoryModel : public QAbstractListModel {
    Q_OBJECT
    std::vector<QString> categoriesCache;
public:
    virtual int rowCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    void recreate();
};

class SegmentationView : public QWidget {
Q_OBJECT
    QVBoxLayout layout;
    QHBoxLayout toolsLayout;
    QButtonGroup modeGroup;
    QLabel brushRadiusLabel{"Brush radius"};
    QSpinBox brushRadiusEdit;
    QPushButton twodBtn{"2D"};
    QPushButton threedBtn{"3D"};
    QCheckBox showOnlySelectedChck{"Show only selected objects"};
    QHBoxLayout filterLayout;
    CategoryModel categoryModel;
    QComboBox categoryFilter;
    QLineEdit commentFilter;
    QCheckBox regExCheckbox{"Regex"};

    //rutuja
    QPushButton objectCreateButton{"Create new object"};

    SegmentationObjectModel objectModel;
    QSortFilterProxyModel objectProxyModelCategory;
    QSortFilterProxyModel objectProxyModelComment;
    TouchedObjectModel touchedObjectModel;

    CategoryDelegate categoryDelegate;

    UserOrientableSplitter splitter;
    QWidget touchedLayoutWidget;
    QHBoxLayout touchedTableLayout;
    QLabel touchedObjectsLabel{"<strong>Objects containing subobject</strong>"};

    //rutuja
    QLabel ObjectsLabel{"objectsTable"};

    QTreeView touchedObjsTable;
    QTreeView objectsTable;
    QMenu touchedObjsContextMenu{&touchedObjsTable};
    QMenu objectsContextMenu{&objectsTable};
    int objSortSectionIndex;
    int touchedObjSortSectionIndex;
    QHBoxLayout bottomHLayout;
    QLabel objectCountLabel;
    QLabel subobjectCountLabel;
    QLabel subobjectHoveredLabel;

    QColorDialog colorDialog{this};

    bool objectSelectionProtection = false;
    bool touchedObjectSelectionProtection = false;

    //rutuja
    QWidget objectLayoutWidget;
    QHBoxLayout objectTableLayout;
public:
    explicit SegmentationView(QWidget * const parent = nullptr);
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void touchedObjSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void updateSelection();
    void updateTouchedObjSelection();
    void updateLabels();
    uint64_t indexFromRow(const SegmentationObjectModel & model, const QModelIndex index) const;
    uint64_t indexFromRow(const TouchedObjectModel & model, const QModelIndex index) const;
public slots:
    void filter();
};

#endif//SEGMENTATIONVIEW_H
