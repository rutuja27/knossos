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

#include "segmentation.h"
#include <iostream>
#include "file_io.h"
#include "loader.h"
#include "session.h"
#include "skeleton/skeletonizer.h"
#include "viewer.h"
#include "widgets/mainWindow.h"
#include <QTextStream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <utility>

bool Segmentation::enabled = false;
uint64_t Segmentation::SubObject::highestId = 0;
uint64_t Segmentation::Object::highestId = 0;
uint64_t Segmentation::Object::highestIndex = -1;

Segmentation::Object::Object(std::vector<std::reference_wrapper<SubObject>> initialVolumes, const Coordinate & location, const uint64_t id, const bool & todo, const bool & immutable)
    : id(id), todo(todo), immutable(immutable), location(location) {
    highestId = std::max(id, highestId);
    for (auto & elem : initialVolumes) {
        addExistingSubObject(elem);
    }
    std::sort(std::begin(subobjects), std::end(subobjects));
}

Segmentation::Object::Object(Object & first, Object & second)
    : id(++highestId), todo(false), immutable(false), location(second.location), selected{true} { //merge is selected
    subobjects.reserve(first.subobjects.size() + second.subobjects.size());
    merge(first);
    merge(second);
    subobjects.shrink_to_fit();
}

bool Segmentation::Object::operator==(const Segmentation::Object & other) const {
    return index == other.index;
}

void Segmentation::Object::addExistingSubObject(Segmentation::SubObject & sub) {
    const auto objectPosIt = std::lower_bound(std::begin(sub.objects), std::end(sub.objects), this->index);
    if (objectPosIt != std::end(sub.objects) && *objectPosIt == this->index) {
        throw std::runtime_error(tr("object %1 already contains subobject %2").arg(this->id).arg(sub.id).toStdString());
    }
    sub.objects.emplace(objectPosIt, this->index);//register parent
    subobjects.emplace_back(sub);//add child
}

Segmentation::Object & Segmentation::Object::merge(Segmentation::Object & other) {
    for (auto & elem : other.subobjects) {//add parent
        auto & parentObjs = elem.get().objects;
        elem.get().selectedObjectsCount = 1;
        //don’t insert twice
        auto posIt = std::lower_bound(std::begin(parentObjs), std::end(parentObjs), this->index);
        if (posIt == std::end(parentObjs) || *posIt != this->index) {
            parentObjs.emplace(posIt, this->index);
        }
    }
    decltype(subobjects) tmp;
    tmp.reserve(subobjects.size() + other.subobjects.size());
    std::merge(std::begin(subobjects), std::end(subobjects), std::begin(other.subobjects), std::end(other.subobjects), std::back_inserter(tmp));
    tmp.shrink_to_fit();
    std::swap(subobjects, tmp);
    subobjects.erase(std::unique(std::begin(subobjects), std::end(subobjects)), std::end(subobjects));
    return *this;
}

Segmentation & Segmentation::singleton() {
    static Segmentation segmentation;
    return segmentation;
}

Segmentation::Segmentation() {
    loadOverlayLutFromFile();
}

bool Segmentation::hasSegData() const {
    return hasObjects() || (Loader::Controller::singleton().worker != nullptr && !Loader::Controller::singleton().worker->snappyCache.empty());//we will change smth
}

void Segmentation::clear() {
    Session::singleton().unsavedChanges |= hasSegData();

    selectedObjectIndices.clear();
    objects.clear();
    objectIdToIndex.clear();
    Object::highestId = 0;
    Object::highestIndex = -1;
    SubObject::highestId = 0;
    subobjects.clear();
    touched_subobject_id = 0;
    categories = prefixed_categories;

    if (Loader::Controller::singleton().worker != nullptr) {
        //dispatch to loader thread, original cubes are reloaded automatically
        QTimer::singleShot(0, Loader::Controller::singleton().worker.get(), &Loader::Worker::snappyCacheClear);
    }

    emit resetData();
    emit resetSelection();
    emit resetTouchedObjects();
}

std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> Segmentation::subobjectColor(const uint64_t subObjectID) const {
    const auto colorIndex = subObjectID % overlayColorMap.size();
    return std::tuple_cat(overlayColorMap[colorIndex], std::make_tuple(alpha));
}

