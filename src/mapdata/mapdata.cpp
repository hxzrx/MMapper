// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "mapdata.h"

#include <algorithm>
#include <cassert>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <QList>
#include <QString>

#include "../expandoracommon/RoomRecipient.h"
#include "../expandoracommon/coordinate.h"
#include "../expandoracommon/exit.h"
#include "../expandoracommon/room.h"
#include "../global/roomid.h"
#include "../global/utils.h"
#include "../mapfrontend/map.h"
#include "../mapfrontend/mapaction.h"
#include "../mapfrontend/mapfrontend.h"
#include "../parser/CommandId.h"
#include "ExitDirection.h"
#include "ExitFieldVariant.h"
#include "customaction.h"
#include "drawstream.h"
#include "infomark.h"
#include "mmapper2room.h"
#include "roomfilter.h"
#include "roomselection.h"

MapData::MapData(QObject *const parent)
    : MapFrontend(parent)
{}

const DoorName &MapData::getDoorName(const Coordinate &pos, const ExitDirEnum dir)
{
    // REVISIT: Could this function could be made const if we make mapLock mutable?
    // Alternately, WTF are we accessing this from multiple threads?
    QMutexLocker locker(&mapLock);
    if (const Room *const room = map.get(pos)) {
        if (dir < ExitDirEnum::UNKNOWN) {
            return room->exit(dir).getDoorName();
        }
    }

    static const DoorName tmp{"exit"};
    return tmp;
}

void MapData::setDoorName(const Coordinate &pos, DoorName moved_doorName, const ExitDirEnum dir)
{
    QMutexLocker locker(&mapLock);
    if (Room *const room = map.get(pos)) {
        if (dir < ExitDirEnum::UNKNOWN) {
            scheduleAction(std::make_unique<SingleRoomAction>(
                std::make_unique<UpdateExitField>(std::move(moved_doorName), dir), room->getId()));
        }
    }
}

ExitDirections MapData::getExitDirections(const Coordinate &pos)
{
    ExitDirections result;
    QMutexLocker locker(&mapLock);
    if (const Room *const room = map.get(pos)) {
        for (auto dir : ALL_EXITS7) {
            if (room->exit(dir).isExit())
                result |= dir;
        }
    }
    return result;
}

bool MapData::getExitFlag(const Coordinate &pos, const ExitDirEnum dir, ExitFieldVariant var)
{
    assert(var.getType() != ExitFieldEnum::DOOR_NAME);

    QMutexLocker locker(&mapLock);
    if (const Room *const room = map.get(pos)) {
        if (dir < ExitDirEnum::NONE) {
            switch (var.getType()) {
            case ExitFieldEnum::DOOR_NAME: {
                const auto name = room->exit(dir).getDoorName();
                if (var.getDoorName() == name) {
                    return true;
                }
                break;
            }
            case ExitFieldEnum::EXIT_FLAGS: {
                const auto ef = room->exit(dir).getExitFlags();
                if (IS_SET(ef, var.getExitFlags())) {
                    return true;
                }
                break;
            }
            case ExitFieldEnum::DOOR_FLAGS: {
                const auto df = room->exit(dir).getDoorFlags();
                if (IS_SET(df, var.getDoorFlags())) {
                    return true;
                }
                break;
            }
            }
        }
    }
    return false;
}

void MapData::toggleExitFlag(const Coordinate &pos, const ExitDirEnum dir, ExitFieldVariant var)
{
    const auto field = var.getType();
    assert(field != ExitFieldEnum::DOOR_NAME);

    QMutexLocker locker(&mapLock);
    if (Room *const room = map.get(pos)) {
        if (dir < ExitDirEnum::NONE) {
            scheduleAction(std::make_shared<SingleRoomAction>(
                std::make_unique<ModifyExitFlags>(var, dir, FlagModifyModeEnum::TOGGLE),
                room->getId()));
        }
    }
}

void MapData::toggleRoomFlag(const Coordinate &pos, RoomFieldVariant var)
{
    QMutexLocker locker(&mapLock);
    if (Room *room = map.get(pos)) {
        // REVISIT: Consolidate ModifyRoomFlags and UpdateRoomField
        auto action = (var.getType() == RoomFieldEnum::MOB_FLAGS
                       || var.getType() == RoomFieldEnum::LOAD_FLAGS)
                          ? std::make_shared<SingleRoomAction>(
                                std::make_unique<ModifyRoomFlags>(var, FlagModifyModeEnum::TOGGLE),
                                room->getId())
                          : std::make_shared<SingleRoomAction>(std::make_unique<UpdateRoomField>(
                                                                   var),
                                                               room->getId());
        scheduleAction(action);
    }
}

const Room *MapData::getRoom(const Coordinate &pos)
{
    QMutexLocker locker(&mapLock);
    return map.get(pos);
}

