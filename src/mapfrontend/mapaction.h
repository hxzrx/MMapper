#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include <memory>
#include <set>
#include <stack>
#include <vector>
#include <QString>
#include <QVariant>
#include <QtGlobal>

#include "../expandoracommon/parseevent.h"
#include "../global/roomid.h"
#include "../mapdata/ExitDirection.h"
#include "../mapdata/RoomFieldVariant.h"
#include "../mapdata/mmapper2exit.h"
#include "../mapdata/mmapper2room.h"

class Map;
class MapFrontend;
class ParseTree;
class Room;
class RoomCollection;

class FrontendAccessor
{
public:
    virtual ~FrontendAccessor() = default;

    virtual void setFrontend(MapFrontend *in);

protected:
    MapFrontend *m_frontend = nullptr;

    Map &map();

    ParseTree &getParseTree();

    RoomIndex &roomIndex();
    Room *roomIndex(RoomId) const;

    RoomHomes &roomHomes();
    const SharedRoomCollection &roomHomes(RoomId) const;
};

class AbstractAction : public virtual FrontendAccessor
{
public:
    ~AbstractAction() override;
    virtual void preExec(RoomId) {}

    virtual void exec(RoomId id) = 0;

    virtual void insertAffected(RoomId id, std::set<RoomId> &affected) { affected.insert(id); }
};

class NODISCARD MapAction
{
    friend class MapFrontend;

public:
    virtual void schedule(MapFrontend *in) = 0;

    virtual ~MapAction();

protected:
    virtual void exec() = 0;

    virtual const std::set<RoomId> &getAffectedRooms() { return affectedRooms; }

    std::set<RoomId> affectedRooms{};
};

class SingleRoomAction final : public MapAction
{
public:
    explicit SingleRoomAction(std::unique_ptr<AbstractAction> ex, RoomId id);

    void schedule(MapFrontend *in) override { executor->setFrontend(in); }

protected:
    void exec() override
    {
        executor->preExec(id);
        executor->exec(id);
    }

    const RoomIdSet &getAffectedRooms() override;

private:
    RoomId id = INVALID_ROOMID;
    std::unique_ptr<AbstractAction> executor;
};

class AddExit : public MapAction, public FrontendAccessor
{
public:
    void schedule(MapFrontend *in) override { setFrontend(in); }

    explicit AddExit(RoomId in_from, RoomId in_to, ExitDirEnum in_dir);

protected:
    void exec() override;

    void tryExec();

    RoomId from;
    RoomId to;
    ExitDirEnum dir = ExitDirEnum::UNKNOWN;
};

class RemoveExit : public MapAction, public FrontendAccessor
{
public:
    explicit RemoveExit(RoomId from, RoomId to, ExitDirEnum dir);

    void schedule(MapFrontend *in) override { setFrontend(in); }

protected:
    void exec() override;

    void tryExec();

    RoomId from = DEFAULT_ROOMID;
    RoomId to = DEFAULT_ROOMID;
    ExitDirEnum dir = ExitDirEnum::UNKNOWN;
};

class MakePermanent final : public AbstractAction
{
public:
    void exec(RoomId RoomId) override;
};

class Update final : public virtual AbstractAction
{
public:
    Update();

    explicit Update(const SigParseEvent &sigParseEvent);

    void exec(RoomId id) override;

protected:
    ParseEvent props;
};

class ExitsAffecter : public AbstractAction
{
public:
    void insertAffected(RoomId id, std::set<RoomId> &affected) override;
};

class Remove : public ExitsAffecter
{
protected:
    void exec(RoomId id) override;
};