void Segmentation::loadOverlayLutFromFile(const QString & path) {
    overlayColorMap = loadLookupTable(path);
    emit resetData();
}

bool Segmentation::hasObjects() const {
    return !this->objects.empty();
}

void Segmentation::createAndSelectObject(const Coordinate & position) {
    clearObjectSelection();
    auto & newObject = createObjectFromSubobjectId(SubObject::highestId + 1, position);
    selectObject(newObject);
}

Segmentation::Object & Segmentation::createObjectFromSubobjectId(const uint64_t initialSubobjectId, const Coordinate & location, const uint64_t objectId, const bool todo, const bool immutable) {
    if (objectIdToIndex.find(objectId) != std::end(objectIdToIndex)) {
        throw std::runtime_error(tr("object with id %1 already exists").arg(objectId).toStdString());
    }
    //first is iterator to the newly inserted key-value pair or the already existing value
    auto subobjectIt = subobjects.emplace(std::piecewise_construct, std::forward_as_tuple(initialSubobjectId), std::forward_as_tuple(initialSubobjectId)).first;
    return createObject(std::vector<std::reference_wrapper<SubObject>>{subobjectIt->second}, location, objectId, todo, immutable);
}

template<typename... Args>
Segmentation::Object & Segmentation::createObject(Args &&... args) {
    emit beforeAppendRow();
    objects.emplace_back(std::forward<Args>(args)...);
    objectIdToIndex[objects.back().id] = objects.size() - 1;
    emit appendedRow();
    return objects.back();
}

void Segmentation::removeObject(Object & object) {
    unselectObject(object);
    for (auto & elem : object.subobjects) {
        auto & subobject = elem.get();
        subobject.objects.erase(std::remove(std::begin(subobject.objects), std::end(subobject.objects), object.index), std::end(subobject.objects));
        if (subobject.objects.empty()) {
            subobjects.erase(subobject.id);
        }
    }
    //swap with last, so no intermediate rows need to be deleted
    if (objects.size() > 1 && object.index != objects.back().index) {
        //replace object index in subobjects
        for (auto & elem : objects.back().subobjects) {
            auto & subobject = elem.get();
            std::replace(std::begin(subobject.objects), std::end(subobject.objects), objects.back().index, object.index);
        }
        //replace object index in selected objects
        selectedObjectIndices.replace(objects.back().index, object.index);
        std::swap(objects.back().index, object.index);
        std::swap(objects.back(), object);
        std::swap(objectIdToIndex[objects.back().id], objectIdToIndex[object.id]);
        emit changedRow(object.index);//object now references the former end
        emit changedRowSelection(object.index);//object now references the former end
        emit changedRowSelection(objects.back().index);//object now references the former end
    }
    emit beforeRemoveRow();
    //the last element is the one which gets removed
    if (objects.back().id == Object::highestId) {
        --Object::highestId;//reassign highest object id if it was removed before
    }
    objectIdToIndex.erase(objects.back().id);
    objects.pop_back();
    emit removedRow();
    --Object::highestIndex;
}

void Segmentation::changeCategory(Object & obj, const QString & category) {
    obj.category = category;
    emit changedRow(obj.index);
    categories.insert(category);
    emit categoriesChanged();
}

void Segmentation::changeColor(Object &obj, const std::tuple<uint8_t, uint8_t, uint8_t> & color) {
    obj.color = color;
    emit changedRow(obj.index);
}

void Segmentation::changeComment(Object & obj, const QString & comment) {
    obj.comment = comment;
    emit changedRow(obj.index);
}

void Segmentation::newSubObject(Object & obj, uint64_t subObjID) {
    auto subobjectIt = subobjects.emplace(std::piecewise_construct, std::forward_as_tuple(subObjID), std::forward_as_tuple(subObjID)).first;
    obj.addExistingSubObject(subobjectIt->second);
}

void Segmentation::setRenderOnlySelectedObjs(const bool onlySelected) {
    renderOnlySelectedObjs = onlySelected;
    emit renderOnlySelectedObjsChanged(renderOnlySelectedObjs);
}

decltype(Segmentation::backgroundId) Segmentation::getBackgroundId() const {
    return backgroundId;
}

