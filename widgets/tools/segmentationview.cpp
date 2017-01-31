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

#include "segmentationview.h"

#include "model_helper.h"
#include "viewer.h"

#include <QApplication>
#include <QEvent>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QString>

#include <chrono>

CategoryDelegate::CategoryDelegate(CategoryModel & categoryModel) {
    box.setModel(&categoryModel);
    box.setEditable(true);//support custom categories
    //we don’t wanna insert values here, they get added in Segmentation::changeCategory
    //ENTER after typing a non-existent value doesn’t work otherwise
    box.setInsertPolicy(QComboBox::NoInsert);
}

QWidget * CategoryDelegate::createEditor(QWidget * parent, const QStyleOptionViewItem &, const QModelIndex &) const {
    box.setParent(parent);//parent is needed for proper placement
    return &box;
}

int TouchedObjectModel::rowCount(const QModelIndex &) const {
    return objectCache.size();
}

QVariant TouchedObjectModel::data(const QModelIndex & index, int role) const {
    if (index.isValid()) {
        //http://coliru.stacked-crooked.com/a/98276b01d551fb41
        const auto & obj = objectCache[index.row()].get();
        return objectGet(obj, index, role);
    }
    return QVariant();//return invalid QVariant
}

bool TouchedObjectModel::setData(const QModelIndex & index, const QVariant & value, int role) {
    if (index.isValid()) {
        auto & obj = objectCache[index.row()].get();
        return objectSet(obj, index, value, role);
    }
    return true;
}

void TouchedObjectModel::recreate() {
    beginResetModel();
    objectCache = Segmentation::singleton().touchedObjects();
    endResetModel();
}

int SegmentationObjectModel::rowCount(const QModelIndex &) const {
    return Segmentation::singleton().objects.size();
}

int SegmentationObjectModel::columnCount(const QModelIndex &) const {
    return header.size();
}

QVariant SegmentationObjectModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        return header[section];
    } else {
        return QVariant();//return invalid QVariant
    }
}

QVariant SegmentationObjectModel::objectGet(const Segmentation::Object &obj, const QModelIndex & index, int role) const {
    //rutuja - extra column added for the branch on-off functionality
    if(index.column() == 0 && role == Qt::CheckStateRole){
        return (obj.on_off ? Qt::Checked : Qt::Unchecked);
    } else if (index.column() == 1 && (role == Qt::BackgroundRole || role == Qt::DecorationRole)) {
        const auto color = Segmentation::singleton().colorObjectFromIndex(obj.index);
        return QColor(std::get<0>(color), std::get<1>(color), std::get<2>(color));
    } else if (index.column() == 3 && role == Qt::CheckStateRole) {
        return (obj.immutable ? Qt::Checked : Qt::Unchecked);
    } else if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case 2: return static_cast<quint64>(obj.id);
        case 4: return obj.category;
        case 5: return obj.comment;
        case 6: return static_cast<quint64>(obj.subobjects.size());
        case 7: {
            QString output;
            const auto elemCount = std::min(MAX_SHOWN_SUBOBJECTS, obj.subobjects.size());
            auto subobjectIt = std::begin(obj.subobjects);
            for (std::size_t i = 0; i < elemCount; ++i) {
                output += QString::number(subobjectIt->get().id) + ", ";
                subobjectIt = std::next(subobjectIt);
            }
            output.chop(2);
            output += (obj.subobjects.size() > MAX_SHOWN_SUBOBJECTS) ? "…" : "";;
            return output;
        }
        }
    }
    return QVariant();//return invalid QVariant
}

QVariant SegmentationObjectModel::data(const QModelIndex & index, int role) const {
    if (index.isValid()) {
        const auto & obj = Segmentation::singleton().objects[index.row()];
        return objectGet(obj, index, role);
    }
    return QVariant();//return invalid QVariant
}

