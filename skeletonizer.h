/*
 *  This file is a part of KNOSSOS.
 *
 *  (C) Copyright 2007-2012
 *  Max-Planck-Gesellschaft zur Förderung der Wissenschaften e.V.
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

static struct segmentListElement *addSegmentListElement(struct segmentListElement **firstSegment, struct nodeListElement *sourceNode, struct nodeListElement *targetNode, struct stateInfo *state);
static struct treeListElement *findTreeByTreeID(int32_t treeID, struct stateInfo *state);
static uint32_t addNodeToSkeletonStruct(struct nodeListElement *node, struct stateInfo *state);
static uint32_t addSegmentToSkeletonStruct(struct segmentListElement *segment, struct stateInfo *state);
static uint32_t delNodeFromSkeletonStruct(struct nodeListElement *node, struct stateInfo *state);
static uint32_t delSegmentFromSkeletonStruct(struct segmentListElement *segment, struct stateInfo *state);
static void WRAP_popBranchNode();
static void popBranchNodeCanceled();