void Segmentation::setBackgroundId(decltype(backgroundId) newBackgroundId) {
    emit backgroundIdChanged(backgroundId = newBackgroundId);
}

std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> Segmentation::colorObjectFromIndex(const uint64_t objectIndex) const {
    if (objects[objectIndex].color) {
        return std::tuple_cat(objects[objectIndex].color.get(), std::make_tuple(alpha));
    } else {
        const auto & objectId = objects[objectIndex].id;
        const auto colorIndex = objectId % overlayColorMap.size();
        return std::tuple_cat(overlayColorMap[colorIndex], std::make_tuple(alpha));
    }
}

std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> Segmentation::colorOfSelectedObject() const {
    if (selectedObjectsCount() != 0) {
        return colorObjectFromIndex(selectedObjectIndices.front());
    }
    return {};
}

std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> Segmentation::colorOfSelectedObject(const SubObject & subobject) const {
    if (subobject.selectedObjectsCount > 1) {
        return std::make_tuple(std::uint8_t{255}, std::uint8_t{0}, std::uint8_t{0}, alpha);//mark overlapping objects in red
    }
    const auto objectIndex = *std::find_if(std::begin(subobject.objects), std::end(subobject.objects), [this](const uint64_t index){
        return objects[index].selected;
    });
    return colorObjectFromIndex(objectIndex);
}

std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> Segmentation::colorObjectFromSubobjectId(const uint64_t subObjectID) const {
    if (subObjectID == backgroundId) {
        return {};
    }
    const auto subobjectIt = subobjects.find(subObjectID);
    if (subobjectIt == std::end(subobjects)) {
        if (!renderOnlySelectedObjs || selectedObjectIndices.empty()) {
            return subobjectColor(subObjectID);
        } else {
            return {};
        }
    }
    const auto & subobject = subobjectIt->second;
    if (subobject.selectedObjectsCount != 0) {
        return colorOfSelectedObject(subobject);
    } else if (renderOnlySelectedObjs) {
        return {};
    }
    return colorObjectFromIndex(largestObjectContainingSubobject(subobject));
}

bool Segmentation::subobjectExists(const uint64_t & subobjectId) const {
    auto it = subobjects.find(subobjectId);
    return it != std::end(subobjects);
}

uint64_t Segmentation::subobjectIdOfFirstSelectedObject(const Coordinate & newLocation) {
    if (selectedObjectsCount() != 0) {
        auto & obj = objects[selectedObjectIndices.front()];
        obj.location = newLocation;
        return obj.subobjects.front().get().id;
    } else {
        throw std::runtime_error("no objects selected for subobject retrieval");
    }
}

Segmentation::SubObject & Segmentation::subobjectFromId(const uint64_t & subobjectId, const Coordinate & location) {
    auto it = subobjects.find(subobjectId);//check if subobject exists
    if (it == std::end(subobjects)) {
        createObjectFromSubobjectId(subobjectId, location);//create an object for the selected subobject
        it = subobjects.find(subobjectId);
    }
    return it->second;
}

bool Segmentation::objectOrder(const uint64_t & lhsIndex, const uint64_t & rhsIndex) const {
    const auto & lhs = objects[lhsIndex];
    const auto & rhs = objects[rhsIndex];
    //operator< substitute, prefer immutable objects and choose the smallest
    return (lhs.immutable && !rhs.immutable) || (lhs.immutable == rhs.immutable && lhs.subobjects.size() < rhs.subobjects.size());
}

uint64_t Segmentation::largestObjectContainingSubobjectId(const uint64_t subObjectId, const Coordinate & location) {
    const auto & subobject = subobjectFromId(subObjectId, location);
    return largestObjectContainingSubobject(subobject);
}

uint64_t Segmentation::largestObjectContainingSubobject(const Segmentation::SubObject & subobject) const {
    //same comparator for both functions, it seems to work as it is, so i don’t waste my head now to find out why
    //there may have been some reasoning… (at first glance it seems too restrictive for the largest object)
    auto comparator = std::bind(&Segmentation::objectOrder, this, std::placeholders::_1, std::placeholders::_2);
    const auto objectIndex = *std::max_element(std::begin(subobject.objects), std::end(subobject.objects), comparator);
    return objectIndex;
}

