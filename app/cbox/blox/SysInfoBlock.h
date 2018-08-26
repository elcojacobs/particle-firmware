/*
 * Copyright 2018 BrewPi B.V.
 *
 * This file is part of Controlbox
 *
 * Controlbox is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Controlbox.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Block.h"
#include "DataStream.h"
#include "Object.h"
#include "SysInfo.pb.h"
#if PLATFORM_ID != PLATFORM_GCC
#include "deviceid_hal.h"
#endif

// provides a protobuf interface to the read only system info
class SysInfoBlock : public cbox::Object {
    virtual cbox::CboxError streamTo(cbox::DataOut& out) const override final
    {
        blox_SysInfo message;

#if PLATFORM_ID != PLATFORM_GCC
        HAL_device_ID(static_cast<uint8_t*>(&message.deviceId[0]), 12);
#endif

        return streamProtoTo(out, &message, blox_SysInfo_fields, blox_SysInfo_size);
    }

    virtual cbox::CboxError streamFrom(cbox::DataIn& in) override final
    {
        return cbox::CboxError::OBJECT_NOT_WRITABLE;
    };

    virtual cbox::CboxError streamPersistedTo(cbox::DataOut& out) const override final
    {

        return cbox::CboxError::OK;
    }

    virtual cbox::update_t update(const cbox::update_t& now)
    {
        return update_never(now);
    }

    virtual cbox::obj_type_t typeId() const override final
    {
        return resolveTypeId(this);
    }
};