bool SegmentationObjectModel::objectSet(Segmentation::Object & obj, const QModelIndex & index, const QVariant & value, int role) {
    if (index.column() == 3 && role == Qt::CheckStateRole) {
        QMessageBox prompt;
        prompt.setWindowFlags(Qt::WindowStaysOnTopHint);
        prompt.setIcon(QMessageBox::Question);
        const auto lock = obj.immutable ? tr("Unlock") : tr("Lock");
        prompt.setWindowTitle(lock + tr(" Object"));
        prompt.setText(lock + tr(" the object with id %1?").arg(obj.id));
        const auto & lockButton = prompt.addButton(lock, QMessageBox::YesRole);
        prompt.addButton(tr("Cancel"), QMessageBox::NoRole);
        prompt.exec();
        if (prompt.clickedButton() == lockButton) {
            obj.immutable = value.toBool();
        }
    } else if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case 4: Segmentation::singleton().changeCategory(obj, value.toString()); break;
        case 5: Segmentation::singleton().changeComment(obj, value.toString()); break;
        default:
            return false;
        }
    }else if (index.column() == 0 && role == Qt::CheckStateRole){//rutuja

        if(value == Qt::Checked)
        {
          obj.on_off = Qt::Checked;
        }
        else{

          obj.on_off =Qt::Unchecked;
        }
        auto & seg = Segmentation::singleton();
        seg.branch_onoff(obj);


    }
    return true;
}

bool SegmentationObjectModel::setData(const QModelIndex & index, const QVariant & value, int role) {
    if (index.isValid()) {
        auto & obj = Segmentation::singleton().objects[index.row()];
        return objectSet(obj, index, value, role);
    }
    return true;
}

Qt::ItemFlags SegmentationObjectModel::flags(const QModelIndex & index) const {
    Qt::ItemFlags flags = QAbstractItemModel::flags(index) | Qt::ItemNeverHasChildren;//not editable
    switch(index.column()) {
    case 0 : flags | Qt::ItemIsUserCheckable;//rutuja
    case 2:

    case 3:
        return flags | Qt::ItemIsUserCheckable;
    case 4:
        return flags | Qt::ItemIsEditable;
    }
    return flags;
}

void SegmentationObjectModel::recreate() {
    beginResetModel();
    endResetModel();
}

void SegmentationObjectModel::appendRowBegin() {
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
}

void SegmentationObjectModel::popRowBegin() {
    beginRemoveRows(QModelIndex(), rowCount()-1, rowCount()-1);
}

void SegmentationObjectModel::appendRow() {
    endInsertRows();
}

void SegmentationObjectModel::popRow() {
    endRemoveRows();
}

void SegmentationObjectModel::changeRow(int idx) {
    emit dataChanged(index(idx, 0), index(idx, columnCount()-1));
}

void CategoryModel::recreate() {
    beginResetModel();
    categoriesCache.clear();
    for (auto & category : Segmentation::singleton().categories) {
        categoriesCache.emplace_back(category);
    }
    std::sort(std::begin(categoriesCache), std::end(categoriesCache));
    endResetModel();
}

int CategoryModel::rowCount(const QModelIndex &) const {
    return categoriesCache.size();
}

QVariant CategoryModel::data(const QModelIndex &index, int role) const {
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        return categoriesCache[index.row()];
    }
    return QVariant();
}

class scope {
    bool & protection;
    bool prev;
public:
    operator bool() & {// be sure not to use a temporary scope object
        return !prev;
    }
    scope(bool & protection) : protection(protection) {
        prev = protection;
        protection = true;
    }
    ~scope() {
        protection = prev ? protection : false;
    }
};