uint64_t Segmentation::tryLargestObjectContainingSubobject(const uint64_t subObjectId) const {
    auto it = subobjects.find(subObjectId);
    if(it == std::end(subobjects)) {
        return 0;
    }
    return largestObjectContainingSubobject(it->second);
}

uint64_t Segmentation::smallestImmutableObjectContainingSubobject(const Segmentation::SubObject & subobject) const {
    auto comparitor = std::bind(&Segmentation::objectOrder, this, std::placeholders::_1, std::placeholders::_2);
    const auto objectIndex = *std::min_element(std::begin(subobject.objects), std::end(subobject.objects), comparitor);
    return objectIndex;
}

void Segmentation::hoverSubObject(const uint64_t subobject_id) {
    if (subobject_id != hovered_subobject_id) {
        const auto & iter = Segmentation::singleton().subobjects.find(subobject_id);
        std::vector<uint64_t> overlappingObjIndices;
        if (iter != std::end(Segmentation::singleton().subobjects)) {
            overlappingObjIndices = iter->second.objects;
        }
        emit hoveredSubObjectChanged(hovered_subobject_id = subobject_id, overlappingObjIndices);
    }
}

void Segmentation::touchObjects(const uint64_t subobject_id) {
    touched_subobject_id = subobject_id;
    emit resetTouchedObjects();
}

void Segmentation::untouchObjects() {
    touched_subobject_id = backgroundId;
    emit resetTouchedObjects();
}

std::vector<std::reference_wrapper<Segmentation::Object>> Segmentation::touchedObjects() {
    auto it = subobjects.find(touched_subobject_id);
    std::vector<std::reference_wrapper<Segmentation::Object>> vec;
    if (it != std::end(subobjects)) {
        for (const auto & index : it->second.objects) {
            vec.emplace_back(objects[index]);
        }
    }

    return vec;
}

bool Segmentation::isSelected(const SubObject & rhs) const {
    return rhs.selectedObjectsCount != 0;
}

bool Segmentation::isSelected(const uint64_t & objectIndex) const {
    return objects[objectIndex].selected;
}

bool Segmentation::isSubObjectIdSelected(const uint64_t & subobjectId) const {
    auto it = subobjects.find(subobjectId);
    return it != std::end(subobjects) ? isSelected(it->second) : false;
}

void Segmentation::clearObjectSelection() {
    const auto blockState = blockSignals(true);
    while (!selectedObjectIndices.empty()) {
        unselectObject(selectedObjectIndices.back());
    }
    blockSignals(blockState);
    emit resetSelection();
}

void Segmentation::selectObject(Object & object) {
    if (object.selected) {
        return;
    }
    object.selected = true;
    for (auto & subobj : object.subobjects) {
        ++subobj.get().selectedObjectsCount;
        //rutuja
        ++subobj.get().activeObjectsCount;
    }
    selectedObjectIndices.emplace_back(object.index);
    //rutuja - add the selected subobjects to the active list
    activeIndices.emplace_back(object.index);
    emit changedRowSelection(object.index);
}

void Segmentation::unselectObject(const uint64_t & objectIndex) {
    if (objectIndex < objects.size()) {
        unselectObject(objects[objectIndex]);
    }
}

void Segmentation::unselectObject(Object & object) {
    if (!object.selected) {
        return;
    }
    object.selected = false;
    for (auto & subobj : object.subobjects) {
        --subobj.get().selectedObjectsCount;
        //rutuja
        --subobj.get().activeObjectsCount;
    }
    selectedObjectIndices.erase(object.index);
    //rutuja - delete the selected objects from the list
    activeIndices.erase(object.index);
    emit changedRowSelection(object.index);
}

void Segmentation::jumpToObject(Object & object) {
    state->viewer->setPositionWithRecentering(object.location);
}

void Segmentation::jumpToObject(const uint64_t & objectIndex) {
    if(objectIndex < objects.size()) {
        jumpToObject(objects[objectIndex]);
    }
}

void Segmentation::selectNextTodoObject() {
    if(selectedObjectIndices.empty() == false) {
        lastTodoObject_id = selectedObjectIndices.front();
        auto & obj = objects[lastTodoObject_id];
        obj.todo = false;
        unselectObject(obj);
    }
    auto list = todolist();
    if(todolist().empty() == false) {
        selectObject(list.front());
        jumpToObject(list.front());
    }
    emit todosLeftChanged();
}

