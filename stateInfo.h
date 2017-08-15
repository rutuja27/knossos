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

#ifndef STATE_INFO_H
#define STATE_INFO_H

#include "coordinate.h"
#include "hashtable.h"

#include <QElapsedTimer>
#include <QMutex>
#include <QString>
#include <QWaitCondition>

class stateInfo;
extern stateInfo * state;

//used to be MAX_PATH but conflicted with the official one
#define CSTRING_SIZE 250

#define NUM_MAG_DATASETS 65536

// Bytes for an object ID.
#define OBJID_BYTES sizeof(uint64_t)

// UserMove type
enum UserMoveType {USERMOVE_DRILL, USERMOVE_HORIZONTAL, USERMOVE_NEUTRAL};
Q_DECLARE_METATYPE(UserMoveType)

//custom tail recursive constexpr log implementation
//required for the following array declarations/accesses: (because std::log2 isn’t required to be constexpr yet)
//  magPaths, magNames, magBoundaries, Dc2Pointer, Oc2Pointer
//TODO to be removed when the above mentioned variables vanish
//integral return value for castless use in subscripts
template<typename T>
constexpr std::size_t int_log(const T val, const T base = 2, const std::size_t res = 0) {
    return val < base ? res : int_log(val/base, base, res+1);
}

/**
  * @stateInfo
  * @brief stateInfo holds everything we need to know about the current instance of Knossos
  *
  * It gets instantiated in the main method of knossos.cpp and referenced in almost all important files and classes below the #includes with extern  stateInfo
  */
class stateInfo {
public:
    //  Info about the data
    bool gpuSlicer = false;
    // How user movement was generated
    UserMoveType loaderUserMoveType{USERMOVE_NEUTRAL};
    // Direction of user movement in case of drilling,
    // or normal to viewport plane in case of horizontal movement.
    // Left unset in neutral movement.
    Coordinate loaderUserMoveViewportDirection{};

    bool quitSignal{false};

    // Current dataset identifier string
    QString name;

    // stores the currently active magnification;
    // it is set by magnification = 2^MAGx
    // state->magnification should only be used by the viewer,
    // but its value is copied over to loaderMagnification.
    // This is locked for thread safety.
    // do not change to uint, it causes bugs in the display of higher mag datasets
    int magnification;

    uint compressionRatio;

    uint highestAvailableMag;
    uint lowestAvailableMag;

    // Bytes in one datacube: 2^3N
    std::size_t cubeBytes;

    // The edge length of a datacube is 2^N, which makes the size of a
    // datacube in bytes 2^3N which has to be <= 2^32 - 1 (unsigned int).
    // So N cannot be larger than 10.
    // Edge length of one cube in pixels: 2^N
    int cubeEdgeLength;

    // Area of a cube slice in pixels;
    int cubeSliceArea;

    // Supercube edge length in datacubes.
    int M;
    std::size_t cubeSetElements;


    // Bytes in one supercube (This is pretty much the memory
    // footprint of KNOSSOS): M^3 * 2^3M
    std::size_t cubeSetBytes;


    // Edge length of the current data set in data pixels.
    Coordinate boundary;

    // pixel-to-nanometer scale
    floatCoordinate scale;

    //rutuja -hdf5 name
    bool hdf5_found = false;
    bool raw_found = false;
    bool seg_found = false;
    bool warn_once = false;
    bool seg_lvl_changed = false;
    std::string hdf5 = "";
    std::string raw_static_label;
    std::string segmentation_static_label;
    int segmentation_level=0;
    int mode = 0;

    QString baseUrl;

    //rutuja
    Coordinate superChunkSize= {8,8,4};
    Coordinate cube_offset;

    // With 2^N being the edge length of a datacube in pixels and
    // M being the edge length of a supercube (the set of all
    // simultaneously loaded datacubes) in datacubes:

// --- Inter-thread communication structures / signals / mutexes, etc. ---

    // ANY access to the Dc2Pointer or Oc2Pointer tables has
    // to be locked by this mutex.

    QMutex protectCube2Pointer;

 //---  Info about the state of KNOSSOS in general. --------

    // Dc2Pointer and Oc2Pointer provide a mappings from cube
    // coordinates to pointers to datacubes / overlay cubes loaded
    // into memory.
    // It is a set of key (cube coordinate) / value (pointer) pairs.
    // Whenever we access a datacube in memory, we do so through
    // this structure.
    coord2bytep_map_t Dc2Pointer[int_log(NUM_MAG_DATASETS)+1];
    coord2bytep_map_t Oc2Pointer[int_log(NUM_MAG_DATASETS)+1];

    struct ViewerState * viewerState;
    class MainWindow * mainWindow{nullptr};
    class Viewer * viewer;
    class Scripting * scripting;
    class SignalRelay * signalRelay;
    struct SkeletonState * skeletonState;
};

#endif//STATE_INFO_H
