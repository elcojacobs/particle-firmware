/*
 * Copyright 2014-2015 Matthew McGowan.
 *
 * This file is part of Nice Firmware.
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

#include "stddef.h"
#include "stdint.h"

#include "DataStream.h"
#include "EepromAccess.h"
#include "CboxMixins.h"
#include "ResolveType.h"


namespace cbox {

typedef int8_t container_id;

const container_id INVALID_ID = (container_id)(-1);

typedef uint16_t prepare_t;

namespace ObjectFlags {
enum Enum {
	Object = 0,
	Value = 4,			    // 0x000001xx are for value types. Base value type is stream only readable.
	Container = 8,
	WritableFlag = 1,		// flag for stream writable values
	hasStateFlag = 2,		// value (also) has state that can change when not written from stream
	OpenContainerFlag = 1,  // value to flag that a container supports the OpenContainer interface (that the container is writable.)
	NotLogged = 16,		    // flag to indicate that a value is not logged normally
	StaticlyAllocated = 32
};
}

/**
 * System flags for an object type. Objects are classified as
 * containers, open containers, readable, writable.
 */
typedef uint8_t object_flags_t;

#define delete_object(x) delete (x)

// have a hook for all object creations.
#define new_object(x) new x

#define cast_object_ptr(t, x) ((t*)x)

struct Object : virtual public ObjectMixin
{
public:
	Object() {}

	virtual ~Object() = default;

	/**
	 * Determines the system type of object this is.
	 * @return A value of the object_t enumeration indicating the type of object
	 * this is.
	 */
	virtual object_flags_t objectFlags() { return ObjectFlags::Object; }

	/**
	 * The application defined typeID for this object instance. Defined by derived class
	 */
	virtual obj_type_t typeID() = 0;


	/**
	 * Notifies this object that it has been created and is operational in the system.
	 * The eeprom address that contains the object's definition is provided for instances
	 * that want to retrieve or amend their definition details.
	 * @param eeprom_address offset in eeprom that defines the data for this object. the length
	 * Preceeding this address is the length, then id_chain, and before that, the creation command. (0x03)
	 */
	virtual void rehydrated(eptr_t /*eeprom_address*/) {}

	/**
	 * Prepare this object for subsequent updates.
	 * The returned value is the number of milliseconds the object needs before updates can be performed.
	 */
	virtual prepare_t prepare() { return 0; }

	/**
	 * Called after prepare to update this object's state.
	 */
	virtual void update() { }

};

const uint8_t MAX_CONTAINER_DEPTH = 3;
const container_id MAX_CONTAINER_ID = 127;

/**
 * A container that you cannot open. You can see the objects inside the container, but not add new ones.
 */
struct Container : public Object
{
	virtual object_flags_t objectFlags() override { return Object::objectFlags() | ObjectFlags::Container; }

	/**
	 * Fetches the object with the given id.
	 * /return the object with that id, which may be null.
	 *
	 * After retrieving the item, callers must call returnItem()
	 */
	virtual Object* item(container_id /*id*/) { return NULL; }

	/**
	 * Returns a previously fetched item back the container.
	 * @param id		The id the item had in this container.
	 * @param item	The object to return to the container.
	 *
	 * This method should be called after each successful call to
	 * {@link #item}
	 */
	virtual void returnItem(container_id /*id*/, Object* /*item*/) { }

	/*
	 * The maximum number of items in this container. Calling {@link #item()} at an index less than this value
	 * may return {@code NULL}. This is provided so callers know
	 * the upper limit for indexes to iterate over for this container.
	 */
	virtual container_id size() { return 0; }

};


/**
 * A container that creates it's contained items on demand.
 */
class FactoryContainer : public Container {
public:

	/**
	 * Deletes the item. This assumes item was created on-demand by the item() method.
	 */
	virtual void returnItem(container_id /*id*/, Object* item) override {
		delete_object(item);
	}
};


/**
 * A container of objects that is open - i.e. you can put things in it.
 */
class OpenContainer : public Container
{
public:
	virtual object_flags_t objectFlags() override { return Container::objectFlags() | ObjectFlags::OpenContainerFlag; }

	/*
	 * Add the given object to the container at the given slot.
	 * The container guarantees the object will be available at the slot until removed.
	 * @param index	The index of the slot. >=0.
	 * @param item	The object to add.
	 * @return non-zero on success, zero on error.
	 * The current size of the container may be less than the slot. If the container can resize
	 * to make additional slots available, it should do so, but this is an optional operation for
	 * fixed size containers.
	 */
	virtual bool add(container_id /*index*/, Object* /*item*/) { return false; }

	/**
	 * Determines the next available free slot in this container.
	 * @return A value greater or equal to zero - the next available free slot. Negative value
	 *	indicates no more free slots.
	 */
	virtual container_id next() { return -1; }

	/**
	 * Removes the item at the given index.
	 * @param id	The id of the item to remove.
	 * If there is no item at the given index, or the item has already been removed the method does nothing.
	 */
	virtual void remove(container_id /*id*/) { }

};

/**
 * A basic value type. All values are as a minimum stream readable, meaning they can push their value to a stream
 * (a streamed read operation.)
 */
class Value : public Object {

public:

	virtual object_flags_t objectFlags() override { return Object::objectFlags() | ObjectFlags::Value; }	// basic value type - read only stream
	virtual void readTo(DataOut& out)=0;
	virtual uint8_t readStreamSize()=0;			// the size this value occupies in the stream.

};

class WritableValue : public Value {
public:
	virtual object_flags_t objectFlags() override { return Value::objectFlags() | ObjectFlags::WritableFlag; }
	virtual void writeFrom(DataIn& dataIn)=0;
	virtual uint8_t writeStreamSize() { return readStreamSize(); }
};

/**
 * A base class for objects that want to know where in the persisted data their definition is stored.
 */
class EepromAwareValue: public Value
{
	eptr_t address = -1;
public:

	virtual void rehydrated(eptr_t _address) override final {
		address = _address;
	}

	eptr_t eeprom_offset() { return address; }
	uint8_t eepromSize(cb_nonstatic_decl(EepromAccess& eepromAccess)) { return eepromAccess.readByte(address-1); }

};

/**
 * A base class for writable objects that want to know where in the persisted data their definition is stored.
 * This code repetition is to avoid multiple inheritance
 */
class EepromAwareWritableValue : public WritableValue
{
	eptr_t address = -1;
public:

	virtual void rehydrated(eptr_t _address) override final {
		address = _address;
	}

	eptr_t eeprom_offset() { return address; }
	uint8_t eepromSize(cb_nonstatic_decl(EepromAccess& eepromAccess)) { return eepromAccess.readByte(address-1); }

};


/**
 * Definition parameters for creating a new object.
 */
struct ObjectDefinition {

#if !CONTROLBOX_STATIC
	EepromAccess& ea;

	Container* root;

	EepromAccess& eepromAccess() { return ea; }
#endif


	/**
	 * This stream provides the definition data for this object
	 */
	DataIn* in;

	/**
	 * The number of bytes in the stream for the object definition
	 */
	uint8_t len;

	/**
	 * The application defined type of this object.
	 */
	obj_type_t type;

	/**
	 * Ensure all the data is read from the datastream. This is only required
	 * if the application doesn't read all of the data. (Calling this when
	 * all the data has been read is a no-op.)
	 */
	void spool();
};

inline bool hasFlags(uint8_t value, uint8_t flags) {
    return ((value&flags)==flags);
}

inline bool isContainer(Object* o)
{
	return o!=NULL && (hasFlags(o->objectFlags(), ObjectFlags::Container));
}

inline bool isOpenContainer(Object* o)
{
	return o!=NULL && (hasFlags(o->objectFlags(), (ObjectFlags::Container|ObjectFlags::OpenContainerFlag)));
}

inline bool isValue(Object* o)
{
	return o!=NULL && (hasFlags(o->objectFlags(), ObjectFlags::Value));
}

inline bool isLoggedValue(Object* o)
{
	return o!=NULL && (o->objectFlags() & (ObjectFlags::Value|ObjectFlags::NotLogged))==ObjectFlags::Value;
}

inline bool isDynamicallyAllocated(Object* o)
{
	return o!=NULL && (o->objectFlags() & ObjectFlags::StaticlyAllocated)==0;
}

inline bool isWritable(Object* o)
{
	return o!=NULL && (hasFlags(o->objectFlags(), ObjectFlags::WritableFlag));
}

/*
 * Callback function for enumerating objects.
 * The function can return true to stop enumeration.
 *
 * @param id	The start of the id chain
 * @param end	The end of the id chain. When this is equal to id, the function callback pertains to the root container.
 * @param enter	When {@code true} this call is entering this portion of the hierarchy. This is called before any
 *   child objects have been enumerated.
 *		When {@code false} this call is exiting this portion of the hierarchy. This is called after all
 *   child objects have been enumerated.
 */
typedef bool (*EnumObjectsFn)(Object* obj, void* data, const container_id* id, const container_id* end, bool enter);


bool walkContainer(Container* c, EnumObjectsFn callback, void* data, container_id* id, container_id* end);

bool walkObject(Object* obj, EnumObjectsFn callback, void* data, container_id* id, container_id* end);


/**
 * Enumerate all objects the of root container and child containers.
 */
inline bool walkRoot(Container* root, EnumObjectsFn callback, void* data, container_id* id) {
	return walkContainer(root, callback, data, id, id);
}

/**
 * Read the id chain from the stream and resolve the corresponding object.
 */
Object* lookupObject(Object* current, DataIn& data);

/**
 * Read the id chain from the stream and resolve the container and the final index.
 */
Container* lookupContainer(Object* current, DataIn& data, int8_t& lastID);

/**
 * Fetches the object at a given index in a container.
 */
Object* fetchContainedObject(Object* o, container_id id);

/**
 * Read the id chain from the stream and resolve the corresponding object.
 */
Object* lookupUserObject(Container* root, DataIn& data);

/**
 * Read the id chain from the stream and resolve the container and the final index.
 */
OpenContainer* lookupUserOpenContainer(Container* root, DataIn& data, int8_t& lastID);

/**
 * Fetches the object at the given id, and retrieves the last ID.
 */
Object* lookupObject(Object* current, DataIn& data, int8_t& lastID);

int16_t read2BytesFrom(Value* value);

obj_type_t readObjTypeFrom(DataIn & in);

}