void Segmentation::selectPrevTodoObject() {
    if(selectedObjectIndices.empty() == false) {
        auto & obj = objects[selectedObjectIndices.front()];
        unselectObject(obj);
    }
    auto & obj = objects[lastTodoObject_id];
    obj.todo = true;
    selectObject(obj);
    jumpToObject(obj);
    emit todosLeftChanged();
    return;
}

void Segmentation::markSelectedObjectForSplitting(const Coordinate & pos) {
    if(selectedObjectIndices.empty() == false) {
        auto & object = objects[selectedObjectIndices.front()];
        object.comment = "Split request";
        object.location = pos;
        emit changedRow(selectedObjectIndices.front());
        selectNextTodoObject();
    }
}

std::vector<std::reference_wrapper<Segmentation::Object>> Segmentation::todolist() {
    std::vector<std::reference_wrapper<Segmentation::Object>> todolist;
    for (auto & obj : objects) {
        if(obj.todo) {
            todolist.push_back(obj);
        }
    }
    std::sort(todolist.begin(), todolist.end());
    return todolist;
}


void Segmentation::unmergeObject(Segmentation::Object & object, Segmentation::Object & other, const Coordinate & position) {
    decltype(object.subobjects) tmp;
    std::set_difference(std::begin(object.subobjects), std::end(object.subobjects), std::begin(other.subobjects), std::end(other.subobjects), std::back_inserter(tmp));
    if (!tmp.empty()) {//only unmerge if subobjects remain
        if (object.immutable) {
            unselectObject(object);
            createObject(tmp, position);
            selectObject(objects.back());
        } else {
            unselectObject(object);
            for (auto & elem : other.subobjects) {
                auto & parentObjs = elem.get().objects;
                parentObjs.erase(std::remove(std::begin(parentObjs), std::end(parentObjs), object.index), std::end(parentObjs));//remove parent
            }
            std::swap(object.subobjects, tmp);
            selectObject(object);
            emit changedRow(object.index);
        }
    }
}

Segmentation::Object & Segmentation::objectFromSubobject(Segmentation::SubObject & subobject, const Coordinate & position) {
    const auto & other = std::find_if(std::begin(subobject.objects), std::end(subobject.objects)
    , [&](const uint64_t elemId){
        const auto & elem = objects[elemId];
        return elem.subobjects.size() == 1 && elem.subobjects.front().get().id == subobject.id;
    });
    if (other == std::end(subobject.objects)) {
        return createObject(std::vector<std::reference_wrapper<SubObject>>{subobject}, position);
    } else {
        return objects[*other];
    }
}

void Segmentation::selectObjectFromSubObject(Segmentation::SubObject & subobject, const Coordinate & position) {
    selectObject(objectFromSubobject(subobject, position));
}

void Segmentation::selectObjectFromSubObject(const uint64_t soid, const Coordinate & position) {
    selectObject(objectFromSubobject(subobjectFromId(soid, position), position));
}

void Segmentation::selectObject(const uint64_t & objectIndex) {
    if (objectIndex < objects.size()) {
        selectObject(objects[objectIndex]);
    }
}

std::size_t Segmentation::selectedObjectsCount() const {
    return selectedObjectIndices.size();
}

void Segmentation::mergelistSave(QIODevice & file) const {
    QTextStream stream(&file);
    for (const auto & obj : objects) {
        stream << obj.id << ' ' << obj.todo << ' ' << obj.immutable;
        for (const auto & subObj : obj.subobjects) {
            stream << ' ' << subObj.get().id;
            stream << ' ' << superChunkids.at(subObj.get().id).x << ' ' << superChunkids.at(subObj.get().id).y << ' ' << superChunkids.at(subObj.get().id).z;

        }
        stream << '\n';
        stream << obj.location.x << ' ' << obj.location.y << ' ' << obj.location.z << ' ';

        if (obj.color) {
            stream << std::get<0>(obj.color.get()) << ' ' << std::get<1>(obj.color.get()) << ' ' << std::get<2>(obj.color.get()) << '\n';
        } else {
            stream << '\n';
        }
        stream << obj.category << '\n';
        stream << obj.comment << '\n';
    }
    if (stream.status() != QTextStream::Ok) {
        qDebug() << "mergelistSave fail";
    }
}