bool MapData::getRoomFlag(const Coordinate &pos, RoomFieldVariant var)
{
    // REVISIT: Use macros
    QMutexLocker locker(&mapLock);
    if (Room *const room = map.get(pos)) {
        switch (var.getType()) {
        case RoomFieldEnum::NOTE: {
            return var.getNote() == room->getNote();
        }
        case RoomFieldEnum::MOB_FLAGS: {
            const auto ef = room->getMobFlags();
            if (IS_SET(ef, var.getMobFlags())) {
                return true;
            }
            break;
        }
        case RoomFieldEnum::LOAD_FLAGS: {
            const auto ef = room->getLoadFlags();
            if (IS_SET(ef, var.getLoadFlags())) {
                return true;
            }
            break;
        }
        case RoomFieldEnum::ALIGN_TYPE:
            return var.getAlignType() == room->getAlignType();
        case RoomFieldEnum::LIGHT_TYPE:
            return var.getLightType() == room->getLightType();
        case RoomFieldEnum::PORTABLE_TYPE:
            return var.getPortableType() == room->getPortableType();
        case RoomFieldEnum::RIDABLE_TYPE:
            return var.getRidableType() == room->getRidableType();
        case RoomFieldEnum::SUNDEATH_TYPE:
            return var.getSundeathType() == room->getSundeathType();
        case RoomFieldEnum::TERRAIN_TYPE:
            return var.getTerrainType() == room->getTerrainType();
        case RoomFieldEnum::NAME:
        case RoomFieldEnum::DESC:
        case RoomFieldEnum::DYNAMIC_DESC:
        case RoomFieldEnum::LAST:
        case RoomFieldEnum::RESERVED:
            throw std::runtime_error("impossible");
        }
    }
    return false;
}

QList<Coordinate> MapData::getPath(const Coordinate &start, const CommandQueue &dirs)
{
    QMutexLocker locker(&mapLock);
    QList<Coordinate> ret;

    //* NOTE: room is used and then reassigned inside the loop.
    if (const Room *room = map.get(start)) {
        for (const auto cmd : dirs) {
            if (cmd == CommandEnum::LOOK)
                continue;

            if (!isDirectionNESWUD(cmd)) {
                break;
            }

            const Exit &e = room->exit(getDirection(cmd));
            if (!e.isExit()) {
                // REVISIT: why does this continue but all of the others break?
                continue;
            }

            if (!e.outIsUnique()) {
                break;
            }

            const SharedConstRoom &tmp = roomIndex[e.outFirst()];
            if (room == nullptr) {
                break;
            }

            // WARNING: room is reassigned here!
            room = tmp.get();
            ret.append(room->getPosition());
        }
    }
    return ret;
}

// the room will be inserted in the given selection. the selection must have been created by mapdata
const Room *MapData::getRoom(const Coordinate &pos, RoomSelection &selection)
{
    QMutexLocker locker(&mapLock);
    if (Room *const room = map.get(pos)) {
        auto id = room->getId();
        lockRoom(&selection, id);
        selection.insert(id, room);
        return room;
    }
    return nullptr;
}

const Room *MapData::getRoom(const RoomId id, RoomSelection &selection)
{
    QMutexLocker locker(&mapLock);
    if (const SharedRoom &room = roomIndex[id]) {
        const RoomId roomId = room->getId();
        assert(id == roomId);

        lockRoom(&selection, roomId);
        Room *const pRoom = room.get();
        selection.insert(roomId, pRoom);
        return pRoom;
    }
    return nullptr;
}

void MapData::draw(const Coordinate &min, const Coordinate &max, MapCanvasRoomDrawer &screen)
{
    QMutexLocker locker(&mapLock);
    DrawStream drawer(screen, roomIndex, locks);
    map.getRooms(drawer, min, max);
    drawer.draw();
}

bool MapData::execute(std::unique_ptr<MapAction> action, const SharedRoomSelection &selection)
{
    QMutexLocker locker(&mapLock);
    action->schedule(this);
    std::list<RoomId> selectedIds;

    for (auto i = selection->begin(); i != selection->end();) {
        const Room *room = *i++;
        const auto id = room->getId();
        locks[id].erase(selection.get());
        selectedIds.push_back(id);
    }
    selection->clear();

    MapAction *const pAction = action.get();
    const bool executable = isExecutable(pAction);
    if (executable) {
        executeAction(pAction);
    } else {
        qWarning() << "Unable to execute action" << pAction;
    }

    for (auto id : selectedIds) {
        if (const SharedRoom &room = roomIndex[id]) {
            locks[id].insert(selection.get());
            selection->insert(id, room.get());
        }
    }
    return executable;
}

void MapData::clear()
{
    MapFrontend::clear();
    m_markers.clear();
    emit log("MapData", "cleared MapData");
}

void MapData::removeDoorNames()
{
    QMutexLocker locker(&mapLock);

    const auto noName = DoorName{};
    for (auto &room : roomIndex) {
        if (room != nullptr) {
            for (const auto dir : ALL_EXITS_NESWUD) {
                scheduleAction(
                    std::make_unique<SingleRoomAction>(std::make_unique<UpdateExitField>(noName,
                                                                                         dir),
                                                       room->getId()));
            }
        }
    }
}

void MapData::genericSearch(RoomRecipient *recipient, const RoomFilter &f)
{
    QMutexLocker locker(&mapLock);
    for (const SharedRoom &room : roomIndex) {
        if (room == nullptr)
            continue;
        Room *const r = room.get();
        if (!f.filter(r))
            continue;
        locks[room->getId()].insert(recipient);
        recipient->receiveRoom(this, r);
    }
}

MapData::~MapData() = default;

void MapData::removeMarker(const std::shared_ptr<InfoMark> &im)
{
    if (im != nullptr) {
        auto it = std::find_if(m_markers.begin(), m_markers.end(), [&im](const auto &target) {
            return target == im;
        });
        if (it != m_markers.end()) {
            m_markers.erase(it);
        }
    }
}

void MapData::removeMarkers(const MarkerList &toRemove)
{
    // If toRemove is short, this is probably "good enough." However, it may become
    // very painful if both toRemove.size() and m_markers.size() are in the thousands.
    for (const auto &im : toRemove) {
        removeMarker(im);
    }
}

void MapData::addMarker(const std::shared_ptr<InfoMark> &im)
{
    if (im != nullptr)
        m_markers.emplace_back(im);
}