SegmentationView::SegmentationView(QWidget * const parent) : QWidget(parent), categoryDelegate(categoryModel) {
    modeGroup.addButton(&twodBtn, 0);
    modeGroup.addButton(&threedBtn, 1);

    twodBtn.setCheckable(true);
    threedBtn.setCheckable(true);

    twodBtn.setToolTip("Only work on one 2D slice.");
    threedBtn.setToolTip("Apply changes on several consecutive slices.");

    brushRadiusEdit.setRange(1, 1000);
    brushRadiusEdit.setValue(Segmentation::singleton().brush.getRadius());
    twodBtn.setChecked(true);

    toolsLayout.addWidget(&showOnlySelectedChck);
    toolsLayout.addStretch();
    toolsLayout.addStretch();
    toolsLayout.addWidget(&brushRadiusLabel);
    toolsLayout.addWidget(&brushRadiusEdit);
    toolsLayout.addStretch();
    toolsLayout.addWidget(&twodBtn);
    toolsLayout.addWidget(&threedBtn);
    layout.addLayout(&toolsLayout);

    categoryModel.recreate();
    categoryFilter.setModel(&categoryModel);
    categoryFilter.setEditable(true);
    categoryFilter.lineEdit()->setPlaceholderText("Category");
    commentFilter.setPlaceholderText("Filter for comment...");
    showOnlySelectedChck.setChecked(Segmentation::singleton().renderOnlySelectedObjs);

    auto setupTable = [this](auto & table, auto & model, auto & sortIndex){
        table.setModel(&model);
        table.setAllColumnsShowFocus(true);
        table.setContextMenuPolicy(Qt::CustomContextMenu);
        table.setUniformRowHeights(true);//perf hint from doc
        table.setRootIsDecorated(false);//remove padding to the left of each cell’s content
        table.setSelectionMode(QAbstractItemView::ExtendedSelection);
        table.setItemDelegateForColumn(3, &categoryDelegate);
        table.setSortingEnabled(true);
        table.sortByColumn(sortIndex = 1, Qt::SortOrder::AscendingOrder);
    };

    setupTable(touchedObjsTable, touchedObjectModel, touchedObjSortSectionIndex);
    touchedLayoutWidget.hide();
    //rutuja
    setupTable(objectsTable, objectProxyModelComment, touchedObjSortSectionIndex);
    objectLayoutWidget.hide();

    //proxy model chaining, so we can filter twice
    objectProxyModelCategory.setSourceModel(&objectModel);
    objectProxyModelComment.setSourceModel(&objectProxyModelCategory);
    objectProxyModelCategory.setFilterKeyColumn(3);
    objectProxyModelComment.setFilterKeyColumn(4);


    filterLayout.addWidget(&categoryFilter);
    filterLayout.addWidget(&commentFilter);
    filterLayout.addWidget(&regExCheckbox);

    bottomHLayout.addWidget(&objectCountLabel);
    bottomHLayout.addWidget(&subobjectCountLabel);
    bottomHLayout.addWidget(&subobjectHoveredLabel);
    //rutuja
    bottomHLayout.addWidget(&objectCreateButton, 0, Qt::AlignRight);

    touchedTableLayout.addWidget(&touchedObjectsLabel);
    touchedTableLayout.addWidget(&touchedObjsTable);
    touchedLayoutWidget.setLayout(&touchedTableLayout);

    //rutuja-add active object window
    objectTableLayout.addWidget(&ObjectsLabel);
    objectTableLayout.addWidget(&objectsTable);
    objectLayoutWidget.setLayout(&objectTableLayout);

    splitter.setOrientation(Qt::Horizontal);
    splitter.addWidget(&touchedObjsTable);
    splitter.addWidget(&touchedLayoutWidget);
    splitter.setStretchFactor(0, 5);

    //rutuja -set the active object window
    splitter.setOrientation(Qt::Vertical);
    splitter.addWidget(&objectsTable);
    splitter.addWidget(&objectLayoutWidget);
    splitter.setStretchFactor(0, 5);

    layout.addLayout(&filterLayout);
    layout.addWidget(&splitter);
    layout.addLayout(&bottomHLayout);
    setLayout(&layout);

    QObject::connect(&brushRadiusEdit, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), [](int value){
        Segmentation::singleton().brush.setRadius(value);
    });
    QObject::connect(&modeGroup, static_cast<void(QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), [](int id){
        Segmentation::singleton().brush.setMode(static_cast<brush_t::mode_t>(id));
    });
    QObject::connect(&Segmentation::singleton().brush, &brush_subject::modeChanged, [this](brush_t::mode_t value){
        modeGroup.button(static_cast<int>(value))->setChecked(true);
    });
    QObject::connect(&Segmentation::singleton().brush, &brush_subject::radiusChanged, [this](int value){
        brushRadiusEdit.setValue(value);
    });

    QObject::connect(&categoryFilter,  &QComboBox::currentTextChanged, this, &SegmentationView::filter);
    QObject::connect(&commentFilter, &QLineEdit::textEdited, this, &SegmentationView::filter);
    QObject::connect(&regExCheckbox, &QCheckBox::stateChanged, this, &SegmentationView::filter);

    for (const auto & index : {0, 1, 2}) {
        //resize once, constantly resizing slows down selection and scroll to considerably
        touchedObjsTable.resizeColumnToContents(index);
        objectsTable.resizeColumnToContents(index);
    }

    QObject::connect(&Segmentation::singleton(), &Segmentation::beforeAppendRow, &objectModel, &SegmentationObjectModel::appendRowBegin);
    QObject::connect(&Segmentation::singleton(), &Segmentation::beforeRemoveRow, [this](){
        objectSelectionProtection = true;
        objectModel.popRowBegin();
        if (Segmentation::singleton().objects.back().selected) {
            const auto index = Segmentation::singleton().objects.back().index;
            const auto & proxyIndex = objectProxyModelComment.mapFromSource(objectProxyModelCategory.mapFromSource(objectModel.index(index, 0)));
            objectsTable.selectionModel()->select(proxyIndex, QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
        }
        objectSelectionProtection = false;
        touchedObjectModel.recreate();
        updateTouchedObjSelection();
        updateLabels();
    });
    QObject::connect(&Segmentation::singleton(), &Segmentation::appendedRow, [this](){
        objectSelectionProtection = true;
        objectModel.appendRow();
        if (Segmentation::singleton().objects.back().selected) {
            const auto index = Segmentation::singleton().objects.back().index;
            const auto & proxyIndex = objectProxyModelComment.mapFromSource(objectProxyModelCategory.mapFromSource(objectModel.index(index, 0)));
            objectsTable.selectionModel()->setCurrentIndex(proxyIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
        objectSelectionProtection = false;
        touchedObjectModel.recreate();
        updateTouchedObjSelection();
        updateLabels();
    });
    QObject::connect(&Segmentation::singleton(), &Segmentation::removedRow, [this](){
        objectModel.popRow();
        touchedObjectModel.recreate();
        updateTouchedObjSelection();
        updateLabels();
    });
    QObject::connect(&Segmentation::singleton(), &Segmentation::changedRow, [this](int index){
        objectModel.changeRow(index);
        touchedObjectModel.recreate();
        updateLabels();//maybe subobject count changed
    });
    QObject::connect(&Segmentation::singleton(), &Segmentation::changedRowSelection, [this](int index){
        if (scope s{objectSelectionProtection}) {
            const auto & proxyIndex = objectProxyModelComment.mapFromSource(objectProxyModelCategory.mapFromSource(objectModel.index(index, 0)));
            //selection lookup is way cheaper than reselection (sadly)
            const bool alreadySelected = objectsTable.selectionModel()->isSelected(proxyIndex);
            if (Segmentation::singleton().objects[index].selected && !alreadySelected) {
                objectsTable.selectionModel()->setCurrentIndex(proxyIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);
            } else if (!Segmentation::singleton().objects[index].selected && alreadySelected) {
                objectsTable.selectionModel()->setCurrentIndex(proxyIndex, QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
            }
            touchedObjectModel.recreate();
            updateTouchedObjSelection();
        }
    });
    QObject::connect(&Segmentation::singleton(), &Segmentation::resetData, [this](){
        touchedObjsTable.clearSelection();
        touchedObjectModel.recreate();
        objectsTable.clearSelection();
        objectModel.recreate();
        updateSelection();
        updateTouchedObjSelection();
        updateLabels();
    });
    QObject::connect(&Segmentation::singleton(), &Segmentation::resetTouchedObjects, [this]() {
        touchedObjectModel.recreate();
        touchedLayoutWidget.setHidden(touchedObjectModel.objectCache.size() <= 1);
        touchedObjectsLabel.setText(tr("<strong>Objects containing subobject %1</strong>").arg(Segmentation::singleton().touched_subobject_id));
    });
    QObject::connect(&Segmentation::singleton(), &Segmentation::resetTouchedObjects, this, &SegmentationView::updateTouchedObjSelection);
    QObject::connect(&Segmentation::singleton(), &Segmentation::resetSelection, this, &SegmentationView::updateSelection);
    QObject::connect(&Segmentation::singleton(), &Segmentation::resetSelection, this, &SegmentationView::updateTouchedObjSelection);
    QObject::connect(&Segmentation::singleton(), &Segmentation::renderOnlySelectedObjsChanged, &showOnlySelectedChck, &QCheckBox::setChecked);
    QObject::connect(&Segmentation::singleton(), &Segmentation::categoriesChanged, &categoryModel, &CategoryModel::recreate);
    QObject::connect(&Segmentation::singleton(), &Segmentation::hoveredSubObjectChanged, [this](const uint64_t subobject_id, const std::vector<uint64_t> & overlapObjIndices) {
        auto text = tr("Hovered raw segmentation ID: %1").arg(subobject_id);
        if (overlapObjIndices.empty() == false) {
             text += " (part of: ";
            for (const uint64_t index : overlapObjIndices) {
                text += QString::number(Segmentation::singleton().objects[index].id) + ", ";
            }
            text.chop(2);
            text += ")";
        }
        subobjectHoveredLabel.setWordWrap(true);
        subobjectHoveredLabel.setText(text);
    });

    static auto setColor = [this](QTreeView & table, const QModelIndex & index) {
        if (index.column() == 0) {
            colorDialog.setCurrentColor(table.model()->data(index, Qt::BackgroundRole).value<QColor>());
            state->viewerState->renderInterval = SLOW;
            if (colorDialog.exec() == QColorDialog::Accepted) {
                auto & obj = (&table == &objectsTable) ? Segmentation::singleton().objects[index.row()] : touchedObjectModel.objectCache[index.row()].get();
                auto color = colorDialog.currentColor();
                Segmentation::singleton().changeColor(obj, std::make_tuple(color.red(), color.green(), color.blue()));
            }
            state->viewerState->renderInterval = FAST;
        }
    };

    QObject::connect(&touchedObjsTable, &QTreeView::doubleClicked, [this](const QModelIndex & index) {
        setColor(touchedObjsTable, index);
    });
    QObject::connect(&objectsTable, &QTreeView::doubleClicked, [this](const QModelIndex & index) {
        setColor(objectsTable, index);
    });

    QObject::connect(touchedObjsTable.header(), &QHeaderView::sortIndicatorChanged, threeWaySorting(touchedObjsTable, touchedObjSortSectionIndex));
    QObject::connect(objectsTable.header(), &QHeaderView::sortIndicatorChanged, threeWaySorting(objectsTable, objSortSectionIndex));
    QObject::connect(&objectsTable, &QTreeView::doubleClicked, [this](const QModelIndex index){
        if (index.column() == 1) {//only on id cell
            Segmentation::singleton().jumpToObject(indexFromRow(objectModel, index));
        }
    });
    QObject::connect(&touchedObjsTable, &QTreeView::doubleClicked, [this](const QModelIndex index){
        if (index.column() == 1) {//only on id cell
            Segmentation::singleton().jumpToObject(indexFromRow(touchedObjectModel, index));
        }
    });
    static auto createContextMenu = [](QMenu & contextMenu, QTreeView & table){
        QAction * jumpAction{nullptr}, * deleteAction{nullptr};
        QObject::connect(jumpAction = contextMenu.addAction("Jump to object"), &QAction::triggered, &Segmentation::singleton(), &Segmentation::jumpToSelectedObject);
        QObject::connect(contextMenu.addAction("Merge"), &QAction::triggered, &Segmentation::singleton(), &Segmentation::mergeSelectedObjects);
        QObject::connect(contextMenu.addAction("Restore default color"), &QAction::triggered, &Segmentation::singleton(), &Segmentation::restoreDefaultColorForSelectedObjects);
        QObject::connect(deleteAction = contextMenu.addAction(QIcon(":/resources/icons/menubar/trash.png"), "Delete"), &QAction::triggered, &Segmentation::singleton(), &Segmentation::deleteSelectedObjects);
        deleteAction->setShortcut(Qt::Key_Delete);
        deleteAction->setShortcutContext(Qt::WidgetShortcut);// local to the table
        table.addAction(deleteAction);
        contextMenu.setDefaultAction(jumpAction);
    };
    createContextMenu(objectsContextMenu, objectsTable);
    {
        objectsContextMenu.addSeparator();
        auto & newAction = *objectsContextMenu.addAction("Create new object");
        QObject::connect(&newAction, &QAction::triggered, []() { Segmentation::singleton().createAndSelectObject(state->viewerState->currentPosition); });
    }
    createContextMenu(touchedObjsContextMenu, touchedObjsTable);
    static auto showContextMenu = [](auto & contextMenu, const QTreeView & table, const QPoint & pos){
        contextMenu.actions()[0]->setEnabled(Segmentation::singleton().selectedObjectsCount() == 1);// jumpAction
        contextMenu.actions()[1]->setEnabled(Segmentation::singleton().selectedObjectsCount() > 1);// mergeAction
        contextMenu.actions()[2]->setEnabled(Segmentation::singleton().selectedObjectsCount() > 0);// restoreColorAction
        contextMenu.actions()[3]->setEnabled(Segmentation::singleton().selectedObjectsCount() > 0);// deleteAction
        contextMenu.exec(table.viewport()->mapToGlobal(pos));
        contextMenu.actions()[3]->setEnabled(true);// make deleteAction always available after ctx menu is closed
    };
    QObject::connect(&objectsTable, &QTreeView::customContextMenuRequested, [this](const QPoint & pos){
        showContextMenu(objectsContextMenu, objectsTable, pos);
    });
    QObject::connect(&touchedObjsTable, &QTreeView::customContextMenuRequested, [this](const QPoint & pos){
        showContextMenu(touchedObjsContextMenu, touchedObjsTable, pos);
    });
    QObject::connect(objectsTable.selectionModel(), &QItemSelectionModel::selectionChanged, this, &SegmentationView::selectionChanged);
    QObject::connect(touchedObjsTable.selectionModel(), &QItemSelectionModel::selectionChanged, this, &SegmentationView::touchedObjSelectionChanged);
    QObject::connect(&showOnlySelectedChck, &QCheckBox::clicked, &Segmentation::singleton(), &Segmentation::setRenderOnlySelectedObjs);
    //rutuja
    QObject::connect(&objectCreateButton, &QPushButton::clicked, [](){Segmentation::singleton().createandselect=true;});

    touchedObjectModel.recreate();
    objectModel.recreate();
    touchedObjectModel.recreate();
    objectModel.recreate();

    updateLabels();
}

template<typename Func>
void commitSelection(const QItemSelection & selected, const QItemSelection & deselected, Func proxy) {

    //rutuja - to disable deselecting of objects from the viewer that have already been selected.
    //Adding the ability to selectively
    auto & seg = Segmentation::singleton();
    Segmentation::singleton().activeIndices.clear();

    if(selected.indexes().size() != 0)
    {

        for (const auto & index : selected.indexes()) {

            if (index.column() == 2) {

                Segmentation::singleton().activeIndices.emplace_back(proxy(index.row()));
            }
       }
    } else {

        size_t size = seg.selectedObjectIndices.size();

        std::vector<uint64_t> temp(size);
        std::vector<uint64_t>::iterator it;
        for(uint64_t  o = 0;o < size; o++)
        {
          temp.at(o) = o;
        }

        for (const auto & index : deselected.indexes()) {

           it = std::find(temp.begin(),temp.end(),proxy(index.row()));
           if(it != temp.end())
           {
               temp.erase(it);
           }

        }
        seg.activeIndices.emplace_back(temp.front());
    }

    //old code - commented by rutuja
    /*for (const auto & index : deselected.indexes()) {
        if (index.column() == 1) {//only evaluate id cell
            Segmentation::singleton().unselectObject(proxy(index.row()));
        }
    }
    for (const auto & index : selected.indexes()) {
        if (index.column() == 1) {//only evaluate id cell
            Segmentation::singleton().selectObject(proxy(index.row()));
        }
    }*/
}

void commitSelection(const QItemSelection & selected, const QItemSelection & deselected) {
    commitSelection(selected, deselected, [](const int & i){return i;});
}

void SegmentationView::touchedObjSelectionChanged(const QItemSelection & selected, const QItemSelection & deselected) {
    if (touchedObjectSelectionProtection) {
        return;
    }
    if (!QApplication::keyboardModifiers().testFlag(Qt::ControlModifier)) {
        //unselect all previously selected objects
        commitSelection(QItemSelection{}, objectsTable.selectionModel()->selection());
    }
    commitSelection(selected, deselected, [this](const int & i){
        return touchedObjectModel.objectCache[i].get().index;
    });
    updateSelection();
}

void SegmentationView::selectionChanged(const QItemSelection & selected, const QItemSelection & deselected) {
    if (scope s{objectSelectionProtection}) {
        const auto & proxySelected = objectProxyModelCategory.mapSelectionToSource(objectProxyModelComment.mapSelectionToSource(selected));
        const auto & proxyDeselected = objectProxyModelCategory.mapSelectionToSource(objectProxyModelComment.mapSelectionToSource(deselected));
        commitSelection(proxySelected, proxyDeselected);
        updateTouchedObjSelection();
    }
}

void SegmentationView::updateTouchedObjSelection() {
    const auto & selectedItems = blockSelection(touchedObjectModel, touchedObjectModel.objectCache);

    touchedObjectSelectionProtection = true;//using block signals prevents update of the tableview
    touchedObjsTable.selectionModel()->select(selectedItems, QItemSelectionModel::ClearAndSelect);
    touchedObjectSelectionProtection = false;
}

void SegmentationView::updateSelection() {
    const auto & selectedItems = blockSelection(objectModel, Segmentation::singleton().objects);
    const auto & proxySelection = objectProxyModelComment.mapSelectionFromSource(objectProxyModelCategory.mapSelectionFromSource(selectedItems));

    objectSelectionProtection = true;//using block signals prevents update of the tableview
    objectsTable.selectionModel()->select(proxySelection, QItemSelectionModel::ClearAndSelect);
    objectSelectionProtection = false;

    if (!selectedItems.indexes().isEmpty()) {// scroll to first selected entry
        objectsTable.scrollTo(proxySelection.indexes().front());
    }
}

void SegmentationView::filter() {
    objectProxyModelCategory.setFilterFixedString(categoryFilter.currentText());
    if (regExCheckbox.isChecked()) {
        objectProxyModelComment.setFilterRegExp(commentFilter.text());
    } else {
        objectProxyModelComment.setFilterFixedString(commentFilter.text());
    }
    updateSelection();
    updateTouchedObjSelection();
}

void SegmentationView::updateLabels() {
    objectCountLabel.setText(QString("Objects: %1").arg(Segmentation::singleton().objects.size()));
    subobjectCountLabel.setText(QString("Subobjects: %1").arg(Segmentation::singleton().subobjects.size()));
}

uint64_t SegmentationView::indexFromRow(const SegmentationObjectModel &, const QModelIndex index) const {
    return objectProxyModelCategory.mapSelectionToSource(objectProxyModelComment.mapSelectionToSource({index, index})).indexes().front().row();
}
uint64_t SegmentationView::indexFromRow(const TouchedObjectModel & model, const QModelIndex index) const {
    return model.objectCache[index.row()].get().index;
}