void Segmentation::mergelistLoad(QIODevice & file) {
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error("mergelistLoad open failed");
    }
    QTextStream stream(&file);
    QString line;
    const auto blockState = blockSignals(true);
    while (!(line = stream.readLine()).isNull()) {
        std::istringstream lineStream(line.toStdString());
        std::istringstream coordColorLineStream(stream.readLine().toStdString());

        uint64_t objId;
        bool todo;
        bool immutable;
        Coordinate location;
        uint r; uint g; uint b;
        uint64_t initialVolume;
        QString category;
        QString comment;
        std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> color;

        bool valid0 = (lineStream >> objId) && (lineStream >> todo) && (lineStream >> immutable) && (lineStream >> initialVolume);
        bool valid1 = (coordColorLineStream >> location.x) && (coordColorLineStream >> location.y) && (coordColorLineStream >> location.z);
        bool customColorValid = (coordColorLineStream >> r) && (coordColorLineStream >> g) && (coordColorLineStream >> b);
        bool valid2 = !(category = stream.readLine()).isNull();
        bool valid3 = !(comment = stream.readLine()).isNull();

        if (valid0 && valid1 && valid2 && valid3) {
            uint64_t subObjId;
            supervoxel info;
            auto & obj = createObjectFromSubobjectId(initialVolume, location, objId, todo, immutable);
            //get superchunkid
            lineStream >> state->viewer->superChunkId.x;
            lineStream >> state->viewer->superChunkId.y;
            lineStream >> state->viewer->superChunkId.z;
            superChunkids.insert(std::make_pair(initialVolume,state->viewer->superChunkId));
            //rutuja- get the initial subobjectid
            info.seed = initialVolume;
            info.objid = obj.id;
            color = colorObjectFromSubobjectId(initialVolume);
            info.color = color;
            info.show = true;
            state->viewer->supervoxel_info.push_back(info);
            while (lineStream >> subObjId) {
                newSubObject(obj, subObjId);
                //get superchunkid
                lineStream >> state->viewer->superChunkId.x;
                lineStream >> state->viewer->superChunkId.y;
                lineStream >> state->viewer->superChunkId.z;
                superChunkids.insert(std::make_pair(subObjId,state->viewer->superChunkId));
                //rutuja - get the rest of the subobjectids
                info.seed = subObjId;
                info.objid = obj.id;
                color = colorObjectFromSubobjectId(subObjId);
                info.color = color;
                info.show = true;
                state->viewer->supervoxel_info.push_back(info);

            }
            selectObject(obj);
            std::sort(std::begin(obj.subobjects), std::end(obj.subobjects));
            changeCategory(obj, category);
            if (customColorValid) {
                changeColor(obj, std::make_tuple(r, g, b));
            }
            obj.comment = comment;
            //rutuja

        } else {
            blockSignals(blockState);
            Segmentation::clear();
            throw std::runtime_error("mergelistLoad parsing failed");
        }
    }
    blockSignals(blockState);
    emit resetData();
}

void Segmentation::jobLoad(QIODevice & file) {
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error("jobLoad open failed");
    }
    QTextStream stream(&file);
    QString job_line = stream.readLine();
    QString campaign_line = stream.readLine();
    QString worker_line = stream.readLine();
    QString submit_line = stream.readLine();
    job.id = job_line.isNull() ? 0 : job_line.toInt();
    job.campaign = campaign_line.isNull() ? "" : campaign_line;
    job.worker = worker_line.isNull() ? "" : worker_line;
    job.submitPath = submit_line.isNull() ? "" : submit_line;
    if (job.id != 0) {
        Session::singleton().annotationMode = AnnotationMode::Mode_MergeSimple;
    }
}

void Segmentation::jobSave(QIODevice &file) const {
    QTextStream stream(&file);
    stream << job.id << '\n';
    stream << job.campaign << '\n';
    stream << job.worker << '\n';
    stream << job.submitPath << '\n';
    if (stream.status() != QTextStream::Ok) {
        qDebug() << "mergelistSave fail";
    }
}

void Segmentation::startJobMode() {
    alpha = 37;
    Segmentation::singleton().selectNextTodoObject();
    setRenderOnlySelectedObjs(true);
}

void Segmentation::deleteSelectedObjects() {
    const auto blockState = blockSignals(true);
    while (!activeIndices.empty()) { // changed from selectedObjectindices to activeIndices-rutuja
        Object Object = objects[activeIndices.back()];
        deleted_id = Object.id;
        removeObject(Object);
        //removeObject(objects[activeIndices.back()]);
    }

    //added this code to add new active object after the previous is deleted - rutuja
    if(!objects.empty())
    {
      auto & obj = objects.back();
      activeIndices.emplace_back(obj.index);
    }
     Segmentation::flag_delete = true;

    blockSignals(blockState);
    emit resetData();
}

void Segmentation::mergeSelectedObjects() {

    //while (selectedObjectIndices.size() > 1) {
      while (activeIndices.size() > 1){
        //auto & firstObj = objects[selectedObjectIndices.front()];//front is the merge origin
        //auto & secondObj = objects[selectedObjectIndices.back()];
          auto & firstObj = objects[activeIndices.front()];//front is the merge origin
          auto & secondObj = objects[activeIndices.back()];
        //objects are no longer selected when they got merged
        auto flat_deselect = [this](Object & object){
            object.selected = false;
            selectedObjectIndices.erase(object.index);
            activeIndices.erase(object.index);
            emit changedRowSelection(object.index);//deselect
        };
        //4 (im)mutability possibilities
        if (secondObj.immutable && firstObj.immutable) {
            flat_deselect(secondObj);
            const auto firstIndex = firstObj.index;
            const auto secondIndex = secondObj.index;

            secondObj.todo = false;//secondObj will get invalidated
            uint64_t newIndex = createObject(secondObj, firstObj).index;//create new object from merge result, invalidates firstObj and secondObj references since vector size changed

            flat_deselect(objects[firstIndex]);//firstObj got invalidated
            selectedObjectIndices.emplace_front(newIndex);//move new index to front, so it gets the new merge origin
            activeIndices.emplace_front(newIndex);
            emit changedRowSelection(firstIndex);
            emit changedRowSelection(secondIndex);
        } else if (secondObj.immutable) {
            flat_deselect(secondObj);
            firstObj.merge(secondObj);
            secondObj.todo = false;
            emit changedRowSelection(secondObj.index);
            emit changedRow(firstObj.index);
        } else if (firstObj.immutable) {
            flat_deselect(firstObj);
            secondObj.merge(firstObj);
            firstObj.todo = false;
            emit changedRowSelection(firstObj.index);
            emit changedRow(secondObj.index);
        } else {//if both are mutable the second object is merged into the first
            flat_deselect(secondObj);
            firstObj.merge(secondObj);
            secondObj.todo = false;
            emit changedRow(firstObj.index);
            removeObject(secondObj);
        }
    }
    emit todosLeftChanged();
}

void Segmentation::unmergeSelectedObjects(const Coordinate & clickPos) {
    if (selectedObjectIndices.size() == 1) {
        deleteSelectedObjects();
    } else while (selectedObjectIndices.size() > 1) {
        auto & objectToUnmerge = objects[selectedObjectIndices.back()];
        unmergeObject(objects[selectedObjectIndices.front()], objectToUnmerge, clickPos);
        unselectObject(objectToUnmerge);
        objectToUnmerge.todo = true;
    }
    emit todosLeftChanged();
}

void Segmentation::jumpToSelectedObject() {
    if (!selectedObjectIndices.empty()) {
        jumpToObject(objects[selectedObjectIndices.front()]);
    }
}

bool Segmentation::placeCommentForSelectedObject(const QString & comment) {
    if(selectedObjectIndices.size() == 1) {
        int index = selectedObjectIndices.front();
        objects[index].comment = comment;
        emit changedRow(index);
        return true;
    }
    return false;
}

void Segmentation::restoreDefaultColorForSelectedObjects() {
    if (!selectedObjectIndices.empty()) {
        for (auto index : selectedObjectIndices) {
            auto & colormap = Segmentation::singleton().overlayColorMap;
            auto & obj = objects[index];
            obj.color = colormap[obj.id % colormap.size()];
        }
        emit resetData();
    }
}

//rutuja -- count the num,ber of current active objects
std::size_t Segmentation::activeObjectsCount() const {
    return activeIndices.size();
}

//rutuja-completely remove an object
void Segmentation::remObject(uint64_t subobjectid, Segmentation::Object & sub)
{
    std::vector<std::reference_wrapper<SubObject>>tmp;
    for (auto & elem : sub.subobjects) {
        auto & subobject = elem.get();
        if(subobject.id == subobjectid){
          //unselectObject(sub);
          decltype(sub.subobjects)j;
          deleted_cell_id = subobjectid;
          //flag_delete_cell = true;
          tmp.push_back(subobject);
          std::set_difference(std::begin(sub.subobjects), std::end(sub.subobjects), std::begin(tmp), std::end(tmp), std::back_inserter(j));
          subobject.objects.erase(std::remove(std::begin(subobject.objects), std::end(subobject.objects), sub.index), std::end(subobject.objects));
          subobjects.erase(subobject.id);
          std::swap(sub.subobjects,j);
          emit changedRow(sub.index);


        }

    }

}

// rutuja - clear only the subobjects in the active selection
void Segmentation::clearActiveSelection(){

   while(!activeIndices.empty())
   {
     activeIndices.clear();
   }
}

//rutuja - delete a single subobject from an object in the 3d mesh
void Segmentation::cell_delete(){
   auto & segment = Segmentation::singleton();
   auto & skeleton = Skeletonizer::singleton();
   //segment.flag_delete_cell = false;
   std::vector<supervoxel>::iterator i = state->viewer->supervoxel_info.begin();
   while(i != state->viewer->supervoxel_info.end()){
        if(i->seed == segment.deleted_cell_id){
          uint64_t objId = i->objid;
          i = state->viewer->supervoxel_info.erase(i);

          break;
        }
        i++;
    }
    skeleton.deleteMeshOfTree(segment.deleted_cell_id);
}

//rutuja- selectively turn on/off branches of meshes in 3D window
void Segmentation::branch_onoff(Segmentation::Object & obj) {
   auto & skeleton = Skeletonizer::singleton();
   int size = state->viewer->supervoxel_info.size();

   std::vector<supervoxel>::iterator it = state->viewer->supervoxel_info.begin();
   int k = 0;
   while(k < size ){
        if(it->objid == obj.id){

           it->show = obj.on_off;
           if(it->show)
           {
              state->viewer->hdf5_read(*it);
           }else {
              skeleton.deleteMeshOfTree(it->seed);
           }

        }

       it++;
       k++;
    }
}

//rutuja- Delete an entire tree from the 3D mesh
void Segmentation::branch_delete(){

   auto & segment = Segmentation::singleton();
   segment.flag_delete = false;
   std::vector<supervoxel>::iterator i = state->viewer->supervoxel_info.begin();
   while(i != state->viewer->supervoxel_info.end()){
        if(i->objid == segment.deleted_id){
           i = state->viewer->supervoxel_info.erase(i);
        }else{
           i++;
        }
   }
}

//rutuja - get active id color
 std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> Segmentation::get_active_color()
{
   return activeid_color;
}

void Segmentation::set_active_color()
{
   activeid_color = {250, 0 ,0 , alpha};
}

//rutuja - function to dynamicaaly switch colors between current
//active and non active supervoxels
void Segmentation::change_colors(uint64_t objid)
{

    int k = state->viewer->supervoxel_info.size();
    std::tuple<uint8_t,uint8_t,uint8_t,uint8_t> color;
    if(k > 1){
       std::vector<supervoxel>::iterator i = state->viewer->supervoxel_info.begin();
       while(i != state->viewer->supervoxel_info.end())
       {
         if(i->objid != objid)
         {
           color = colorObjectFromSubobjectId(i->seed);
           i->color = color;
           state->viewer->hdf5_read(*i);
           i++;
         }
         else
         {
           i->color = get_active_color();
           state->viewer->hdf5_read(*i);
           i++;
         }
       }
    }

}

void Segmentation::setCurrentmergeid(uint64_t subobjid)
{
   currentmergeid = subobjid;
}

uint64_t Segmentation::getCurrentmergeid()
{
   return currentmergeid;
}
