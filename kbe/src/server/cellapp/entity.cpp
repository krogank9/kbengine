/*
This source file is part of KBEngine
For the latest info, see http://www.kbengine.org/

Copyright (c) 2008-2018 KBEngine.

KBEngine is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

KBEngine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.
 
You should have received a copy of the GNU Lesser General Public License
along with KBEngine.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "cellapp.h"
#include "entity.h"
#include "witness.h"	
#include "profile.h"
#include "space.h"
#include "range_trigger.h"
#include "all_clients.h"
#include "client_entity.h"
#include "controllers.h"	
#include "real_entity_method.h"
#include "entity_coordinate_node.h"
#include "proximity_controller.h"
#include "move_controller.h"	
#include "moveto_point_handler.h"	
#include "moveto_entity_handler.h"	
#include "navigate_handler.h"	
#include "rotator_handler.h"
#include "turn_controller.h"
#include "pyscript/py_gc.h"
#include "entitydef/volatileinfo.h"
#include "entitydef/entity_call.h"
#include "entitydef/entity_component.h"
#include "network/channel.h"	
#include "network/bundle.h"	
#include "network/fixed_messages.h"
#include "network/network_stats.h"
#include "client_lib/client_interface.h"
#include "helper/eventhistory_stats.h"
#include "navigation/navigation.h"
#include "math/math.h"

#include "../../server/baseapp/baseapp_interface.h"
#include "../../server/cellapp/cellapp_interface.h"

#ifndef CODE_INLINE
#include "entity.inl"
#endif

namespace KBEngine{

//-------------------------------------------------------------------------------------
ENTITY_METHOD_DECLARE_BEGIN(Cellapp, Entity)
SCRIPT_METHOD_DECLARE("setViewRadius",				pySetViewRadius,				METH_VARARGS,				0)
SCRIPT_METHOD_DECLARE("getViewRadius",				pyGetViewRadius,				METH_VARARGS,				0)
SCRIPT_METHOD_DECLARE("getViewHystArea",			pyGetViewHystArea,				METH_VARARGS,				0)
SCRIPT_METHOD_DECLARE("isReal",						pyIsReal,						METH_VARARGS,				0)	
SCRIPT_METHOD_DECLARE("addProximity",				pyAddProximity,					METH_VARARGS,				0)
SCRIPT_METHOD_DECLARE("addYawRotator",				pyAddYawRotator,				METH_VARARGS,				0)
SCRIPT_METHOD_DECLARE("clientEntity",				pyClientEntity,					METH_VARARGS,				0)
SCRIPT_METHOD_DECLARE("cancelController",			pyCancelController,				METH_VARARGS,				0)
SCRIPT_METHOD_DECLARE("canNavigate",				pycanNavigate,					METH_VARARGS,				0)
SCRIPT_METHOD_DECLARE("navigatePathPoints",			pyNavigatePathPoints,			METH_VARARGS,				0)
SCRIPT_METHOD_DECLARE("navigate",					pyNavigate,						METH_VARARGS,				0)
SCRIPT_METHOD_DECLARE("getRandomPoints",			pyGetRandomPoints,				METH_VARARGS,				0)
SCRIPT_METHOD_DECLARE("moveToPoint",				pyMoveToPoint,					METH_VARARGS,				0)
SCRIPT_METHOD_DECLARE("moveToEntity",				pyMoveToEntity,					METH_VARARGS,				0)
SCRIPT_METHOD_DECLARE("accelerate",					pyAccelerate,					METH_VARARGS,				0)
SCRIPT_METHOD_DECLARE("entitiesInRange",			pyEntitiesInRange,				METH_VARARGS,				0)
SCRIPT_METHOD_DECLARE("entitiesInView",				pyEntitiesInView,				METH_VARARGS,				0)
SCRIPT_METHOD_DECLARE("teleport",					pyTeleport,						METH_VARARGS,				0)
SCRIPT_METHOD_DECLARE("destroySpace",				pyDestroySpace,					METH_VARARGS,				0)
SCRIPT_METHOD_DECLARE("debugView",					pyDebugView,					METH_VARARGS,				0)
ENTITY_METHOD_DECLARE_END()

SCRIPT_MEMBER_DECLARE_BEGIN(Entity)
SCRIPT_MEMBER_DECLARE_END()

ENTITY_GETSET_DECLARE_BEGIN(Entity)
SCRIPT_GET_DECLARE("base",							pyGetBaseEntityCall,			0,							0)
SCRIPT_GET_DECLARE("client",						pyGetClientEntityCall,			0,							0)
SCRIPT_GET_DECLARE("allClients",					pyGetAllClients,				0,							0)
SCRIPT_GET_DECLARE("otherClients",					pyGetOtherClients,				0,							0)
SCRIPT_GET_DECLARE("isWitnessed",					pyIsWitnessed,					0,							0)
SCRIPT_GET_DECLARE("hasWitness",					pyHasWitness,					0,							0)
SCRIPT_GET_DECLARE("isOnGround",					pyGetIsOnGround,				0,							0)
SCRIPT_GET_DECLARE("spaceID",						pyGetSpaceID,					0,							0)
SCRIPT_GETSET_DECLARE("layer",						pyGetLayer,						pySetLayer,					0,		0)
SCRIPT_GETSET_DECLARE("position",					pyGetPosition,					pySetPosition,				0,		0)
SCRIPT_GETSET_DECLARE("direction",					pyGetDirection,					pySetDirection,				0,		0)
SCRIPT_GETSET_DECLARE("topSpeed",					pyGetTopSpeed,					pySetTopSpeed,				0,		0)
SCRIPT_GETSET_DECLARE("topSpeedY",					pyGetTopSpeedY,					pySetTopSpeedY,				0,		0)
SCRIPT_GETSET_DECLARE("controlledBy",				pyGetControlledBy,				pySetControlledBy,			0,		0)
SCRIPT_GETSET_DECLARE("volatileInfo",				pyGetVolatileinfo,				pySetVolatileinfo,			0,		0)
ENTITY_GETSET_DECLARE_END()
BASE_SCRIPT_INIT(Entity, 0, 0, 0, 0, 0)	

Entity::BufferedScriptCallArray Entity::_scriptCallbacksBuffer;
int32 Entity::_scriptCallbacksBufferCount = 0;
int32 Entity::_scriptCallbacksBufferNum = 0;

//-------------------------------------------------------------------------------------
Entity::Entity(ENTITY_ID id, const ScriptDefModule* pScriptModule):
ScriptObject(getScriptType(), true),
ENTITY_CONSTRUCTION(Entity),
clientEntityCall_(NULL),
baseEntityCall_(NULL),
realCell_(0),
ghostCell_(0),
lastpos_(),
position_(),
pPyPosition_(NULL),
direction_(),
pPyDirection_(NULL),
posChangedTime_(0),
dirChangedTime_(0),
isOnGround_(false),
topSpeed_(-0.1f),
topSpeedY_(-0.1f),
witnesses_(),
witnesses_count_(0),
pWitness_(NULL),
allClients_(new AllClients(pScriptModule, id, false)),
otherClients_(new AllClients(pScriptModule, id, true)),
pEntityCoordinateNode_(NULL),
pControllers_(new Controllers(id)),
pyPositionChangedCallback_(),
pyDirectionChangedCallback_(),
layer_(0),
isDirty_(true),
pCustomVolatileinfo_(NULL)
{
	pyPositionChangedCallback_ = std::tr1::bind(&Entity::onPyPositionChanged, this);
	pyDirectionChangedCallback_ = std::tr1::bind(&Entity::onPyDirectionChanged, this);
	pPyPosition_ = new script::ScriptVector3(&position(), &pyPositionChangedCallback_);
	pPyDirection_ = new script::ScriptVector3(&direction().dir, &pyDirectionChangedCallback_);

	ENTITY_INIT_PROPERTYS(Entity);

	if(g_kbeSrvConfig.getCellApp().use_coordinate_system)
	{
		pEntityCoordinateNode_ = new EntityCoordinateNode(this);
	}

	script::PyGC::incTracing("Entity");
}

//-------------------------------------------------------------------------------------
Entity::~Entity()
{
	ENTITY_DECONSTRUCTION(Entity);

	S_RELEASE(pCustomVolatileinfo_);

	S_RELEASE(clientEntityCall_);
	S_RELEASE(baseEntityCall_);
	S_RELEASE(allClients_);
	S_RELEASE(otherClients_);
	
	KBE_ASSERT(pWitness_ == NULL);

	SAFE_RELEASE(pControllers_);
	KBE_ASSERT(pEntityCoordinateNode_ == NULL);

	Py_DECREF(pPyPosition_);
	pPyPosition_ = NULL;
	
	Py_DECREF(pPyDirection_);
	pPyDirection_ = NULL;

	if(Cellapp::getSingleton().pEntities())
		Cellapp::getSingleton().pEntities()->pGetbages()->erase(id());

	script::PyGC::decTracing("Entity");
}	

//-------------------------------------------------------------------------------------
void Entity::installCoordinateNodes(CoordinateSystem* pCoordinateSystem)
{
	if(g_kbeSrvConfig.getCellApp().use_coordinate_system)
		pCoordinateSystem->insert((KBEngine::CoordinateNode*)pEntityCoordinateNode());
}

//-------------------------------------------------------------------------------------
void Entity::uninstallCoordinateNodes(CoordinateSystem* pCoordinateSystem)
{
	if(g_kbeSrvConfig.getCellApp().use_coordinate_system)
	{
		pCoordinateSystem->remove((KBEngine::CoordinateNode*)pEntityCoordinateNode());
		pEntityCoordinateNode_ = new EntityCoordinateNode(this);
	}
}

//-------------------------------------------------------------------------------------
void Entity::onCoordinateNodesDestroy(EntityCoordinateNode* pEntityCoordinateNode)
{
	if (pEntityCoordinateNode_ == pEntityCoordinateNode)
		pEntityCoordinateNode_ = NULL;
}

//-------------------------------------------------------------------------------------
void Entity::onDestroy(bool callScript)
{
	if(callScript && isReal())
	{
		SCOPED_PROFILE(SCRIPTCALL_PROFILE);
		CALL_ENTITY_AND_COMPONENTS_METHOD(this, SCRIPT_OBJECT_CALL_ARGS0(pyTempObj, const_cast<char*>("onDestroy"), false));

		// If you do not notify the script, then this callback will not be generated
		// Often if destroying an entity does not notify the script it may have been caused by a migration or teleport
		if(baseEntityCall_ != NULL)
		{
			setDirty();
			this->backupCellData();

			Network::Bundle* pBundle = Network::Bundle::createPoolObject();
			(*pBundle).newMessage(BaseappInterface::onLoseCell);
			(*pBundle) << id_;
			baseEntityCall_->sendCall(pBundle);
		}
	}

	stopMove();

	// Remove the controller's reference
	S_RELEASE(controlledBy_);

	if(pWitness_)
	{
		pWitness_->detach(this);
		Witness::reclaimPoolObject(pWitness_);
		pWitness_ = NULL;
	}

	// Remove this entity from its space
	Space* space = Spaces::findSpace(this->spaceID());
	if(space)
	{
		space->removeEntity(this);
	}
	else
	{
		WARNING_MSG(fmt::format("{}::onDestroy(): {}, not found space({})!\n", 
			this->scriptName(), this->id(), spaceID()));
	}
	
	// This may not be 0 when the process is forced off
	//KBE_ASSERT(spaceID() == 0);

	// There should not be witnesses at this time, otherwise it's a View BUG
	if (witnesses_count_ > 0)
	{
		ERROR_MSG(fmt::format("{}::onDestroy(): id={}, witnesses_count({}/{}) != 0, isReal={}, spaceID={}, position=({},{},{})\n", 
			scriptName(), id(), witnesses_count_, witnesses_.size(), isReal(), this->spaceID(), position().x, position().y, position().z));

		std::list<ENTITY_ID> witnesses_copy = witnesses_;
		std::list<ENTITY_ID>::iterator it = witnesses_copy.begin();
		for (; it != witnesses_copy.end(); ++it)
		{
			Entity *ent = Cellapp::getSingleton().findEntity((*it));

			if (ent)
			{
				bool inTargetView = false;

				if (ent->pWitness())
				{
					Witness::VIEW_ENTITIES::iterator view_iter = ent->pWitness()->viewEntities().begin();
					for (; view_iter != ent->pWitness()->viewEntities().end(); ++view_iter)
					{
						if ((*view_iter)->pEntity() == this)
						{
							inTargetView = true;
							ent->pWitness()->_onLeaveView((*view_iter));
							break;
						}
					}
				}
				else
				{
					ent->delWitnessed(this);
				}
				
				ERROR_MSG(fmt::format("\t=>witnessed={}({}), isDestroyed={}, isReal={}, inTargetView={}, spaceID={}, position=({},{},{})\n", 
					ent->scriptName(), (*it), ent->isDestroyed(), ent->isReal(), inTargetView, ent->spaceID(), ent->position().x, ent->position().y, ent->position().z));
			}
			else
			{
				ERROR_MSG(fmt::format("\t=> witnessed={}, not found entity!\n", (*it)));
			}
			
			witnesses_count_ = 0;
			witnesses_.clear();
		}

		//KBE_ASSERT(witnesses_count_ == 0);
	}

	pPyPosition_->onLoseRef();
	pPyDirection_->onLoseRef();

	SAFE_RELEASE(pEntityCoordinateNode_);
}

//-------------------------------------------------------------------------------------
PyObject* Entity::__py_pyDestroyEntity(PyObject* self, PyObject* args, PyObject * kwargs)
{
	uint16 currargsSize = PyTuple_Size(args);
	Entity* pobj = static_cast<Entity*>(self);

	if(pobj->initing())
	{
		PyErr_Format(PyExc_AssertionError,
			"%s::destroy(): %d initing, reject the request!\n",
			pobj->scriptName(), pobj->id());
		PyErr_PrintEx(0);
		return NULL;
	}
	else if (pobj->isDestroyed())
	{
		PyErr_Format(PyExc_AssertionError, "%s::destroy: %d is destroyed!\n",
			pobj->scriptName(), pobj->id());
		PyErr_PrintEx(0);
		return NULL;
	}
	else if(currargsSize > 0)
	{
		PyErr_Format(PyExc_AssertionError,
			"%s: args max require %d args, gived %d! is script(%s), id(%d)!\n",	
			__FUNCTION__, 0, currargsSize, pobj->scriptName(), pobj->id());
		PyErr_PrintEx(0);
		return NULL;
	}

	pobj->destroyEntity();

	S_Return;
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyDestroySpace()
{
	if(!isReal())
	{
		PyErr_Format(PyExc_AssertionError, "%s::destroySpace: not is real entity(%d).", 
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	if (!hasFlags(ENTITY_FLAGS_DESTROYING) && this->isDestroyed())
	{
		PyErr_Format(PyExc_AssertionError, "%s::destroySpace: %d is destroyed!\n",
			scriptName(), id());		
		PyErr_PrintEx(0);
		return 0;
	}
	
	if(spaceID() == 0)
	{
		PyErr_Format(PyExc_TypeError, "%s::destroySpace: spaceID is 0.\n", scriptName());
		PyErr_PrintEx(0);
		S_Return;
	}

	destroySpace();
	S_Return;
}	

//-------------------------------------------------------------------------------------
void Entity::destroySpace()
{
	if(spaceID() == 0)
		return;

	Spaces::destroySpace(spaceID(), this->id());
}

//-------------------------------------------------------------------------------------
void Entity::onSpaceGone()
{
	SCOPED_PROFILE(SCRIPTCALL_PROFILE);
	CALL_ENTITY_AND_COMPONENTS_METHOD(this, SCRIPT_OBJECT_CALL_ARGS0(pyTempObj, const_cast<char*>("onSpaceGone"), false));
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyGetBaseEntityCall()
{ 
	EntityCall* entityCall = baseEntityCall();
	if(entityCall == NULL)
		S_Return;

	Py_INCREF(entityCall);
	return entityCall; 
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyGetControlledBy()
{
	EntityCall* entityCall = controlledBy();
	if(entityCall == NULL)
		S_Return;

	Py_INCREF(entityCall);
	return entityCall; 
}

//-------------------------------------------------------------------------------------
int Entity::pySetControlledBy(PyObject *value)
{
	if (isDestroyed())
	{
		PyErr_Format(PyExc_AssertionError, "%s: %d is destroyed!\n",		
			scriptName(), id());		
		PyErr_PrintEx(0);
		return 0;
	}

	if (!isReal())
	{
		PyErr_Format(PyExc_AssertionError, "%s::controlledBy: is not real entity(%d).",
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	EntityCall* entityCall = NULL;

	if (value != Py_None)
	{
		if (!PyObject_TypeCheck(value, EntityCall::getScriptType()) || !((EntityCall *)value)->isBase())
		{
			PyErr_Format(PyExc_AssertionError, "%s: param must be base entity entityCall!\n",
				scriptName());
			PyErr_PrintEx(0);
			return 0;
		}

		entityCall = static_cast<EntityCall *>(value);

		// If controlling entity can't see this one, can't control it
		if (!entityInWitnessed(entityCall->id()) && entityCall->id() != id())
		{
			PyErr_Format(PyExc_AssertionError, "%s: entity '%d' isn't witnessing me!\n",
				scriptName(), entityCall->id());
			PyErr_PrintEx(0);
			return 0;
		}

		Entity *ent = Cellapp::getSingleton().findEntity(entityCall->id());
		if (!ent || !ent->clientEntityCall())
		{
			PyErr_Format(PyExc_AssertionError, "%s: entity(%d) entityCall has no 'client' entityCall!\n",
				scriptName());
			PyErr_PrintEx(0);
			return 0;
		}
	}

	setControlledBy(entityCall);
	return 0;
}

bool Entity::setControlledBy(EntityCall* controllerBaseEntityCall)
{
	EntityCall *oldEntityCall = controlledBy();

	// If the old and new entityCall are the same person, do not make any changes
	if (oldEntityCall != NULL && controllerBaseEntityCall != NULL &&
		oldEntityCall->id() == controllerBaseEntityCall->id())
	{
		ERROR_MSG(fmt::format("Entity {0} is already a controller, don't repeat settings\n", oldEntityCall->id()));
		return false;
	}

	if (oldEntityCall != NULL)
	{
		// If the old controller is my own client,
		// Then you need to inform your client: You can no longer control yourself, that is, you are controlled by others.
		if (oldEntityCall->id() == id())
			sendControlledByStatusMessage(oldEntityCall, 1);

		// If the old controller is also one of this entity's observers, it means that its clients can see this entity,
		//  so need to notify the old client that you can't control our movement anymore.
		else if (entityInWitnessed(oldEntityCall->id()))
			sendControlledByStatusMessage(oldEntityCall, 0);

		if (controllerBaseEntityCall != NULL)
		{
			controlledBy(controllerBaseEntityCall);

			// If it is to restore self-control, then you need to notify my client: no one controls you
			if (controllerBaseEntityCall->id() == id())
			{
				KBE_ASSERT(clientEntityCall_);
				sendControlledByStatusMessage(controllerBaseEntityCall, 0);
			}

			// If someone else took was controlling, only need to notify the receiver.
			//    -Because he was still under the control of others, he did not need to be notified.
			// So, inform the controller who took over: you/your entity
			else
			{
				sendControlledByStatusMessage(controllerBaseEntityCall, 1);
			}

		}
		else  // NULL means it is controlled by the system so there is no need to notify others
		{
			controlledBy(NULL);
		}
	}
	else if (controllerBaseEntityCall != NULL)
	{
		controlledBy(controllerBaseEntityCall);
		
		// Since there are new controllers, the system's move behavior must stop.
		stopMove();
		
		// If it is to restore self-control, then you need to notify my client: no one controls you
		if (controllerBaseEntityCall->id() == id())
		{
			KBE_ASSERT(clientEntityCall_);
			sendControlledByStatusMessage(controllerBaseEntityCall, 0);
		}

		// If someone else took was controlling, only need to notify the receiver.
		//    -Because he was still under the control of others, he did not need to be notified.
		// So, inform the controller who took over: you/your entity
		else
		{
			sendControlledByStatusMessage(controllerBaseEntityCall, 1);
		}
	}

	return true;
}

void Entity::sendControlledByStatusMessage(EntityCall* baseEntityCall, int8 isControlled)
{
	KBE_ASSERT(baseEntityCall);

	Network::Channel* pChannel = NULL;

	PyObject* clientMB = PyObject_GetAttrString(baseEntityCall, "client");
	if (clientMB != Py_None)
	{
		pChannel = (static_cast<EntityCall*>(clientMB))->getChannel();
	}

	Py_DECREF(clientMB);

	if (!pChannel)
		return;

	Network::Bundle* pSendBundle = Network::Bundle::ObjPool().createObject();
	Network::Bundle* pForwardBundle = Network::Bundle::ObjPool().createObject();

	(*pForwardBundle).newMessage(ClientInterface::onControlEntity);
	(*pForwardBundle) << id();
	(*pForwardBundle) << isControlled;

	NETWORK_ENTITY_MESSAGE_FORWARD_CLIENT(baseEntityCall->id(), (*pSendBundle), (*pForwardBundle));
	pChannel->send(pSendBundle);
	Network::Bundle::ObjPool().reclaimObject(pForwardBundle);
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyGetClientEntityCall()
{ 
	EntityCall* entityCall = clientEntityCall();
	if(entityCall == NULL)
		S_Return;

	Py_INCREF(entityCall);
	return entityCall; 
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyGetAllClients()
{ 
	AllClients* clients = allClients();
	if(clients == NULL)
		S_Return;

	Py_INCREF(clients);
	return clients; 
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyGetOtherClients()
{ 
	AllClients* clients = otherClients();
	if(clients == NULL)
		S_Return;

	Py_INCREF(clients);
	return clients; 
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyGetIsOnGround()
{ 
	return PyBool_FromLong(isOnGround());
}

//-------------------------------------------------------------------------------------
int Entity::pySetTopSpeedY(PyObject *value)
{
	topSpeedY(float(PyFloat_AsDouble(value)) / g_kbeSrvConfig.gameUpdateHertz()); 
	return 0; 
};

//-------------------------------------------------------------------------------------
PyObject* Entity::pyGetTopSpeedY()
{ 
	return PyFloat_FromDouble(topSpeedY() * g_kbeSrvConfig.gameUpdateHertz()); 
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyGetTopSpeed()
{ 
	return PyFloat_FromDouble(topSpeed() * g_kbeSrvConfig.gameUpdateHertz()); 
}

//-------------------------------------------------------------------------------------
int Entity::pySetTopSpeed(PyObject *value)
{ 
	topSpeed(float(PyFloat_AsDouble(value)) / g_kbeSrvConfig.gameUpdateHertz()); 
	return 0; 
}

//-------------------------------------------------------------------------------------
PyObject* Entity::onScriptGetAttribute(PyObject* attr)
{
	DEBUG_OP_ATTRIBUTE("get", attr)

	wchar_t* PyUnicode_AsWideCharStringRet0 = PyUnicode_AsWideCharString(attr, NULL);
	char* ccattr = strutil::wchar2char(PyUnicode_AsWideCharStringRet0);
	PyMem_Free(PyUnicode_AsWideCharStringRet0);
		
	// If it is ghost, calling def method requires rpc call.
	if(!isReal())
	{
		MethodDescription* pMethodDescription = const_cast<ScriptDefModule*>(pScriptModule())->findCellMethodDescription(ccattr);
		
		if(pMethodDescription)
		{
			free(ccattr);
			return new RealEntityMethod(pMethodDescription, this);
		}
	}
	else
	{
		// If you access the def persistent class container property
		// Since there's no good monitoring of internal changes in the properties of the container class, use a compromise here
		PropertyDescription* pPropertyDescription = const_cast<ScriptDefModule*>(pScriptModule())->findPersistentPropertyDescription(ccattr);
		if(pPropertyDescription && (pPropertyDescription->getFlags() & ENTITY_CELL_DATA_FLAGS) > 0)
		{
			setDirty();
		}
	}
	
	free(ccattr);
	return ScriptObject::onScriptGetAttribute(attr);
}	

//-------------------------------------------------------------------------------------
void Entity::onDefDataChanged(EntityComponent* pEntityComponent, const PropertyDescription* propertyDescription, PyObject* pyData)
{
	// If it's not a realentity or it's initializing, ignore it.
	if(!isReal() || initing())
		return;

	if(propertyDescription->isPersistent())
		setDirty();
	
	ENTITY_PROPERTY_UID componentPropertyUID =0;
	int8 componentPropertyAliasID = 0;

	if (pEntityComponent)
	{
		componentPropertyUID = (pEntityComponent ? pEntityComponent->pPropertyDescription()->getUType() : (ENTITY_PROPERTY_UID)0);
		componentPropertyAliasID = (pEntityComponent ? pEntityComponent->pPropertyDescription()->aliasIDAsUint8() : 0);
	}

	uint32 flags = propertyDescription->getFlags();

	// First create a template stream that needs to be broadcast
	MemoryStream* mstream = MemoryStream::createPoolObject();

	EntityDef::context().currComponentType = g_componentType;
	propertyDescription->getDataType()->addToStream(mstream, pyData);

	// To determine if it needs to be broadcast to other cellapps, this also 
	//  requires the premise that the entity must have a ghost entity
	// Only entities within a certain range of cell boundaries have the ghost entity, 
	//  or they are temporarily set to the ghost state when teleporting to another space.
	if((flags & ENTITY_BROADCAST_CELL_FLAGS) > 0 && hasGhost())
	{
		GhostManager* gm = Cellapp::getSingleton().pGhostManager();
		if(gm)
		{
			Network::Bundle* pForwardBundle = gm->createSendBundle(ghostCell());
			(*pForwardBundle).newMessage(CellappInterface::onUpdateGhostPropertys);
			(*pForwardBundle) << id();
			(*pForwardBundle) << componentPropertyUID;
			(*pForwardBundle) << propertyDescription->getUType();

			pForwardBundle->append(*mstream);

			// Record the amount of data generated by this event
			g_publicCellEventHistoryStats.trackEvent(scriptName(), 
				propertyDescription->getName(), 
				pForwardBundle->currMsgLength());

			gm->pushMessage(ghostCell(), pForwardBundle);
		}
	}
	
	const Position3D& basePos = this->position(); 
	if((flags & ENTITY_BROADCAST_OTHER_CLIENT_FLAGS) > 0)
	{
		DETAIL_TYPE propertyDetailLevel = propertyDescription->getDetailLevel();

		std::list<ENTITY_ID>::iterator witer = witnesses_.begin();
		for(; witer != witnesses_.end(); ++witer)
		{
			Entity* pEntity = Cellapp::getSingleton().findEntity((*witer));
			if(pEntity == NULL || pEntity->pWitness() == NULL)
				continue;

			EntityCall* clientEntityCall = pEntity->clientEntityCall();
			if(clientEntityCall == NULL)
				continue;

			Network::Channel* pChannel = clientEntityCall->getChannel();
			if(pChannel == NULL)
				continue;

			// It's possible that, for example, the data comes from createWitnessFromStream()
			// Or if their own entity is not yet created on the target client
			if(!pEntity->pWitness()->entityInView(id()))
				continue;

			const Position3D& targetPos = pEntity->position();
			Position3D lengthPos = targetPos - basePos;

			if(pScriptModule_->getDetailLevel().level[propertyDetailLevel].inLevel(lengthPos.length()))
			{
				Network::Bundle* pSendBundle = pChannel->createSendBundle();
				NETWORK_ENTITY_MESSAGE_FORWARD_CLIENT_BEGIN(pEntity->id(), (*pSendBundle));
				
				int ialiasID = -1;
				const Network::MessageHandler& msgHandler = pEntity->pWitness()->getViewEntityMessageHandler(ClientInterface::onUpdatePropertys, 
					ClientInterface::onUpdatePropertysOptimized, id(), ialiasID);
				
				ENTITY_MESSAGE_FORWARD_CLIENT_BEGIN(pSendBundle, msgHandler, viewEntityMessage);
				
				if(ialiasID != -1)
				{
					KBE_ASSERT(msgHandler.msgID == ClientInterface::onUpdatePropertysOptimized.msgID);
					(*pSendBundle)  << (uint8)ialiasID;
				}
				else
				{
					KBE_ASSERT(msgHandler.msgID == ClientInterface::onUpdatePropertys.msgID);
					(*pSendBundle)  << id();
				}
				
				if (pScriptModule_->usePropertyDescrAlias())
				{
					(*pSendBundle) << componentPropertyAliasID;
					(*pSendBundle) << propertyDescription->aliasIDAsUint8();
				}
				else
				{
					(*pSendBundle) << componentPropertyUID;
					(*pSendBundle) << propertyDescription->getUType();
				}

				pSendBundle->append(*mstream);
				
				// Record the amount of data generated by this event
				g_publicClientEventHistoryStats.trackEvent(scriptName(), 
					propertyDescription->getName(), 
					pSendBundle->currMsgLength());

				ENTITY_MESSAGE_FORWARD_CLIENT_END(pSendBundle, msgHandler, viewEntityMessage);

				pEntity->pWitness()->sendToClient(ClientInterface::onUpdatePropertysOptimized, pSendBundle);
			}
		}
	}

	/*
	// Determine if this attribute still needs to be broadcast to other clients
	if((flags & ENTITY_BROADCAST_OTHER_CLIENT_FLAGS) > 0)
	{
		int8 detailLevel = propertyDescription->getDetailLevel();
		for(int8 i=DETAIL_LEVEL_NEAR; i<=detailLevel; ++i)
		{
			std::map<ENTITY_ID, Entity*>::iterator iter = witnessEntities_[i].begin();
			for(; iter != witnessEntities_[i].end(); ++iter)
			{
				Entity* entity = iter->second;
				EntityCall* clientEntityCall = entity->clientEntityCall();
				if(clientEntityCall != NULL)
				{
					Packet* sp = clientEntityCall->newCall(ENTITYCALL_TYPE_UPDATE_PROPERTY);
					(*sp) << id_;
					sp->append(mstream->contents(), mstream->size());
					clientEntityCall->post(sp);
				}
			}
		}

		// This property has been updated to add this information to entities that have entered this level, 
		//  but now it may be a little further away, after he came back to re-enter this detaillevel
		// If it is not appropriate to re-update all attributes to his client, we record the change of this attribute.
		// Next time he re-enters, we only need to send all the data that has been changed to his client update.
		for(int8 i=detailLevel; i<=DETAIL_LEVEL_FAR; ++i)
		{
			std::map<ENTITY_ID, Entity*>::iterator iter = witnessEntities_[i].begin();
			for(; iter != witnessEntities_[i].end(); ++iter)
			{
				Entity* entity = iter->second;
				EntityCall* clientEntityCall = entity->clientEntityCall();
				if(clientEntityCall != NULL)
				{
					WitnessInfo* witnessInfo = witnessEntityDetailLevelMap_.find(iter->first)->second;
					if(witnessInfo->detailLevelLog[detailLevel])
					{
						std::vector<uint32>& cddlog = witnessInfo->changeDefDataLogs[detailLevel];
						std::vector<uint32>::iterator fiter = std::find(cddlog.begin(), cddlog.end(), utype);
						if(fiter == cddlog.end())
							witnessInfo->changeDefDataLogs[detailLevel].push_back(utype);
					}
				}

				// Record the amount of data generated by this event
				std::string event_name = this->scriptName();
				event_name += ".";
				event_name += propertyDescription->getName();
				
				g_publicClientEventHistoryStats.add(scriptName(), propertyDescription->getName(), pSendBundle->currMsgLength());
			}
		}
	}
	*/

	// Determine if this attribute still needs to be broadcast to its own client
	if((flags & ENTITY_BROADCAST_OWN_CLIENT_FLAGS) > 0 && clientEntityCall_ != NULL && pWitness_)
	{
		Network::Bundle* pSendBundle = NULL;
		
		Network::Channel* pChannel = pWitness_->pChannel();
		if(!pChannel)
			pSendBundle = Network::Bundle::createPoolObject();
		else
			pSendBundle = pChannel->createSendBundle();
		
		NETWORK_ENTITY_MESSAGE_FORWARD_CLIENT_BEGIN(id(), (*pSendBundle));
		
		ENTITY_MESSAGE_FORWARD_CLIENT_BEGIN(pSendBundle, ClientInterface::onUpdatePropertys, updatePropertys);
		(*pSendBundle) << id();

		if (pScriptModule_->usePropertyDescrAlias())
		{
			(*pSendBundle) << componentPropertyAliasID;
			(*pSendBundle) << propertyDescription->aliasIDAsUint8();
		}
		else
		{
			(*pSendBundle) << componentPropertyUID;
			(*pSendBundle) << propertyDescription->getUType();
		}

		pSendBundle->append(*mstream);
		
		// Record the amount of data generated by this event
		if((flags & ENTITY_BROADCAST_OTHER_CLIENT_FLAGS) <= 0)
		{
			g_privateClientEventHistoryStats.trackEvent(scriptName(), 
				propertyDescription->getName(), 
				pSendBundle->currMsgLength());
		}

		ENTITY_MESSAGE_FORWARD_CLIENT_END(pSendBundle, ClientInterface::onUpdatePropertys, updatePropertys);

		pWitness_->sendToClient(ClientInterface::onUpdatePropertys, pSendBundle);
	}

	MemoryStream::reclaimPoolObject(mstream);
}

//-------------------------------------------------------------------------------------
void Entity::onRemoteMethodCall(Network::Channel* pChannel, MemoryStream& s)
{
	ENTITY_PROPERTY_UID componentPropertyUID = 0;
	s >> componentPropertyUID;

	ENTITY_METHOD_UID utype = 0;
	s >> utype;

	ScriptDefModule* pScriptModule = pScriptModule_;

	PropertyDescription* pComponentPropertyDescription = NULL;
	if (componentPropertyUID > 0)
	{
		pComponentPropertyDescription = pScriptModule_->findCellPropertyDescription(componentPropertyUID);

		if (pComponentPropertyDescription && pComponentPropertyDescription->getDataType()->type() == DATA_TYPE_ENTITY_COMPONENT)
		{
			pScriptModule = static_cast<EntityComponentType*>(pComponentPropertyDescription->getDataType())->pScriptDefModule();
		}
		else
		{
			ERROR_MSG(fmt::format("{2}::onRemoteMethodCall: can't found EntityComponent({3}). utype={0}, methodName=unknown, callerID:{1}.\n"
				, utype, id_, this->scriptName(), (componentPropertyUID)));
		}
	}

	MethodDescription* pMethodDescription = pScriptModule->findCellMethodDescription(utype);

	if (pMethodDescription == NULL)
	{
		ERROR_MSG(fmt::format("{2}::onRemoteMethodCall: can't found {3}method. utype={0}, methodName=unknown, callerID:{1}.\n"
			, utype, id_, this->scriptName(), (pComponentPropertyDescription ? (std::string("component[") + std::string(pScriptModule->getName()) + "] ") : "")));

		return;
	}

	onRemoteMethodCall_(pComponentPropertyDescription, pMethodDescription, id(), s);
}

//-------------------------------------------------------------------------------------
void Entity::onRemoteCallMethodFromClient(Network::Channel* pChannel, ENTITY_ID srcEntityID, MemoryStream& s)
{
	ENTITY_PROPERTY_UID componentPropertyUID = 0;
	s >> componentPropertyUID;

	ENTITY_METHOD_UID utype = 0;
	s >> utype;

	ScriptDefModule* pScriptModule = pScriptModule_;

	PropertyDescription* pComponentPropertyDescription = NULL;
	if (componentPropertyUID > 0)
	{
		pComponentPropertyDescription = pScriptModule_->findCellPropertyDescription(componentPropertyUID);
	}

	if (pComponentPropertyDescription)
	{
		DataType* pDataType = pComponentPropertyDescription->getDataType();
		KBE_ASSERT(pDataType->type() == DATA_TYPE_ENTITY_COMPONENT);

		pScriptModule = static_cast<EntityComponentType*>(pDataType)->pScriptDefModule();
	}

	MethodDescription* pMethodDescription = pScriptModule->findCellMethodDescription(utype);
	if(pMethodDescription)
	{
		if(!pMethodDescription->isExposed())
		{
			ERROR_MSG(fmt::format("{2}::onRemoteCallMethodFromClient: {3}{0} not is exposed, call is illegal! entityID:{1}.\n",
				pMethodDescription->getName(), this->id(), this->scriptName(), (pComponentPropertyDescription ? (std::string(pScriptModule->getName()) + "::") : "")));

			s.done();
			return;
		}
	}
	else
	{
		ERROR_MSG(fmt::format("{2}::onRemoteCallMethodFromClient: can't found {3}method. utype={0}, methodName=unknown, callerID:{1}.\n",
			utype, id_, this->scriptName(), (pComponentPropertyDescription ? (std::string("component[") + std::string(pScriptModule->getName()) + "] ") : "")));

		return;
	}

	onRemoteMethodCall_(pComponentPropertyDescription, pMethodDescription, srcEntityID, s);
}

//-------------------------------------------------------------------------------------
void Entity::onRemoteMethodCall_(PropertyDescription* pComponentPropertyDescription, 
	MethodDescription* pMethodDescription, ENTITY_ID srcEntityID, MemoryStream& s)
{
	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	if (isDestroyed())
	{
		ERROR_MSG(fmt::format("{}::onRemoteMethodCall: {} is destroyed!\n",
			scriptName(), id()));

		s.done();
		return;
	}

	if(pMethodDescription == NULL)
	{
		ERROR_MSG(fmt::format("{1}::onRemoteMethodCall: can't found method, callerID:{0}.\n",
			id_, this->scriptName()));

		return;
	}

	if(g_debugEntity)
	{
		DEBUG_MSG(fmt::format("{3}::onRemoteMethodCall: {0}, {3}::{4}{1}(utype={2}).\n",
			id_, pMethodDescription->getName(), pMethodDescription->getUType(), this->scriptName(),
			(pComponentPropertyDescription ? (std::string(static_cast<EntityComponentType*>(
				pComponentPropertyDescription->getDataType())->pScriptDefModule()->getName()) + "::") : "")));
	}

	PyObject* pyCallObject = this;

	if (pComponentPropertyDescription)
	{
		pyCallObject = PyObject_GetAttrString(this, const_cast<char*>
			(pComponentPropertyDescription->getName()));
	}

	EntityDef::context().currEntityID = srcEntityID;

	PyObject* pyFunc = PyObject_GetAttrString(pyCallObject, const_cast<char*>
						(pMethodDescription->getName()));

	if(pMethodDescription != NULL)
	{
		if(!pMethodDescription->isExposed() && pMethodDescription->getArgSize() == 0)
		{
			pMethodDescription->call(pyFunc, NULL);
		}
		else
		{
			PyObject* pyargs = pMethodDescription->createFromStream(&s);
			if(pyargs)
			{
				pMethodDescription->call(pyFunc, pyargs);
				Py_XDECREF(pyargs);
			}
			else
			{
				SCRIPT_ERROR_CHECK();
				s.done();
			}
		}
	}
	else
	{
		s.done();
	}

	Py_XDECREF(pyFunc);

	if (pyCallObject != static_cast<PyObject*>(this))
		Py_DECREF(pyCallObject);

	SCRIPT_ERROR_CHECK();
}

//-------------------------------------------------------------------------------------
void Entity::addCellDataToStream(COMPONENT_TYPE sendTo, uint32 flags, MemoryStream* mstream, bool useAliasID)
{
	EntityDef::context().currComponentType = g_componentType;

	addPositionAndDirectionToStream(*mstream, useAliasID);
	PyObject* cellData = PyObject_GetAttrString(this, "__dict__");

	ScriptDefModule::PROPERTYDESCRIPTION_MAP& propertyDescrs =
					pScriptModule_->getCellPropertyDescriptions();

	ScriptDefModule::PROPERTYDESCRIPTION_MAP::const_iterator iter = propertyDescrs.begin();

	for(; iter != propertyDescrs.end(); ++iter)
	{
		PropertyDescription* propertyDescription = iter->second;
		if((flags & propertyDescription->getFlags()) > 0)
		{
			// DEBUG_MSG(fmt::format("Entity::addCellDataToStream: {}.\n", propertyDescription->getName()));
			PyObject* pyVal = PyDict_GetItemString(cellData, propertyDescription->getName());

			if(useAliasID && pScriptModule_->usePropertyDescrAlias())
			{
				(*mstream) << (uint8)0;
				(*mstream) << propertyDescription->aliasIDAsUint8();
			}
			else
			{
				(*mstream) << (ENTITY_PROPERTY_UID)0;
				(*mstream) << propertyDescription->getUType();
			}

			if (!propertyDescription->getDataType()->isSameType(pyVal))
			{
				ERROR_MSG(fmt::format("{}::addCellDataToStream: {}({}) not is ({})!\n", this->scriptName(),
					propertyDescription->getName(), (pyVal ? pyVal->ob_type->tp_name : "unknown"), propertyDescription->getDataType()->getName()));

				PyObject* pydefval = propertyDescription->getDataType()->parseDefaultStr("");
				propertyDescription->getDataType()->addToStream(mstream, pydefval);
				Py_DECREF(pydefval);
			}
			else
			{
				propertyDescription->getDataType()->addToStream(mstream, pyVal);
			}

			if (PyErr_Occurred())
 			{	
				PyErr_PrintEx(0);
				DEBUG_MSG(fmt::format("{}::addCellDataToStream: {} error!\n", this->scriptName(),
					propertyDescription->getName()));
			}
		}
	}

	Py_XDECREF(cellData);
	SCRIPT_ERROR_CHECK();
}

//-------------------------------------------------------------------------------------
void Entity::backupCellData()
{
	AUTO_SCOPED_PROFILE("backup");

	if(baseEntityCall_ != NULL)
	{
		// Bundle current cell data together and send it to the base for backup
		Network::Bundle* pBundle = Network::Bundle::createPoolObject();
		(*pBundle).newMessage(BaseappInterface::onBackupEntityCellData);
		(*pBundle) << id_;
		(*pBundle) << isDirty();
		
		if(isDirty())
		{
			MemoryStream* s = MemoryStream::createPoolObject();
			addCellDataToStream(BASEAPP_TYPE, ENTITY_CELL_DATA_FLAGS, s);
			(*pBundle).append(s);
			MemoryStream::reclaimPoolObject(s);
		}
		
		baseEntityCall_->sendCall(pBundle);
	}
	else
	{
		WARNING_MSG(fmt::format("Entity::backupCellData(): {} {} has no base!\n", 
			this->scriptName(), this->id()));
	}

	SCRIPT_ERROR_CHECK();
	
	setDirty(false);
}

//-------------------------------------------------------------------------------------
void Entity::writeToDB(void* data, void* extra1, void* extra2)
{
	CALLBACK_ID* pCallbackID = static_cast<CALLBACK_ID*>(data);
	CALLBACK_ID callbackID = 0;

	if(pCallbackID)
		callbackID = *pCallbackID;

	int8 shouldAutoLoad = -1;
	if (extra1)
		shouldAutoLoad = *static_cast<int8*>(extra1);

	int dbInterfaceIndex = -1;

	if (extra2)
	{
		if (strlen(static_cast<char*>(extra2)) > 0)
		{
			DBInterfaceInfo* pDBInterfaceInfo = g_kbeSrvConfig.dbInterface(static_cast<char*>(extra2));
			if (pDBInterfaceInfo->isPure)
			{
				ERROR_MSG(fmt::format("Entity::writeToDB: dbInterface({}) is a pure database does not support Entity! "
					"kbengine[_defs].xml->dbmgr->databaseInterfaces->*->pure\n",
					static_cast<char*>(extra2)));

				return;
			}

			int fdbInterfaceIndex = pDBInterfaceInfo->index;
			if (fdbInterfaceIndex >= 0)
			{
				dbInterfaceIndex = fdbInterfaceIndex;
			}
			else
			{
				ERROR_MSG(fmt::format("Entity::writeToDB: not found dbInterface({})!\n",
					static_cast<char*>(extra2)));

				return;
			}
		}
	}
	onWriteToDB();
	backupCellData();

	Network::Bundle* pBundle = Network::Bundle::createPoolObject();
	(*pBundle).newMessage(BaseappInterface::onCellWriteToDBCompleted);
	(*pBundle) << this->id();
	(*pBundle) << callbackID;
	(*pBundle) << shouldAutoLoad;
	(*pBundle) << dbInterfaceIndex;

	if(this->baseEntityCall())
	{
		this->baseEntityCall()->sendCall(pBundle);
	}
}

//-------------------------------------------------------------------------------------
void Entity::onWriteToDB()
{
	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	DEBUG_MSG(fmt::format("{}::onWriteToDB(): {}.\n", 
		this->scriptName(), this->id()));

	CALL_ENTITY_AND_COMPONENTS_METHOD(this, SCRIPT_OBJECT_CALL_ARGS0(pyTempObj, const_cast<char*>("onWriteToDB"), false));
}

//-------------------------------------------------------------------------------------
bool Entity::bufferOrExeCallback(const char * funcName, PyObject * funcArgs, bool notFoundIsOK)
{
	bool canBuffer = _scriptCallbacksBufferCount > 0;

	PyObject* pyCallable = PyObject_GetAttrString(this, const_cast<char*>(funcName));

	if (pyCallable == NULL)
	{
		if (!notFoundIsOK)
		{
			ERROR_MSG(fmt::format("{}::bufferOrExeCallback({}): method({}) not found!\n",
				scriptName(), id(), funcName));
		}

		if (funcArgs)
			Py_DECREF(funcArgs);

		PyErr_Clear();
		return false;
	}

	if (canBuffer)
	{
		BufferedScriptCall* pBufferedScriptCall = new BufferedScriptCall();
		pBufferedScriptCall->entityPtr = this;
		pBufferedScriptCall->pyFuncArgs = funcArgs;
		pBufferedScriptCall->pyCallable = pyCallable;
		pBufferedScriptCall->funcName = funcName;
		_scriptCallbacksBuffer.push_back(pBufferedScriptCall);
		++_scriptCallbacksBufferNum;
	}
	else
	{
		Py_INCREF(this);
		PyObject* pyResult = PyObject_CallObject(pyCallable, funcArgs);

		Py_DECREF(pyCallable);

		if (pyResult)
		{
			Py_DECREF(pyResult);
		}
		else
		{
			PyErr_PrintEx(0);
		}

		// Notify all components
		ScriptDefModule::COMPONENTDESCRIPTION_MAP& componentDescrs = pScriptModule_->getComponentDescrs();
		ScriptDefModule::COMPONENTDESCRIPTION_MAP::iterator comps_iter = componentDescrs.begin();
		for (; comps_iter != componentDescrs.end(); ++comps_iter)
		{
			if (!comps_iter->second->hasCell())
				continue;

			PyObject* pyTempObj = PyObject_GetAttrString(this, comps_iter->first.c_str());
			if (pyTempObj)
			{
				PyObject* pyCompCallable = PyObject_GetAttrString(pyTempObj, const_cast<char*>(funcName));

				if (pyCompCallable == NULL)
				{
					PyErr_Clear();
				}
				else
				{
					PyObject* pyCompResult = PyObject_CallObject(pyCompCallable, funcArgs);

					Py_DECREF(pyCompCallable);

					if (pyCompResult)
					{
						Py_DECREF(pyCompResult);
					}
					else
					{
						PyErr_PrintEx(0);
					}
				}

				Py_DECREF(pyTempObj);
			}
			else
			{
				SCRIPT_ERROR_CHECK();
			}
		}

		if (funcArgs)
			Py_DECREF(funcArgs);

		Py_DECREF(this);
	}

	return true;
}

//-------------------------------------------------------------------------------------
void Entity::bufferCallback(bool enable)
{
	if (enable)
	{
		++_scriptCallbacksBufferCount;
	}
	else
	{
		if (_scriptCallbacksBufferCount - 1 == 0)
		{
			// Since the cache is about to be canceled, all callbacks are executed, but care must be taken to generate a callback cache during execution
			// Then need to join the queue
			while (_scriptCallbacksBufferNum > 0)
			{
				BufferedScriptCall* pBufferedScriptCall = (*_scriptCallbacksBuffer.begin());
				_scriptCallbacksBuffer.pop_front();
				--_scriptCallbacksBufferNum;

				PyObject* pyResult = PyObject_CallObject(pBufferedScriptCall->pyCallable, pBufferedScriptCall->pyFuncArgs);

				if (pyResult)
				{
					Py_DECREF(pyResult);
				}
				else
				{
					PyErr_PrintEx(0);
				}

				// Notify all components
				ScriptDefModule::COMPONENTDESCRIPTION_MAP& componentDescrs = pBufferedScriptCall->entityPtr->pScriptModule()->getComponentDescrs();
				ScriptDefModule::COMPONENTDESCRIPTION_MAP::iterator comps_iter = componentDescrs.begin();
				for (; comps_iter != componentDescrs.end(); ++comps_iter)
				{
					if (!comps_iter->second->hasCell())
						continue;

					PyObject* pyTempObj = PyObject_GetAttrString(pBufferedScriptCall->entityPtr.get(), comps_iter->first.c_str());
					if (pyTempObj)
					{
						PyObject* pyCompCallable = PyObject_GetAttrString(pyTempObj, const_cast<char*>(pBufferedScriptCall->funcName));

						if (pyCompCallable == NULL)
						{
							PyErr_Clear();
						}
						else
						{
							PyObject* pyCompResult = PyObject_CallObject(pyCompCallable, pBufferedScriptCall->pyFuncArgs);

							Py_DECREF(pyCompCallable);

							if (pyCompResult)
							{
								Py_DECREF(pyCompResult);
							}
							else
							{
								PyErr_PrintEx(0);
							}
						}

						Py_DECREF(pyTempObj);
					}
					else
					{
						SCRIPT_ERROR_CHECK();
					}
				}

				Py_DECREF(pBufferedScriptCall->pyCallable);
				if (pBufferedScriptCall->pyFuncArgs)
					Py_DECREF(pBufferedScriptCall->pyFuncArgs);

				delete pBufferedScriptCall;
			}
		}

		// Finally, decrement to prevent cache callback from being requested during the callback
		--_scriptCallbacksBufferCount;
		KBE_ASSERT(_scriptCallbacksBufferCount >= 0);
	}
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyIsReal()
{
	return PyBool_FromLong(isReal());
}

//-------------------------------------------------------------------------------------
void Entity::addWitnessed(Entity* entity)
{
	if(Cellapp::getSingleton().pWitnessedTimeoutHandler())
		Cellapp::getSingleton().pWitnessedTimeoutHandler()->delWitnessed(this);

	witnesses_.push_back(entity->id());
	++witnesses_count_;

	/*
	int8 detailLevel = pScriptModule_->getDetailLevel().getLevelByRange(range);
	WitnessInfo* info = new WitnessInfo(detailLevel, entity, range);
	ENTITY_ID id = entity->id();

	DEBUG_MSG("Entity[%s:%ld]::onWitnessed:%s %ld enter detailLevel %d. range=%f.\n", scriptName(), id_, 
			entity->scriptName(), id, detailLevel, range);

#ifdef _DEBUG
	WITNESSENTITY_DETAILLEVEL_MAP::iterator iter = witnessEntityDetailLevelMap_.find(id);
	if(iter != witnessEntityDetailLevelMap_.end())
		ERROR_MSG("Entity::onWitnessed: %s %ld is exist.\n", entity->scriptName(), id);
#endif
	
	witnessEntityDetailLevelMap_[id] = info;
	witnessEntities_[detailLevel][id] = entity;
	onEntityInitDetailLevel(entity, detailLevel);
	*/

	if(witnesses_count_ == 1)
	{
		SCOPED_PROFILE(SCRIPTCALL_PROFILE);

		bufferOrExeCallback(const_cast<char*>("onWitnessed"),
			Py_BuildValue(const_cast<char*>("(O)"), PyBool_FromLong(1)));
	}
}

//-------------------------------------------------------------------------------------
void Entity::delWitnessed(Entity* entity)
{
	KBE_ASSERT(witnesses_count_ > 0);

	witnesses_.remove(entity->id());
	--witnesses_count_;

	if (controlledBy_ != NULL && entity->id() == controlledBy_->id())
	{
		if (clientEntityCall_ && clientEntityCall_->getChannel())
			setControlledBy(baseEntityCall_);
		else
			setControlledBy(NULL);

		SCOPED_PROFILE(SCRIPTCALL_PROFILE);
		CALL_ENTITY_AND_COMPONENTS_METHOD(this, SCRIPT_OBJECT_CALL_ARGS1(pyTempObj, const_cast<char*>("onLoseControlledBy"),
			const_cast<char*>("i"), entity->id(), false));
	}

	// Delayed execution
	// onDelWitnessed();

	if(Cellapp::getSingleton().pWitnessedTimeoutHandler())
		Cellapp::getSingleton().pWitnessedTimeoutHandler()->addWitnessed(this);
}

//-------------------------------------------------------------------------------------
void Entity::onDelWitnessed()
{
	if(witnesses_count_ == 0)
	{
		SCOPED_PROFILE(SCRIPTCALL_PROFILE);

		bufferOrExeCallback(const_cast<char*>("onWitnessed"),
			Py_BuildValue(const_cast<char*>("(O)"), PyBool_FromLong(0)));
	}
}

//-------------------------------------------------------------------------------------
bool Entity::entityInWitnessed(ENTITY_ID entityID)
{
	std::list<ENTITY_ID>::iterator it = witnesses_.begin();
	for (; it != witnesses_.end(); ++it)
	{
		if (*it == entityID)
			return true;
	}

	return false;
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyIsWitnessed()
{
	return PyBool_FromLong(isWitnessed());
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyHasWitness()
{
	return PyBool_FromLong(hasWitness());
}

//-------------------------------------------------------------------------------------
void Entity::restoreProximitys()
{
	if(this->pWitness())
		this->pWitness()->installViewTrigger();

	Controllers::CONTROLLERS_MAP& objects = pControllers_->objects();
	Controllers::CONTROLLERS_MAP::iterator iter = objects.begin();
	for(; iter != objects.end(); ++iter)
	{
		if(iter->second->type() == Controller::CONTROLLER_TYPE_PROXIMITY)
		{
			ProximityController* pProximityController = static_cast<ProximityController*>(iter->second.get());
			pProximityController->reinstall(static_cast<CoordinateNode*>(this->pEntityCoordinateNode()));
		}
	}
}

//-------------------------------------------------------------------------------------
uint32 Entity::addProximity(float range_xz, float range_y, int32 userarg)
{
	if(range_xz <= 0.0f || (CoordinateSystem::hasY && range_y <= 0.0f))
	{
		ERROR_MSG(fmt::format("Entity::addProximity: range(xz={}, y={}) <= 0.0f! entity[{}:{}]\n", 
			range_xz, range_y, scriptName(), id()));

		return 0;
	}
	
	if(this->pEntityCoordinateNode() == NULL || this->pEntityCoordinateNode()->pCoordinateSystem() == NULL)
	{
		ERROR_MSG(fmt::format("Entity::addProximity: {}({}) not in world!\n", 
			scriptName(), id()));

		return 0;
	}

	// Put a trap in space
	KBEShared_ptr<Controller> p( new ProximityController(this, range_xz, range_y, userarg, pControllers_->freeID()) );

	bool ret = pControllers_->add(p);
	KBE_ASSERT(ret);
	return p->id();
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyAddProximity(float range_xz, float range_y, int32 userarg)
{
	if(!isReal())
	{
		PyErr_Format(PyExc_AssertionError, "%s::addProximity: not is real entity(%d).", 
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	if(this->isDestroyed())
	{
		PyErr_Format(PyExc_AssertionError, "%s::addProximity: %d is destroyed!\n",		
			scriptName(), id());		
		PyErr_PrintEx(0);
		return 0;
	}

	return PyLong_FromLong(addProximity(range_xz, range_y, userarg));
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyClientEntity(ENTITY_ID entityID)
{
	if(!isReal())
	{
		PyErr_Format(PyExc_AssertionError, "%s::clientEntity: not is real entity(%d).", 
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	if(this->isDestroyed())
	{
		PyErr_Format(PyExc_AssertionError, "%s::clientEntity: %d is destroyed!\n",		
			scriptName(), id());		
		PyErr_PrintEx(0);
		return 0;
	}

	if (entityID == id())
	{
		PyErr_Format(PyExc_AssertionError, "%s::clientEntity: call your own method using entity.client! id=%d\n",
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	return new ClientEntity(id(), entityID);
}

//-------------------------------------------------------------------------------------
void Entity::cancelController(uint32 id)
{
	if(this->isDestroyed())
	{
		return;
	}

	if(!pControllers_->remove(id))
	{
		ERROR_MSG(fmt::format("{}::cancel: {} not found {}.\n", 
			this->scriptName(), this->id(), id));
	}
}

//-------------------------------------------------------------------------------------
PyObject* Entity::__py_pyCancelController(PyObject* self, PyObject* args)
{
	uint16 currargsSize = PyTuple_Size(args);
	Entity* pobj = static_cast<Entity*>(self);
	
	if(!pobj->isReal())
	{
		PyErr_Format(PyExc_AssertionError, "%s::clientEntity: not is real entity(%d).", 
			pobj->scriptName(), pobj->id());
		PyErr_PrintEx(0);
		return 0;
	}

	uint32 id = 0;
	PyObject* pyargobj = NULL;

	if(currargsSize != 1)
	{
		PyErr_Format(PyExc_AssertionError, "%s::cancel: args require 1 args(controllerID|int or \"Movement\"|str), gived %d! is script[%s].\n",								
			pobj->scriptName(), currargsSize);														
																																
		PyErr_PrintEx(0);																										
		return 0;																								
	}

	if(PyArg_ParseTuple(args, "O", &pyargobj) == -1)
	{
		PyErr_Format(PyExc_TypeError, "%s::cancel: args(controllerID|int or \"Movement\"|str) error!", pobj->scriptName());
		PyErr_PrintEx(0);
		return 0;
	}
	
	if(pyargobj == NULL)
	{
		PyErr_Format(PyExc_TypeError, "%s::cancel: args(controllerID|int or \"Movement\"|str) error!", pobj->scriptName());
		PyErr_PrintEx(0);
		return 0;
	}

	if(PyUnicode_Check(pyargobj))
	{
		wchar_t* PyUnicode_AsWideCharStringRet0 = PyUnicode_AsWideCharString(pyargobj, NULL);
		char* s = strutil::wchar2char(PyUnicode_AsWideCharStringRet0);
		PyMem_Free(PyUnicode_AsWideCharStringRet0);
		
		if(strcmp(s, "Movement") == 0)
		{
			pobj->stopMove();
		}
		else
		{
			PyErr_Format(PyExc_TypeError, "%s::cancel: args not is \"Movement\"!", pobj->scriptName());
			PyErr_PrintEx(0);
			free(s);
			return 0;
		}

		free(s);

		S_Return;
	}
	else
	{
		if(!PyLong_Check(pyargobj))
		{
			PyErr_Format(PyExc_TypeError, "%s::cancel: args(controllerID|int) error!", pobj->scriptName());
			PyErr_PrintEx(0);
			return 0;
		}

		id = PyLong_AsLong(pyargobj);
	}

	// As long as it belongs to the scope of the move controller, it should call stopMove()
	//  to avoid the existence of multiple ways to cause disagreement on the call
	if ((pobj->pMoveController_ && pobj->pMoveController_->id() == id) || 
		(pobj->pTurnController_ && pobj->pTurnController_->id() == id))
	{
		pobj->stopMove();
	}
	else
	{
		pobj->cancelController(id);
	}

	S_Return;
}

//-------------------------------------------------------------------------------------
void Entity::onEnterTrap(Entity* entity, float range_xz, float range_y, uint32 controllerID, int32 userarg)
{
	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onEnterTrap"), 
		Py_BuildValue(const_cast<char*>("(OffIi)"), entity, range_xz, range_y, controllerID, userarg));
}

//-------------------------------------------------------------------------------------
void Entity::onLeaveTrap(Entity* entity, float range_xz, float range_y, uint32 controllerID, int32 userarg)
{
	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onLeaveTrap"), 
		Py_BuildValue(const_cast<char*>("(OffIi)"), entity, range_xz, range_y, controllerID, userarg));
}

//-------------------------------------------------------------------------------------
void Entity::onLeaveTrapID(ENTITY_ID entityID, float range_xz, float range_y, uint32 controllerID, int32 userarg)
{
	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onLeaveTrapID"), 
		Py_BuildValue(const_cast<char*>("(kffIi)"), entityID, range_xz, range_y, controllerID, userarg));
}

//-------------------------------------------------------------------------------------
void Entity::onEnteredView(Entity* entity)
{
	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onEnteredView"),
		Py_BuildValue(const_cast<char*>("(O)"), entity));
}

//-------------------------------------------------------------------------------------
int Entity::pySetPosition(PyObject *value)
{
	if(isDestroyed())	
	{
		PyErr_Format(PyExc_AssertionError, "%s: %d is destroyed!\n",		
			scriptName(), id());		
		PyErr_PrintEx(0);
		return -1;																				
	}

	if(!script::ScriptVector3::check(value))
		return -1;

	Position3D pos;
	script::ScriptVector3::convertPyObjectToVector3(pos, value);
	position(pos);

	static ENTITY_PROPERTY_UID posuid = 0;
	if(posuid == 0)
	{
		posuid = ENTITY_BASE_PROPERTY_UTYPE_POSITION_XYZ;
		Network::FixedMessages::MSGInfo* msgInfo =
					Network::FixedMessages::getSingleton().isFixed("Property::position");

		if(msgInfo != NULL)
			posuid = msgInfo->msgid;
	}

	static PropertyDescription positionDescription(posuid, "VECTOR3", "position", ED_FLAG_ALL_CLIENTS, true, DataTypes::getDataType("VECTOR3"), false, "", 0, "", DETAIL_LEVEL_FAR);
	if(pScriptModule_->usePropertyDescrAlias() && positionDescription.aliasID() == -1)
		positionDescription.aliasID(ENTITY_BASE_PROPERTY_ALIASID_POSITION_XYZ);

	onDefDataChanged(NULL, &positionDescription, value);
	return 0;
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyGetPosition()
{
	Py_INCREF(pPyPosition_);
	return pPyPosition_;
}

//-------------------------------------------------------------------------------------
void Entity::setPosition_XZ_int(Network::Channel* pChannel, int32 x, int32 z)
{
	setPosition_XZ_float(pChannel, float(x), float(z));
}

//-------------------------------------------------------------------------------------
void Entity::setPosition_XYZ_int(Network::Channel* pChannel, int32 x, int32 y, int32 z)
{
	setPosition_XYZ_float(pChannel, float(x), float(y), float(z));
}

//-------------------------------------------------------------------------------------
void Entity::setPosition_XZ_float(Network::Channel* pChannel, float x, float z)
{
	Position3D& pos = position();
	if(almostEqual(x, pos.x) && almostEqual(z, pos.z))
		return;

	pos.x = x;
	pos.z = z;
	onPositionChanged();
}

//-------------------------------------------------------------------------------------
void Entity::setPosition_XYZ_float(Network::Channel* pChannel, float x, float y, float z)
{
	Position3D& pos = position();
	if(almostEqual(x, pos.x) && almostEqual(y, pos.y) && almostEqual(z, pos.z))
		return;

	pos.x = x;
	pos.x = y;
	pos.z = z;
	onPositionChanged();
}

//-------------------------------------------------------------------------------------
int Entity::pySetDirection(PyObject *value)
{
	if(isDestroyed())	
	{
		PyErr_Format(PyExc_AssertionError, "%s: %d is destroyed!\n",		
			scriptName(), id());		
		PyErr_PrintEx(0);
		return -1;																				
	}

	if(PySequence_Check(value) <= 0)
	{
		PyErr_Format(PyExc_TypeError, "args of direction is must a sequence.");
		PyErr_PrintEx(0);
		return -1;
	}

	Py_ssize_t size = PySequence_Size(value);
	if(size != 3)
	{
		PyErr_Format(PyExc_TypeError, "len(direction) != 3. can't set.");
		PyErr_PrintEx(0);
		return -1;
	}

	Direction3D& dir = direction();
	PyObject* pyItem = PySequence_GetItem(value, 0);

	if(!PyFloat_Check(pyItem))
	{
		PyErr_Format(PyExc_TypeError, "args of direction is must a float(curr=%s).", pyItem->ob_type->tp_name);
		PyErr_PrintEx(0);
		Py_DECREF(pyItem);
		return -1;
	}

	dir.roll(float(PyFloat_AsDouble(pyItem)));
	Py_DECREF(pyItem);

	pyItem = PySequence_GetItem(value, 1);

	if(!PyFloat_Check(pyItem))
	{
		PyErr_Format(PyExc_TypeError, "args of direction is must a float(curr=%s).", pyItem->ob_type->tp_name);
		PyErr_PrintEx(0);
		Py_DECREF(pyItem);
		return -1;
	}

	dir.pitch(float(PyFloat_AsDouble(pyItem)));
	Py_DECREF(pyItem);

	pyItem = PySequence_GetItem(value, 2);

	if(!PyFloat_Check(pyItem))
	{
		PyErr_Format(PyExc_TypeError, "args of direction is must a float(curr=%s).", pyItem->ob_type->tp_name);
		PyErr_PrintEx(0);
		Py_DECREF(pyItem);
		return -1;
	}

	dir.yaw(float(PyFloat_AsDouble(pyItem)));
	Py_DECREF(pyItem);

	static ENTITY_PROPERTY_UID diruid = 0;
	if(diruid == 0)
	{
		diruid = ENTITY_BASE_PROPERTY_UTYPE_DIRECTION_ROLL_PITCH_YAW;
		Network::FixedMessages::MSGInfo* msgInfo = Network::FixedMessages::getSingleton().isFixed("Property::direction");
		if(msgInfo != NULL)	
			diruid = msgInfo->msgid;
	}

	static PropertyDescription directionDescription(diruid, "VECTOR3", "direction", ED_FLAG_ALL_CLIENTS, true, DataTypes::getDataType("VECTOR3"), false, "", 0, "", DETAIL_LEVEL_FAR);
	if(pScriptModule_->usePropertyDescrAlias() && directionDescription.aliasID() == -1)
		directionDescription.aliasID(ENTITY_BASE_PROPERTY_ALIASID_DIRECTION_ROLL_PITCH_YAW);

	onDefDataChanged(NULL, &directionDescription, value);

	return 0;
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyGetDirection()
{
	Py_INCREF(pPyDirection_);
	return pPyDirection_;
}

//-------------------------------------------------------------------------------------
void Entity::setPositionAndDirection(const Position3D& pos, const Direction3D& dir)
{
	if(this->isDestroyed())
		return;

	position(pos);
	direction(dir);
}

//-------------------------------------------------------------------------------------
void Entity::onPyPositionChanged()
{
	if(this->isDestroyed())
		return;

	static ENTITY_PROPERTY_UID posuid = 0;
	if(posuid == 0)
	{
		posuid = ENTITY_BASE_PROPERTY_UTYPE_POSITION_XYZ;
		Network::FixedMessages::MSGInfo* msgInfo =
					Network::FixedMessages::getSingleton().isFixed("Property::position");

		if(msgInfo != NULL)
			posuid = msgInfo->msgid;
	}

	static PropertyDescription positionDescription(posuid, "VECTOR3", "position", ED_FLAG_ALL_CLIENTS, true, DataTypes::getDataType("VECTOR3"), false, "", 0, "", DETAIL_LEVEL_FAR);
	if(pScriptModule_->usePropertyDescrAlias() && positionDescription.aliasID() == -1)
		positionDescription.aliasID(ENTITY_BASE_PROPERTY_ALIASID_POSITION_XYZ);

	onDefDataChanged(NULL, &positionDescription, pPyPosition_);

	if (this->pEntityCoordinateNode())
	{
		Entity::bufferCallback(true);
		this->pEntityCoordinateNode()->update();
		Entity::bufferCallback(false);
	}

	updateLastPos();
}

//-------------------------------------------------------------------------------------
void Entity::onPositionChanged()
{
	if(this->isDestroyed())
		return;

	posChangedTime_ = g_kbetime;

	if (this->pEntityCoordinateNode())
	{
		Entity::bufferCallback(true);
		this->pEntityCoordinateNode()->update();
		Entity::bufferCallback(false);
	}

	updateLastPos();
}

//-------------------------------------------------------------------------------------
void Entity::updateLastPos()
{
	lastpos_ = this->position();
}

//-------------------------------------------------------------------------------------
void Entity::onPyDirectionChanged()
{
	if(this->isDestroyed())
		return;

	// onDirectionChanged();
	static ENTITY_PROPERTY_UID diruid = 0;
	if(diruid == 0)
	{
		diruid = ENTITY_BASE_PROPERTY_UTYPE_DIRECTION_ROLL_PITCH_YAW;
		Network::FixedMessages::MSGInfo* msgInfo = Network::FixedMessages::getSingleton().isFixed("Property::direction");
		if(msgInfo != NULL)	
			diruid = msgInfo->msgid;
	}

	static PropertyDescription directionDescription(diruid, "VECTOR3", "direction", ED_FLAG_ALL_CLIENTS, true, DataTypes::getDataType("VECTOR3"), false, "", 0, "", DETAIL_LEVEL_FAR);
	if(pScriptModule_->usePropertyDescrAlias() && directionDescription.aliasID() == -1)
		directionDescription.aliasID(ENTITY_BASE_PROPERTY_ALIASID_DIRECTION_ROLL_PITCH_YAW);

	onDefDataChanged(NULL, &directionDescription, pPyDirection_);
}

//-------------------------------------------------------------------------------------
void Entity::onDirectionChanged()
{
	if(this->isDestroyed())
		return;

	dirChangedTime_ = g_kbetime;
}

//-------------------------------------------------------------------------------------
void Entity::setWitness(Witness* pWitness)
{
	KBE_ASSERT(this->baseEntityCall() != NULL && !this->hasWitness());
	pWitness_ = pWitness;
	pWitness_->attach(this);
}

//-------------------------------------------------------------------------------------
void Entity::onGetWitnessFromBase(Network::Channel* pChannel)
{
	onGetWitness(true);
}

//-------------------------------------------------------------------------------------
void Entity::onGetWitness(bool fromBase)
{
	KBE_ASSERT(this->baseEntityCall() != NULL);

	if(fromBase)
	{
		// The giveClientTo function of the proxy or reloginBaseapp.
		// If an entity has just created a cell and bound control to the entity, there must be no clientEntityCall.
		if(clientEntityCall() == NULL)
		{
			PyObject* clientMB = PyObject_GetAttrString(baseEntityCall(), "client");
			KBE_ASSERT(clientMB != Py_None);

			EntityCall* client = static_cast<EntityCall*>(clientMB);	
			// Py_INCREF(clientEntityCall); There is no need to add a reference here because a new object will be created each time
			clientEntityCall(client);
		}

		if(pWitness_ == NULL)
		{
			setWitness(Witness::createPoolObject());
		}
		else
		{
			/*
				Rebind, usually the client re-login or reconnect or an account kicks another client out to log in to same account,
				and the Entity is still in memory with a bound witness
				(this situation may also be the server has not yet detected client disconnection)

				In this case we still need to do something to ensure the correctness of the client, such as sending an enterWorld
			*/
			pWitness_->onAttach(this);

			// The entities in the View also need to be reset and resynchronized to the client
			pWitness_->resetViewEntities();
		}
	}

	// Prevent this entity from being destroyed in some script callbacks, here's a reference to yourself
	Py_INCREF(this);

	Space* space = Spaces::findSpace(this->spaceID());
	if(space && space->isGood())
	{
		space->onEntityAttachWitness(this);
	}

	// Finally, set controlledBy to own base
	controlledBy(baseEntityCall());
	
	{
		SCOPED_PROFILE(SCRIPTCALL_PROFILE);
		CALL_ENTITY_AND_COMPONENTS_METHOD(this, SCRIPT_OBJECT_CALL_ARGS0(pyTempObj, const_cast<char*>("onGetWitness"), false));
	}
	
	// If a entity already had a cell giveToClient, then you need to update the latest client property values to the client
	if(fromBase)
	{
		Network::Bundle* pSendBundle = Network::Bundle::createPoolObject();
		NETWORK_ENTITY_MESSAGE_FORWARD_CLIENT_BEGIN(id(), (*pSendBundle));
		
		ENTITY_MESSAGE_FORWARD_CLIENT_BEGIN(pSendBundle, ClientInterface::onUpdatePropertys, updatePropertys);
		MemoryStream* s1 = MemoryStream::createPoolObject();
		(*pSendBundle) << id();
		
		ENTITY_PROPERTY_UID spaceuid = ENTITY_BASE_PROPERTY_UTYPE_SPACEID;

		Network::FixedMessages::MSGInfo* msgInfo = 
			Network::FixedMessages::getSingleton().isFixed("Property::spaceID");

		if(msgInfo != NULL)
			spaceuid = msgInfo->msgid;
		
		if(pScriptModule()->usePropertyDescrAlias())
		{
			uint8 aliasID = ENTITY_BASE_PROPERTY_ALIASID_SPACEID;
			(*s1) << (uint8)0 << aliasID << this->spaceID();
		}
		else
		{
			(*s1) << (ENTITY_PROPERTY_UID)0 << spaceuid << this->spaceID();
		}

		addClientDataToStream(s1);
		(*pSendBundle).append(*s1);
		MemoryStream::reclaimPoolObject(s1);
		ENTITY_MESSAGE_FORWARD_CLIENT_END(pSendBundle, ClientInterface::onUpdatePropertys, updatePropertys);
		
		clientEntityCall()->sendCall(pSendBundle);
	}

	Py_DECREF(this);
}

//-------------------------------------------------------------------------------------
void Entity::onLoseWitness(Network::Channel* pChannel)
{
	//INFO_MSG(fmt::format("{}::onLoseWitness: {}.\n", 
	//	this->scriptName(), this->id()));

	KBE_ASSERT(this->clientEntityCall() != NULL && this->hasWitness());

	clientEntityCall()->addr(Network::Address::NONE);
	Py_DECREF(clientEntityCall());
	clientEntityCall(NULL);

	pWitness_->detach(this);
	Witness::reclaimPoolObject(pWitness_);
	pWitness_ = NULL;

	SCOPED_PROFILE(SCRIPTCALL_PROFILE);
	CALL_ENTITY_AND_COMPONENTS_METHOD(this, SCRIPT_OBJECT_CALL_ARGS0(pyTempObj, const_cast<char*>("onLoseWitness"), false));
}

//-------------------------------------------------------------------------------------
int Entity::pySetLayer(PyObject *value)
{
	if(isDestroyed())	
	{
		PyErr_Format(PyExc_AssertionError, "%s: %d is destroyed!\n",		
			scriptName(), id());		
		PyErr_PrintEx(0);
		return 0;																				
	}

	if(!PyLong_Check(value))
	{
		PyErr_Format(PyExc_AssertionError, "%s: %d set layer value is not int!\n",		
			scriptName(), id());		
		PyErr_PrintEx(0);
		return 0;	
	}

	layer_ = (int8)PyLong_AsLong(value);
	return 0;
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyGetLayer()
{
	return PyLong_FromLong(layer_);
}

//-------------------------------------------------------------------------------------
int Entity::pySetVolatileinfo(PyObject *value)
{
	if (isDestroyed())
	{
		PyErr_Format(PyExc_AssertionError, "%s: %d is destroyed!\n",
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}


	if (!PySequence_Check(value))
	{
		PyErr_Format(PyExc_AssertionError, "%s: %d set volatileInfo value is not tuple!\n",
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	if (PySequence_Size(value) != 4)
	{
		PyErr_Format(PyExc_AssertionError, "%s: %d set volatileInfo value is not tuple(position, yaw, pitch, roll)!\n",
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	if (pCustomVolatileinfo_ == NULL)
		pCustomVolatileinfo_ = new VolatileInfo();

	PyObject* pyPos = PySequence_GetItem(value, 0);
	PyObject* pyYaw = PySequence_GetItem(value, 1);
	PyObject* pyPitch = PySequence_GetItem(value, 2);
	PyObject* pyRoll = PySequence_GetItem(value, 3);

	pCustomVolatileinfo_->position(float(PyFloat_AsDouble(pyPos)));
	pCustomVolatileinfo_->yaw(float(PyFloat_AsDouble(pyYaw)));
	pCustomVolatileinfo_->pitch(float(PyFloat_AsDouble(pyPitch)));
	pCustomVolatileinfo_->roll(float(PyFloat_AsDouble(pyRoll)));

	Py_DECREF(pyPos);
	Py_DECREF(pyYaw);
	Py_DECREF(pyPitch);
	Py_DECREF(pyRoll);

	return 0;
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyGetVolatileinfo()
{
	if (pCustomVolatileinfo_ == NULL)
		pCustomVolatileinfo_ = new VolatileInfo();

	Py_INCREF(pCustomVolatileinfo_);
	return pCustomVolatileinfo_;
}

//-------------------------------------------------------------------------------------
bool Entity::checkMoveForTopSpeed(const Position3D& position)
{
	Position3D movment = position - this->position();
	bool move = true;
	
	// Check to make sure movement obeys speed limit
	if(topSpeedY_ > 0.01f && movment.y > topSpeedY_)
	{
		move = false;
	}

	if(move && topSpeed_ > 0.01f)
	{
		movment.y = 0.f;
		
		if(movment.length() > topSpeed_)
			move = false;
	}

	return move;
}

//-------------------------------------------------------------------------------------
void Entity::onUpdateDataFromClient(KBEngine::MemoryStream& s)
{
	if(spaceID_ == 0)
	{
		s.done();
		return;
	}

	Position3D pos;
	Direction3D dir;
	uint8 isOnGround = 0;
	float yaw, pitch, roll;
	SPACE_ID currSpace;

	s >> pos.x >> pos.y >> pos.z >> roll >> pitch >> yaw >> isOnGround >> currSpace;
	isOnGround_ = isOnGround > 0;

	if(spaceID_ != currSpace)
	{
		s.done();
		return;
	}

	dir.yaw(yaw);
	dir.pitch(pitch);
	dir.roll(roll);
	this->direction(dir);

	if(checkMoveForTopSpeed(pos))
	{
		this->position(pos);
	}
	else
	{
		if (this->pWitness() == NULL && this->controlledBy_ == NULL)
			return;

		Position3D currpos = this->position();
		Position3D movment = pos - currpos;
		float ydist = fabs(movment.y);
		movment.y = 0.f;

		DEBUG_MSG(fmt::format("{}::onUpdateDataFromClient: {} position[({},{},{}) -> ({},{},{}), (xzDist={})>(topSpeed={}) || (yDist={})>(topSpeedY={})] invalid. reset client!\n", 
			this->scriptName(), this->id(),
			this->position().x, this->position().y, this->position().z,
			pos.x, pos.y, pos.z,
			movment.length(), topSpeed_,
			ydist, topSpeedY_));
		
		// this->position(currpos);

		// If I have been controlled, then the source of the data is the controller's client.
		// So we need to notify also the source client, not just its own client.
		Witness* pW = NULL;
		KBEngine::ENTITY_ID targetID = 0;

		if (controlledBy_ != NULL)
		{
			targetID = controlledBy_->id();
			Entity* entity = Cellapp::getSingleton().findEntity(targetID);
			
			if(entity->isReal())
				pW = entity->pWitness();
		}
		else
		{
			targetID = id();
			
			if(isReal())
				pW = this->pWitness();
		}
		
		// When teleporting across processes, in extreme situations (ghost) Witness state may be None
		if(pW)
		{
			// Notify reset
			Network::Bundle* pSendBundle = Network::Bundle::createPoolObject();
			NETWORK_ENTITY_MESSAGE_FORWARD_CLIENT_BEGIN(targetID, (*pSendBundle));
			
			ENTITY_MESSAGE_FORWARD_CLIENT_BEGIN(pSendBundle, ClientInterface::onSetEntityPosAndDir, setEntityPosAndDir);

			(*pSendBundle) << id();
			(*pSendBundle) << currpos.x << currpos.y << currpos.z;
			(*pSendBundle) << direction().roll() << direction().pitch() << direction().yaw();

			ENTITY_MESSAGE_FORWARD_CLIENT_END(pSendBundle, ClientInterface::onSetEntityPosAndDir, setEntityPosAndDir);
			pW->sendToClient(ClientInterface::onSetEntityPosAndDir, pSendBundle);
		}
	}
}

//-------------------------------------------------------------------------------------
int32 Entity::setViewRadius(float radius, float hyst)
{
	if(pWitness_)
	{
		pWitness_->setViewRadius(radius, hyst);
		return 1;
	}

	PyErr_Format(PyExc_AssertionError, "%s::setViewRadius: did not get witness.", scriptName());
	PyErr_PrintEx(0);
	return -1;
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pySetViewRadius(float radius, float hyst)
{
	if(!isReal())
	{
		PyErr_Format(PyExc_AssertionError, "%s::setViewRadius: not is real entity(%d).", 
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	return PyLong_FromLong(setViewRadius(radius, hyst));
}

//-------------------------------------------------------------------------------------
float Entity::getViewRadius(void) const
{
	if(pWitness_)
		return pWitness_->viewRadius();
		
	return 0.0; 
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyGetViewRadius()
{
	return PyFloat_FromDouble(getViewRadius());
}

//-------------------------------------------------------------------------------------
float Entity::getViewHystArea(void) const
{
	if(pWitness_)
		return pWitness_->viewHysteresisArea();
		
	return 0.0; 
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyGetViewHystArea()
{
	return PyFloat_FromDouble(getViewHystArea());
}

//-------------------------------------------------------------------------------------
bool Entity::stopMove()
{
	bool done = false;
	
	if(pMoveController_)
	{
		cancelController(pMoveController_->id());
		pMoveController_->destroy();
		pMoveController_.reset();
		done = true;
	}

	if(pTurnController_)
	{
		cancelController(pTurnController_->id());
		pTurnController_->destroy();
		pTurnController_.reset();
		done = true;
	}

	return done;
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pycanNavigate()
{
	if(canNavigate())
	{
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

//-------------------------------------------------------------------------------------
bool Entity::canNavigate()
{
	if(spaceID() <= 0)
		return false;

	Space* pSpace = Spaces::findSpace(spaceID());
	if(pSpace == NULL || !pSpace->isGood())
		return false;

	if(pSpace->pNavHandle() == NULL)
		return false;

	return true;
}

//-------------------------------------------------------------------------------------
bool Entity::navigatePathPoints( std::vector<Position3D>& outPaths, const Position3D& destination, float maxSearchDistance, int8 layer )
{
	Space* pSpace = Spaces::findSpace(spaceID());
	if(pSpace == NULL || !pSpace->isGood())
	{
		ERROR_MSG(fmt::format("Entity::navigatePathPoints(): not found space({}), entityID({})!\n",
			spaceID(), id()));

		return false;
	}

	NavigationHandlePtr pNavHandle = pSpace->pNavHandle();

	if(!pNavHandle)
	{
		WARNING_MSG(fmt::format("Entity::navigatePathPoints(): space({}), entityID({}), not found navhandle!\n",
			spaceID(), id()));

		return false;
	}

	if (pNavHandle->findStraightPath(layer, position_, destination, outPaths) < 0)
	{
		return false;
	}

	std::vector<Position3D>::iterator iter = outPaths.begin();
	while(iter != outPaths.end())
	{
		Vector3 movement = (*iter) - position_;
		if(KBEVec3Length(&movement) <= 0.00001f)
		{
			iter++;
			continue;
		}

		break;
	}

	// The first coordinate point is the current position, so it can be filtered out
	if (iter != outPaths.begin())
	{
		outPaths.erase(outPaths.begin(), iter);
	}

	return true;
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyNavigatePathPoints(PyObject_ptr pyDestination, float maxSearchDistance, int8 layer)
{
	Position3D destination;

	if(!PySequence_Check(pyDestination))
	{
		PyErr_Format(PyExc_TypeError, "%s::navigatePathPoints: args1(position) not is PySequence!", scriptName());
		PyErr_PrintEx(0);
		return 0;
	}

	if(PySequence_Size(pyDestination) != 3)
	{
		PyErr_Format(PyExc_TypeError, "%s::navigatePathPoints: args1(position) invalid!", scriptName());
		PyErr_PrintEx(0);
		return 0;
	}

	// Extract coordinate information
	script::ScriptVector3::convertPyObjectToVector3(destination, pyDestination);

	std::vector<Position3D> outPaths;
	navigatePathPoints(outPaths, destination, maxSearchDistance, layer);
	
	PyObject* pyList = PyList_New(outPaths.size());

	int i = 0;
	std::vector<Position3D>::iterator iter = outPaths.begin();
	for(; iter != outPaths.end(); ++iter)
	{
		script::ScriptVector3 *pos = new script::ScriptVector3(*iter);
		Py_INCREF(pos);
		PyList_SET_ITEM(pyList, i++, pos);
	}

	return pyList;
}

//-------------------------------------------------------------------------------------
uint32 Entity::navigate(const Position3D& destination, float velocity, float distance, float maxMoveDistance, float maxSearchDistance,
	bool faceMovement, int8 layer, PyObject* userData)
{
	VECTOR_POS3D_PTR paths_ptr( new std::vector<Position3D>() );
	navigatePathPoints(*paths_ptr, destination, maxSearchDistance, layer);
	if (paths_ptr->size() <= 0)
	{
		return 0;
	}

	stopMove();

	velocity = velocity / g_kbeSrvConfig.gameUpdateHertz();

	KBEShared_ptr<Controller> p(new MoveController(this, NULL));
	
	new NavigateHandler(p, destination, velocity, 
		distance, faceMovement, maxMoveDistance, paths_ptr, userData);

	bool ret = pControllers_->add(p);
	KBE_ASSERT(ret);
	
	pMoveController_ = p;
	return p->id();
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyNavigate(PyObject_ptr pyDestination, float velocity, float distance, float maxMoveDistance, float maxDistance,
								 int8 faceMovement, int8 layer, PyObject_ptr userData)
{
	if(!isReal())
	{
		PyErr_Format(PyExc_AssertionError, "%s::navigate: not is real entity(%d).", 
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	if(this->isDestroyed())
	{
		PyErr_Format(PyExc_AssertionError, "%s::navigate: %d is destroyed!\n",		
			scriptName(), id());		
		PyErr_PrintEx(0);
		return 0;
	}

	Position3D destination;

	if(!PySequence_Check(pyDestination))
	{
		PyErr_Format(PyExc_TypeError, "%s::navigate: args1(position) not is PySequence!", scriptName());
		PyErr_PrintEx(0);
		return 0;
	}

	if(PySequence_Size(pyDestination) != 3)
	{
		PyErr_Format(PyExc_TypeError, "%s::navigate: args1(position) invalid!", scriptName());
		PyErr_PrintEx(0);
		return 0;
	}

	// Extract coordinate information
	script::ScriptVector3::convertPyObjectToVector3(destination, pyDestination);

	return PyLong_FromLong(navigate(destination, velocity, distance, maxMoveDistance, 
		maxDistance, faceMovement > 0, layer, userData));
}

//-------------------------------------------------------------------------------------
bool Entity::getRandomPoints(std::vector<Position3D>& outPoints, const Position3D& centerPos,
	float maxRadius, uint32 maxPoints, int8 layer)
{
	Space* pSpace = Spaces::findSpace(spaceID());
	if(pSpace == NULL || !pSpace->isGood())
	{
		ERROR_MSG(fmt::format("Entity::getRandomPoints(): not found space({}), entityID({})!\n",
			spaceID(), id()));

		return false;
	}

	NavigationHandlePtr pNavHandle = pSpace->pNavHandle();

	if(!pNavHandle)
	{
		WARNING_MSG(fmt::format("Entity::getRandomPoints(): space({}), entityID({}), not found navhandle!\n",
			spaceID(), id()));
		return false;
	}

	return pNavHandle->findRandomPointAroundCircle(layer, centerPos, outPoints, maxPoints, maxRadius) > 0;
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyGetRandomPoints(PyObject_ptr pyCenterPos, float maxRadius, uint32 maxPoints, int8 layer)
{
	Position3D centerPos;

	if (!PySequence_Check(pyCenterPos))
	{
		PyErr_Format(PyExc_TypeError, "%s::getRandomPoints: args1(position) not is PySequence!", scriptName());
		PyErr_PrintEx(0);
		return 0;
	}

	if (PySequence_Size(pyCenterPos) != 3)
	{
		PyErr_Format(PyExc_TypeError, "%s::getRandomPoints: args1(position) invalid!", scriptName());
		PyErr_PrintEx(0);
		return 0;
	}

	// Extract coordinate information
	script::ScriptVector3::convertPyObjectToVector3(centerPos, pyCenterPos);

	std::vector<Position3D> outPoints;
	getRandomPoints(outPoints, centerPos, maxRadius, maxPoints, layer);
	
	PyObject* pyList = PyList_New(outPoints.size());

	int i = 0;
	std::vector<Position3D>::iterator iter = outPoints.begin();
	for (; iter != outPoints.end(); ++iter)
	{
		script::ScriptVector3 *pos = new script::ScriptVector3(*iter);
		PyList_SET_ITEM(pyList, i++, pos);
	}

	return pyList;
}

//-------------------------------------------------------------------------------------
uint32 Entity::moveToPoint(const Position3D& destination, float velocity, float distance, PyObject* userData, 
						 bool faceMovement, bool moveVertically)
{
	stopMove();

	velocity = velocity / g_kbeSrvConfig.gameUpdateHertz();

	KBEShared_ptr<Controller> p(new MoveController(this, NULL));

	new MoveToPointHandler(p, layer(), destination, velocity, 
		distance, faceMovement, moveVertically, userData);

	bool ret = pControllers_->add(p);
	KBE_ASSERT(ret);
	
	pMoveController_ = p;
	return p->id();
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyMoveToPoint(PyObject_ptr pyDestination, float velocity, float distance, PyObject_ptr userData,
								 int32 faceMovement, int32 moveVertically)
{
	if(!isReal())
	{
		PyErr_Format(PyExc_AssertionError, "%s::moveToPoint: not is real entity(%d).", 
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	if(this->isDestroyed())
	{
		PyErr_Format(PyExc_AssertionError, "%s::moveToPoint: %d is destroyed!\n",		
			scriptName(), id());		
		PyErr_PrintEx(0);
		return 0;
	}

	Position3D destination;

	if(!PySequence_Check(pyDestination))
	{
		PyErr_Format(PyExc_TypeError, "%s::moveToPoint: args1(position) not is PySequence!", scriptName());
		PyErr_PrintEx(0);
		return 0;
	}

	if(PySequence_Size(pyDestination) != 3)
	{
		PyErr_Format(PyExc_TypeError, "%s::moveToPoint: args1(position) invalid!", scriptName());
		PyErr_PrintEx(0);
		return 0;
	}

	// Extract coordinate information
	script::ScriptVector3::convertPyObjectToVector3(destination, pyDestination);

	return PyLong_FromLong(moveToPoint(destination, velocity, distance, userData, faceMovement > 0, moveVertically > 0));
}

//-------------------------------------------------------------------------------------
uint32 Entity::moveToEntity(ENTITY_ID targetID, float velocity, float distance, PyObject* userData, 
						 bool faceMovement, bool moveVertically)
{
	stopMove();

	velocity = velocity / g_kbeSrvConfig.gameUpdateHertz();

	KBEShared_ptr<Controller> p(new MoveController(this, NULL));

	new MoveToEntityHandler(p, targetID, velocity, distance,
		faceMovement, moveVertically, userData);

	bool ret = pControllers_->add(p);
	KBE_ASSERT(ret);
	
	pMoveController_ = p;
	return p->id();
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyMoveToEntity(ENTITY_ID targetID, float velocity, float distance, PyObject_ptr userData,
								 int32 faceMovement, int32 moveVertically)
{
	if(!isReal())
	{
		PyErr_Format(PyExc_AssertionError, "%s::moveToEntity: not is real entity(%d).", 
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	if(this->isDestroyed())
	{
		PyErr_Format(PyExc_AssertionError, "%s::moveToEntity: %d is destroyed!\n",		
			scriptName(), id());		
		PyErr_PrintEx(0);
		return 0;
	}

	return PyLong_FromLong(moveToEntity(targetID, velocity, distance, userData, faceMovement > 0, moveVertically > 0));
}

//-------------------------------------------------------------------------------------
void Entity::onMove(uint32 controllerId, int layer, const Position3D& oldPos, PyObject* userarg)
{
	if(this->isDestroyed())
		return;

	SCOPED_PROFILE(ONMOVE_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onMove"),
		Py_BuildValue(const_cast<char*>("(IO)"), controllerId, userarg));

	setDirty();
}

//-------------------------------------------------------------------------------------
void Entity::onMoveOver(uint32 controllerId, int layer, const Position3D& oldPos, PyObject* userarg)
{
	if(this->isDestroyed())
		return;

	if(pMoveController_ == NULL)
		return;
	
	pMoveController_->destroy();
	pMoveController_.reset();

	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onMoveOver"),
		Py_BuildValue(const_cast<char*>("(IO)"), controllerId, userarg));
	
	setDirty();
}

//-------------------------------------------------------------------------------------
void Entity::onMoveFailure(uint32 controllerId, PyObject* userarg)
{
	if(this->isDestroyed())
		return;

	if(pMoveController_ == NULL)
		return;
	
	pMoveController_->destroy();
	pMoveController_.reset();

	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onMoveFailure"),
		Py_BuildValue(const_cast<char*>("(IO)"), controllerId, userarg));
	
	setDirty();
}

//-------------------------------------------------------------------------------------
float Entity::accelerate(const char* type, float acceleration)
{
	acceleration = acceleration / g_kbeSrvConfig.gameUpdateHertz();

	if (strcmp(type, "Movement") == 0)
	{
		MoveController* pMoveController = static_cast<MoveController*>(pMoveController_.get());
		if (pMoveController != NULL)
		{
			float velocity = pMoveController->velocity() + acceleration;
			pMoveController->velocity(velocity);
			return velocity * g_kbeSrvConfig.gameUpdateHertz();
		}
	}
	else if (strcmp(type, "Turn") == 0)
	{
		TurnController* pTurnController = static_cast<TurnController*>(pTurnController_.get());
		if (pTurnController != NULL)
		{
			float velocity = pTurnController->velocity() + acceleration;
			pTurnController->velocity(velocity);
			return velocity * g_kbeSrvConfig.gameUpdateHertz();
		}
	}
	else
	{
		PyErr_Format(PyExc_AssertionError, "%s::accelerate: %d type error! only support[\"Movement\",\"Turn\"]\n",
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0.f;
	}

	return 0.f;
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyAccelerate(const_charptr type, float acceleration)
{
	if (!isReal())
	{
		PyErr_Format(PyExc_AssertionError, "%s::accelerate: not is real entity(%d).",
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	if (this->isDestroyed())
	{
		PyErr_Format(PyExc_AssertionError, "%s::accelerate: %d is destroyed!\n",
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	return PyFloat_FromDouble(accelerate(type, acceleration));
}

//-------------------------------------------------------------------------------------
uint32 Entity::addYawRotator(float yaw, float velocity, PyObject* userData)
{
	stopMove();

	velocity = velocity / g_kbeSrvConfig.gameUpdateHertz();

	KBEShared_ptr<Controller> p(new TurnController(this, NULL));

	Direction3D dir;
	dir.yaw(yaw);

	new RotatorHandler(p, dir, velocity,
		userData);

	bool ret = pControllers_->add(p);
	KBE_ASSERT(ret);

	pTurnController_ = p;
	return p->id();
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyAddYawRotator(float yaw, float velocity, PyObject* userData)
{
	if (!isReal())
	{
		PyErr_Format(PyExc_AssertionError, "%s::addYawRotator: not is real entity(%d).",
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	if (this->isDestroyed())
	{
		PyErr_Format(PyExc_AssertionError, "%s::addYawRotator: %d is destroyed!\n",
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	return PyLong_FromLong(addYawRotator(yaw, velocity, userData));
}

//-------------------------------------------------------------------------------------
void Entity::onTurn(uint32 controllerId, PyObject* userarg)
{
	if (this->isDestroyed())
		return;

	pTurnController_.reset();

	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onTurn"),
		Py_BuildValue(const_cast<char*>("(IO)"), controllerId, userarg));

	setDirty();
}

//-------------------------------------------------------------------------------------
void Entity::debugView()
{
	if(pWitness_ == NULL)
	{
		Cellapp::getSingleton().getScript().pyPrint(fmt::format("{}::debugView: {} has no witness!", scriptName(), this->id()));
		return;
	}
	
	int pending = 0;
	Witness::VIEW_ENTITIES::iterator iter = pWitness_->viewEntities().begin();
	for (; iter != pWitness_->viewEntities().end(); ++iter)
	{
		Entity* pEntity = (*iter)->pEntity();

		if(pEntity)
		{
			if(((*iter)->flags() & ENTITYREF_FLAG_ENTER_CLIENT_PENDING) > 0)
				pending++;
		}
	}

	Cellapp::getSingleton().getScript().pyPrint(fmt::format("{}::debugView: {} size={}, Seen={}, Pending={}, viewRadius={}, viewHyst={}", scriptName(), this->id(), 
		pWitness_->viewEntitiesMap().size(), pWitness_->viewEntitiesMap().size() - pending, pending, pWitness_->viewRadius(), pWitness_->viewHysteresisArea()));

	iter = pWitness_->viewEntities().begin();
	for(; iter != pWitness_->viewEntities().end(); ++iter)
	{
		Entity* pEntity = (*iter)->pEntity();
		Position3D epos;
		float dist = 0.0f;

		if(pEntity)
		{
			epos = pEntity->position();
			Vector3 distvec = epos - this->position();
			dist = KBEVec3Length(&distvec);
		}

		Cellapp::getSingleton().getScript().pyPrint(fmt::format("{7}::debugView: {0} {1}({2}), position({3}.{4}.{5}), dist={6}, Seen={8}", 
			this->id(), 
			(pEntity != NULL ? pEntity->scriptName() : "unknown"),
			(*iter)->id(),
			epos.x, epos.y, epos.z,
			dist,
			this->scriptName(), (((*iter)->flags() & ENTITYREF_FLAG_ENTER_CLIENT_PENDING) ? "false" : "true")));
	}
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyDebugView()
{
	if(!isReal())
	{
		PyErr_Format(PyExc_AssertionError, "%s::debugView: not is real entity(%d).", 
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	if (!hasFlags(ENTITY_FLAGS_DESTROYING) && this->isDestroyed())
	{
		PyErr_Format(PyExc_AssertionError, "%s::debugView: %d is destroyed!\n",		
			scriptName(), id());		
		PyErr_PrintEx(0);
		return 0;
	}

	debugView();
	S_Return;
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyEntitiesInView()
{
	if(!isReal())
	{
		PyErr_Format(PyExc_AssertionError, "%s::entitiesInView: not is real entity(%d).", 
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	if(this->isDestroyed())
	{
		PyErr_Format(PyExc_AssertionError, "%s::entitiesInView: %d is destroyed!\n",		
			scriptName(), id());		
		PyErr_PrintEx(0);
		return 0;
	}

	if (!pWitness_)
	{
		PyErr_Format(PyExc_AssertionError, "%s::entitiesInView: %d has no witness!\n",
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	int calcSize = 0;
	Witness::VIEW_ENTITIES::iterator iter = pWitness_->viewEntities().begin();
	
	for(; iter != pWitness_->viewEntities().end(); ++iter)
	{
		Entity* pEntity = (*iter)->pEntity();

		if(pEntity)
		{
			++calcSize;
		}
	}
	
	PyObject* pyList = PyList_New(calcSize);
	
	iter = pWitness_->viewEntities().begin();
		
	int i = 0;
	for(; iter != pWitness_->viewEntities().end(); ++iter)
	{
		Entity* pEntity = (*iter)->pEntity();

		if(pEntity)
		{
			Py_INCREF(pEntity);
			PyList_SET_ITEM(pyList, i++, pEntity);
		}
	}

	return pyList;
}

//-------------------------------------------------------------------------------------
PyObject* Entity::__py_pyEntitiesInRange(PyObject* self, PyObject* args)
{
	uint16 currargsSize = PyTuple_Size(args);
	Entity* pobj = static_cast<Entity*>(self);

	if (!pobj->isReal())
	{
		PyErr_Format(PyExc_AssertionError, "%s::entitiesInRange: not is real entity(%d).",
			pobj->scriptName(), pobj->id());
		PyErr_PrintEx(0);
		return 0;
	}

	PyObject* pyPosition = NULL, *pyEntityType = NULL;
	float radius = 0.f;

	if (pobj->isDestroyed() && !pobj->hasFlags(ENTITY_FLAGS_DESTROYING) /* 允许在销毁期间调用 */)
	{
		PyErr_Format(PyExc_TypeError, "%s::entitiesInRange: entity(%d) is destroyed!",
			pobj->scriptName(), pobj->id());
		PyErr_PrintEx(0);
		return 0;
	}

	if (currargsSize == 1)
	{
		if (PyArg_ParseTuple(args, "f", &radius) == -1)
		{
			PyErr_Format(PyExc_TypeError, "%s::entitiesInRange: args error! entity(%d)",
				pobj->scriptName(), pobj->id());
			PyErr_PrintEx(0);
			return 0;
		}
	}
	else if (currargsSize == 2)
	{
		if (PyArg_ParseTuple(args, "fO", &radius, &pyEntityType) == -1)
		{
			PyErr_Format(PyExc_TypeError, "%s::entitiesInRange: args error! entity(%d)",
				pobj->scriptName(), pobj->id());
			PyErr_PrintEx(0);
			return 0;
		}

		if (pyEntityType && pyEntityType != Py_None && !PyUnicode_Check(pyEntityType))
		{
			PyErr_Format(PyExc_TypeError, "%s::entitiesInRange: args(entityType) error! entity(%d)",
				pobj->scriptName(), pobj->id());
			PyErr_PrintEx(0);
			return 0;
		}

	}
	else if (currargsSize == 3)
	{
		if (PyArg_ParseTuple(args, "fOO", &radius, &pyEntityType, &pyPosition) == -1)
		{
			PyErr_Format(PyExc_TypeError, "%s::entitiesInRange: args error! entity(%d)",
				pobj->scriptName(), pobj->id());
			PyErr_PrintEx(0);
			return 0;
		}

		if (pyEntityType && pyEntityType != Py_None && !PyUnicode_Check(pyEntityType))
		{
			PyErr_Format(PyExc_TypeError, "%s::entitiesInRange: args(entityType) error! entity(%d)",
				pobj->scriptName(), pobj->id());
			PyErr_PrintEx(0);
			return 0;
		}

		if (pyPosition != Py_None && (!PySequence_Check(pyPosition) || PySequence_Size(pyPosition) < 3))
		{
			PyErr_Format(PyExc_TypeError, "%s::entitiesInRange: args(position) error! entity(%d)",
				pobj->scriptName(), pobj->id());
			PyErr_PrintEx(0);
			return 0;
		}
	}
	else
	{
		PyErr_Format(PyExc_TypeError, "%s::entitiesInRange: args error! entity(%d)",
			pobj->scriptName(), pobj->id());
		PyErr_PrintEx(0);
		return 0;
	}

	char* pEntityType = NULL;
	Position3D originpos;

	// Extract coordinate information
	if (pyPosition && pyPosition != Py_None)
	{
		script::ScriptVector3::convertPyObjectToVector3(originpos, pyPosition);
	}
	else
	{
		originpos = pobj->position();
	}

	if (pyEntityType && pyEntityType != Py_None)
	{
		wchar_t* PyUnicode_AsWideCharStringRet0 = PyUnicode_AsWideCharString(pyEntityType, NULL);
		pEntityType = strutil::wchar2char(PyUnicode_AsWideCharStringRet0);
		PyMem_Free(PyUnicode_AsWideCharStringRet0);
	}

	int entityUType = -1;

	if (pEntityType)
	{
		ScriptDefModule* sm = EntityDef::findScriptModule(pEntityType);
		if (sm == NULL)
		{
			free(pEntityType);
			return PyList_New(0);
		}

		free(pEntityType);
		entityUType = sm->getUType();
	}

	std::vector<Entity*> findentities;

	// Users always expect to search near the entity, so we search from around
	EntityCoordinateNode::entitiesInRange(findentities, pobj->pEntityCoordinateNode(), originpos, radius, entityUType);

	PyObject* pyList = PyList_New(findentities.size());

	std::vector<Entity*>::iterator iter = findentities.begin();
	int i = 0;
	for (; iter != findentities.end(); ++iter)
	{
		Entity* pEntity = (*iter);

		Py_INCREF(pEntity);
		PyList_SET_ITEM(pyList, i++, pEntity);
	}

	return pyList;
}

//-------------------------------------------------------------------------------------
void Entity::_sendBaseTeleportResult(ENTITY_ID sourceEntityID, COMPONENT_ID sourceBaseAppID, SPACE_ID spaceID, SPACE_ID lastSpaceID, bool fromCellTeleport)
{
	Components::ComponentInfos* cinfos = Components::getSingleton().findComponent(sourceBaseAppID);
	if(cinfos != NULL && cinfos->pChannel != NULL)
	{
		Network::Bundle* pBundle = Network::Bundle::createPoolObject();
		(*pBundle).newMessage(BaseappInterface::onTeleportCB);
		(*pBundle) << sourceEntityID;
		BaseappInterface::onTeleportCBArgs2::staticAddToBundle((*pBundle), spaceID, fromCellTeleport);
		cinfos->pChannel->send(pBundle);
	}
}

//-------------------------------------------------------------------------------------
void Entity::teleportFromBaseapp(Network::Channel* pChannel, COMPONENT_ID cellAppID, ENTITY_ID targetEntityID, COMPONENT_ID sourceBaseAppID)
{
	DEBUG_MSG(fmt::format("{}::teleportFromBaseapp: {}, targetEntityID={}, cell={}, sourceBaseAppID={}.\n", 
		this->scriptName(), this->id(), targetEntityID, cellAppID, sourceBaseAppID));

	SPACE_ID lastSpaceID = this->spaceID();

	if (!isReal())
	{
		ERROR_MSG(fmt::format("{}::teleportFromBaseapp: not is real entity({}), sourceBaseAppID={}.\n",
			this->scriptName(), this->id(), sourceBaseAppID));

		_sendBaseTeleportResult(this->id(), sourceBaseAppID, 0, lastSpaceID, false);
		return;
	}

	if(hasFlags(ENTITY_FLAGS_TELEPORT_START))
	{
		ERROR_MSG(fmt::format("{}::teleportFromBaseapp: In transit! entity={}, sourceBaseAppID={}.\n",
			this->scriptName(), this->id(), sourceBaseAppID));

		_sendBaseTeleportResult(this->id(), sourceBaseAppID, 0, lastSpaceID, false);
		return;
	}
	
	// If not on a cell
	if(cellAppID != g_componentID)
	{
		Components::ComponentInfos* cinfos = Components::getSingleton().findComponent(cellAppID);
		if(cinfos == NULL || cinfos->pChannel == NULL)
		{
			ERROR_MSG(fmt::format("{}::teleportFromBaseapp: {}, teleport error, not found cellapp, targetEntityID, cellAppID={}.\n",
				this->scriptName(), this->id(), targetEntityID, cellAppID));

			_sendBaseTeleportResult(this->id(), sourceBaseAppID, 0, lastSpaceID, false);
			return;
		}

		// The target cell is not current, we can now move the entity to its destination
	}
	else
	{
		Entity* entity = Cellapp::getSingleton().findEntity(targetEntityID);
		if(entity == NULL || entity->isDestroyed())
		{
			ERROR_MSG(fmt::format("{}::teleportFromBaseapp: {}, can't found targetEntity({}).\n",
				this->scriptName(), this->id(), targetEntityID));

			_sendBaseTeleportResult(this->id(), sourceBaseAppID, 0, lastSpaceID, false);
			return;
		}
		
		// Find space
		SPACE_ID spaceID = entity->spaceID();

		// If it is a different space jump
		if(spaceID != this->spaceID())
		{
			Space* space = Spaces::findSpace(spaceID);
			if(space == NULL || !space->isGood())
			{
				ERROR_MSG(fmt::format("{}::teleportFromBaseapp: {}, can't found space({}).\n",
					this->scriptName(), this->id(), spaceID));

				_sendBaseTeleportResult(this->id(), sourceBaseAppID, 0, lastSpaceID, false);
				return;
			}
			
			Space* currspace = Spaces::findSpace(this->spaceID());
			currspace->removeEntity(this);
			space->addEntityAndEnterWorld(this);
			_sendBaseTeleportResult(this->id(), sourceBaseAppID, spaceID, lastSpaceID, false);
		}
		else
		{
			WARNING_MSG(fmt::format("{}::teleportFromBaseapp: {} targetSpace({}) == currSpaceID({}).\n",
				this->scriptName(), this->id(), spaceID, this->spaceID()));

			_sendBaseTeleportResult(this->id(), sourceBaseAppID, spaceID, lastSpaceID, false);
		}
	}
}

//-------------------------------------------------------------------------------------
PyObject* Entity::pyTeleport(PyObject* nearbyMBRef, PyObject* pyposition, PyObject* pydirection)
{
	if(!isReal())
	{
		PyErr_Format(PyExc_AssertionError, "%s::teleport: not is real entity(%d).", 
			scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	if(this->isDestroyed())
	{
		PyErr_Format(PyExc_AssertionError, "%s::teleport: %d is destroyed!\n",		
			scriptName(), id());		
		PyErr_PrintEx(0);
		return 0;
	}

	Space* currspace = Spaces::findSpace(this->spaceID());
	if(currspace == NULL || !currspace->isGood())
	{
		PyErr_Format(PyExc_Exception, "%s::teleport: %d, current space has been destroyed!\n", scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}
	
	if(!PySequence_Check(pyposition) || PySequence_Size(pyposition) != 3)
	{
		PyErr_Format(PyExc_Exception, "%s::teleport: %d position not is Sequence!\n", scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	if(!PySequence_Check(pydirection) || PySequence_Size(pydirection) != 3)
	{
		PyErr_Format(PyExc_Exception, "%s::teleport: %d direction not is Sequence!\n", scriptName(), id());
		PyErr_PrintEx(0);
		return 0;
	}

	Position3D pos;
	Direction3D dir;

	PyObject* pyitem = PySequence_GetItem(pyposition, 0);
	pos.x = (float)PyFloat_AsDouble(pyitem);
	Py_DECREF(pyitem);

	pyitem = PySequence_GetItem(pyposition, 1);
	pos.y = (float)PyFloat_AsDouble(pyitem);
	Py_DECREF(pyitem);

	pyitem = PySequence_GetItem(pyposition, 2);
	pos.z = (float)PyFloat_AsDouble(pyitem);
	Py_DECREF(pyitem);

	pyitem = PySequence_GetItem(pydirection, 0);
	dir.roll((float)PyFloat_AsDouble(pyitem));
	Py_DECREF(pyitem);

	pyitem = PySequence_GetItem(pydirection, 1);
	dir.pitch((float)PyFloat_AsDouble(pyitem));
	Py_DECREF(pyitem);

	pyitem = PySequence_GetItem(pydirection, 2);
	dir.yaw((float)PyFloat_AsDouble(pyitem));
	Py_DECREF(pyitem);
	
	teleport(nearbyMBRef, pos, dir);
	S_Return;
}

//-------------------------------------------------------------------------------------
void Entity::teleportRefEntity(Entity* entity, Position3D& pos, Direction3D& dir)
{
	if(entity == NULL)
	{
		PyErr_Format(PyExc_Exception, "%s::teleport: %d nearbyEntityRef is null!\n", scriptName(), id());
		PyErr_PrintEx(0);

		onTeleportFailure();
		return;
	}
	
	SPACE_ID lastSpaceID = this->spaceID();
	
	/* Even if the entity is destroyed, the spaceID should be correct when the memory is not freed, so in theory can still find the space
	if(entity->isDestroyed())
	{
		ERROR_MSG("Entity::teleport: nearbyMBRef is destroyed!\n");
		onTeleportFailure();
		return;
	}
	*/

	/* Even if ghost, but space is definitely on the current cell, direct operation should not be a problem
	if(!entity->isReal())
	{
		ERROR_MSG("Entity::teleport: nearbyMBRef is ghost!\n");
		onTeleportFailure();
		return;
	}
	*/

	SPACE_ID spaceID = entity->spaceID();

	// If same space do a local jump
	if(spaceID == this->spaceID())
	{
		teleportLocal(entity, pos, dir);
	}
	else
	{
		// Otherwise it is space on the current cellapp, then we can also perform operations directly
		Space* currspace = Spaces::findSpace(this->spaceID());
		Space* space = Spaces::findSpace(spaceID);

		// If the space to be jumped does not exist,
		//  or the referenced entity is the creator of this space and has been destroyed,
		//  then it should be a jump failure.
		if(space == NULL || !space->isGood() || entity->isDestroyed())
		{
			if (entity->isDestroyed())
			{
				PyErr_Format(PyExc_Exception, "%s::teleport: %d, nearbyEntityRef has been destroyed!\n", scriptName(), id());
				PyErr_PrintEx(0);
			}
			else
			{
				PyErr_Format(PyExc_Exception, "%s::teleport: %d, not found space(%d)!\n", scriptName(), id() % spaceID);
				PyErr_PrintEx(0);
			}

			onTeleportFailure();
			return;
		}

		currspace->removeEntity(this);
		this->setPositionAndDirection(pos, dir);
		space->addEntityAndEnterWorld(this);

		onTeleportSuccess(entity, lastSpaceID);
	}
}

//-------------------------------------------------------------------------------------
void Entity::teleportRefEntityCall(EntityCall* nearbyMBRef, Position3D& pos, Direction3D& dir)
{
	if(hasFlags(ENTITY_FLAGS_TELEPORT_START))
	{
		PyErr_Format(PyExc_Exception, "%s::teleport: %d, In transit!\n", 
			scriptName(), id());

		PyErr_PrintEx(0);

		onTeleportFailure();
	}
	
	if (!nearbyMBRef->isCellReal())
	{
		char buf[1024];
		nearbyMBRef->c_str(buf, 1024);

		PyErr_Format(PyExc_Exception, "%s::teleport: %d, nearbyRef error, not is cellEntityCall! curr=%s\n", 
			scriptName(), id(), buf);

		PyErr_PrintEx(0);

		onTeleportFailure();
		return;
	}

	// 如果这个entity有base部分， 假如是本进程级别的传送，那么相关操作按照正常的执行
	// 如果是跨cellapp的传送， 那么我们可以先设置entity为ghost并立即序列化entity发往目的cellapp
	// 如果期间有base的消息发送过来， entity的ghost机制能够转到real上去， 因此传送之前不需要对base
	// 做一些设置，传送成功后先设置base的关系base在被改变关系后仍然有0.1秒的时间收到包继续发往ghost，
	// 如果一直有包则一直刷新时间直到没有任何包需要广播并且超时0.1秒之后的包才会直接发往real）, 这样做的好处是传送并不需要非常谨慎的与base耦合
	// 传送过程中有任何错误也不会影响到base部分，base部分的包也能够按照秩序送往real。
	// ...partial goog translate:
	// If this entity has a base section, if it is a process-level transfer, then the associated operation is performed as normal
	// If the transmission is across Cellapp, then we can set entity to ghost and immediately serialize entity to the destination Cellapp
	// If a base message is sent during this period, the entity's ghost can forward to real, so the base does not need to be transmitted before
	// 做一些设置，传送成功后先设置base的关系base在被改变关系后仍然有0.1秒的时间收到包继续发往ghost，
	// 如果一直有包则一直刷新时间直到没有任何包需要广播并且超时0.1秒之后的包才会直接发往real）, 这样做的好处是传送并不需要非常谨慎的与base耦合
	// Any errors in the transmission process will not affect the base part. The base part of the package can also be sent to real in order.
	if(this->baseEntityCall() != NULL)
	{
		// If we have the base part, we also need to call the backup function.
		// Because the ghost function will addCellDataToStream a data stream,
		//  and can reuse the entity when the delivery fails, there is no need to backup
		// this->backupCellData();
		
		Network::Channel* pBaseChannel = baseEntityCall()->getChannel();
		if(pBaseChannel)
		{
			// At the same time, need to inform base to temporarily store the message sent to cellapp, because if the jump
			//  succeeds later, it needs to switch the cellEntityCall to the new cellapp entity.
			// In order to avoid mess in the moment of handover (the old cellapp message will also go to the new cellapp),
			//  it needs to be done before the transfer
			// Temporarily, after the success of the transfer, the old cellapp is notified to destroy the entity and the
			//  baseapp is notified to change the mapping at the same time.
			Network::Bundle* pBundle = Network::Bundle::createPoolObject();
			(*pBundle).newMessage(BaseappInterface::onMigrationCellappStart);
			(*pBundle) << id();
			(*pBundle) << g_componentID;
			(*pBundle) << nearbyMBRef->componentID();
			pBaseChannel->send(pBundle);
		}
		else
		{
			PyErr_Format(PyExc_Exception, "%s::teleport: %d, nearbyRef error, not found baseapp!\n", scriptName(), id());
			PyErr_PrintEx(0);
			onTeleportFailure();
			return;
		}
	}

	onTeleportRefEntityCall(nearbyMBRef, pos, dir);
}

//-------------------------------------------------------------------------------------
void Entity::onTeleportRefEntityCall(EntityCall* nearbyMBRef, Position3D& pos, Direction3D& dir)
{
	// We need to send the entity packet to the destination cellapp
	Network::Bundle* pBundle = Network::Bundle::createPoolObject();
	(*pBundle).newMessage(CellappInterface::reqTeleportToCellApp);
	(*pBundle) << id();
	(*pBundle) << nearbyMBRef->id();
	(*pBundle) << spaceID();
	(*pBundle) << pScriptModule()->getUType();
	(*pBundle) << pos.x << pos.y << pos.z;
	(*pBundle) << dir.roll() << dir.pitch() << dir.yaw();
	(*pBundle) << g_componentID;

	MemoryStream* s = MemoryStream::createPoolObject();
	changeToGhost(nearbyMBRef->componentID(), *s);

	(*pBundle).append(s);
	MemoryStream::reclaimPoolObject(s);

	// Don't destroy the entity for the time being, wait for it to be successfully created
	// Messages during this period can be forwarded to real via ghost
	// If it fails to transmit correctly, you can resume the entity from the current cell.
	// Cellapp::getSingleton().destroyEntity(id(), false);

	nearbyMBRef->sendCall(pBundle);

	// After the serialization, the entity will stop moving first. If the transmission fails,
	//  it can be restored according to the serialized content.
	stopMove();
}

//-------------------------------------------------------------------------------------
void Entity::teleportLocal(PyObject_ptr nearbyMBRef, Position3D& pos, Direction3D& dir)
{
	// In the future, local teleport needs to consider a space can be divided into multiple cells.
	// Currently though, it can just use direct calls
	SPACE_ID lastSpaceID = this->spaceID();

	// First remove the entity node from CoordinateSystem
	Space* currspace = Spaces::findSpace(this->spaceID());
	this->uninstallCoordinateNodes(currspace->pCoordinateSystem());

	// This does not disturb the rangelist
	this->setPositionAndDirection(pos, dir);

	if(this->pWitness())
	{
		// Notify that location was forcibly changed
		Network::Bundle* pSendBundle = Network::Bundle::createPoolObject();
		NETWORK_ENTITY_MESSAGE_FORWARD_CLIENT_BEGIN(id(), (*pSendBundle));
		
		ENTITY_MESSAGE_FORWARD_CLIENT_BEGIN(pSendBundle, ClientInterface::onSetEntityPosAndDir, setEntityPosAndDir);
		(*pSendBundle) << id();
		(*pSendBundle) << pos.x << pos.y << pos.z;
		(*pSendBundle) << direction().roll() << direction().pitch() << direction().yaw();

		ENTITY_MESSAGE_FORWARD_CLIENT_END(pSendBundle, ClientInterface::onSetEntityPosAndDir, setEntityPosAndDir);
		this->pWitness()->sendToClient(ClientInterface::onSetEntityPosAndDir, pSendBundle);
	}

	currspace->addEntityToNode(this);

	std::list<ENTITY_ID>::iterator witer = witnesses_.begin();
	for (; witer != witnesses_.end(); ++witer)
	{
		Entity* pEntity = Cellapp::getSingleton().findEntity((*witer));
		if (pEntity == NULL || pEntity->pWitness() == NULL)
			continue;

		EntityCall* clientEntityCall = pEntity->clientEntityCall();
		if (clientEntityCall == NULL)
			continue;

		Network::Channel* pChannel = clientEntityCall->getChannel();
		if (pChannel == NULL)
			continue;

		// This possibility exists, for example the data comes from createWitnessFromStream()
		// Another example is that their entity is not yet created on the target client
		if (!pEntity->pWitness()->entityInView(id()))
			continue;

		// Notify that location was forcibly changed
		Network::Bundle* pSendBundle = Network::Bundle::createPoolObject();
		NETWORK_ENTITY_MESSAGE_FORWARD_CLIENT_BEGIN(pEntity->id(), (*pSendBundle));
		
		ENTITY_MESSAGE_FORWARD_CLIENT_BEGIN(pSendBundle, ClientInterface::onSetEntityPosAndDir, setEntityPosAndDir);
		(*pSendBundle) << id();
		(*pSendBundle) << pos.x << pos.y << pos.z;
		(*pSendBundle) << direction().roll() << direction().pitch() << direction().yaw();

		ENTITY_MESSAGE_FORWARD_CLIENT_END(pSendBundle, ClientInterface::onSetEntityPosAndDir, setEntityPosAndDir);
		pEntity->pWitness()->sendToClient(ClientInterface::onSetEntityPosAndDir, pSendBundle);
	}

	onTeleportSuccess(nearbyMBRef, lastSpaceID);
}

//-------------------------------------------------------------------------------------
void Entity::teleport(PyObject_ptr nearbyMBRef, Position3D& pos, Direction3D& dir)
{
	/*
		1: Any form of teleport is considered to be momentary (can instantly move to any space),
			even if it is only 0.1m from the current position. If the current entity happens to be in some trap,
			the teleport before moving 0.1 meters will cause leaving the trap, because this is the characteristic of the momentary
			movement. Currently, the entity will leave the trap triggering the relevant callback, and then
			instantaneously appear at another point, then because this point is also in the current trap,
			also throws the enter trap callback.

		2: If it is a jump on the current space, move it immediately

		3: If it is a jump to another space, but that space is also in the current cellapp situation, immediately perform
			the teleport operation (since there is no need to do any other maintenance of the relationship, just switch directly).
		
		4: If the target space to jump to is on another cellapp:
			4.1: The current entity does not have a base part, and does not need to consider maintaining the relationship of the base part.
				However, it is still necessary to consider unexpected situations that cause a teleport failure.
				At this point, the jump failure callback should be returned and continue.
					Normally exists on the current space.
		
			4.2: The current entity has a base part, so we need to change the cell EntityCall of the base.
				(and all messages sent to the cell on the baseapp should not be lost when the relationship is in the process of being switched).
				We need to do some work to ensure safety.
	*/

	Py_INCREF(this);

	// If it is None, then the entity wants to jump to a location on this space.
	if(nearbyMBRef == Py_None)
	{
		// Directly perform operations
		teleportLocal(nearbyMBRef, pos, dir);
	}
	else
	{
		//EntityCall* mb = NULL;

		// If it is an entity, it must be on this cellapp and can also be directly operated
		if(PyObject_TypeCheck(nearbyMBRef, Entity::getScriptType()))
		{
			teleportRefEntity(static_cast<Entity*>(nearbyMBRef), pos, dir);
		}
		else
		{
			// If it is an entityCall, first check whether the cell can find the entity through the ID of the entityCall.
			// If it can find it, it is also on this cellapp and can operate directly on it
			if(PyObject_TypeCheck(nearbyMBRef, EntityCall::getScriptType()))
			{
				EntityCall* mb = static_cast<EntityCall*>(nearbyMBRef);
				Entity* entity = Cellapp::getSingleton().findEntity(mb->id());
				
				if(entity)
				{
					teleportRefEntity(entity, pos, dir);
				}
				else
				{
					teleportRefEntityCall(mb, pos, dir);
				}
			}
			else
			{
				// If it's not an Entity, not an EntityCall, and it's not None, then must be an input error.
				PyErr_Format(PyExc_Exception, "%s::teleport: %d, nearbyRef error!\n", scriptName(), id());
				PyErr_PrintEx(0);

				onTeleportFailure();
			}
		}
	}
	
	Py_DECREF(this);
}

//-------------------------------------------------------------------------------------
void Entity::onTeleport()
{
	// This method is only called before the base.teleport jump, cell.teleport will not be called.
	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onTeleport"), NULL);
}

//-------------------------------------------------------------------------------------
void Entity::onTeleportFailure()
{
	ERROR_MSG(fmt::format("{}::onTeleportFailure(): entityID={}\n", 
		this->scriptName(), id()));

	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onTeleportFailure"), NULL);
}

//-------------------------------------------------------------------------------------
void Entity::onTeleportSuccess(PyObject* nearbyEntity, SPACE_ID lastSpaceID)
{
	EntityCall* mb = this->baseEntityCall();
	if(mb)
	{
		_sendBaseTeleportResult(this->id(), mb->componentID(), this->spaceID(), lastSpaceID, true);
	}

	// If the Entity has traps and other triggers, they have to be added back in
	restoreProximitys();

	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onTeleportSuccess"),
		Py_BuildValue(const_cast<char*>("(O)"), nearbyEntity));
}

//-------------------------------------------------------------------------------------
void Entity::onEnterSpace(Space* pSpace)
{
	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onEnterSpace"), NULL);
}

//-------------------------------------------------------------------------------------
void Entity::onLeaveSpace(Space* pSpace)
{
	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onLeaveSpace"), NULL);
}

//-------------------------------------------------------------------------------------
void Entity::onEnteredCell()
{
	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onEnteredCell"), NULL);
}

//-------------------------------------------------------------------------------------
void Entity::onEnteringCell()
{
	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onEnteringCell"), NULL);
}

//-------------------------------------------------------------------------------------
void Entity::onLeavingCell()
{
	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onLeavingCell"), NULL);
}

//-------------------------------------------------------------------------------------
void Entity::onLeftCell()
{
	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onLeftCell"), NULL);
}

//-------------------------------------------------------------------------------------
void Entity::onRestore()
{
	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onRestore"), NULL);
	removeFlags(ENTITY_FLAGS_INITING);
}

//-------------------------------------------------------------------------------------
bool Entity::_reload(bool fullReload)
{
	allClients_->setScriptModule(pScriptModule_);
	return true;
}

//-------------------------------------------------------------------------------------
void Entity::onUpdateGhostPropertys(KBEngine::MemoryStream& s)
{
	ENTITY_PROPERTY_UID utype;
	s >> utype;

	PropertyDescription* pPropertyDescription = pScriptModule()->findCellPropertyDescription(utype);
	if(pPropertyDescription == NULL)
	{
		ERROR_MSG(fmt::format("{}::onUpdateGhostPropertys: not found propertyID({}), entityID({})\n", 
			scriptName(), utype, id()));

		s.done();
		return;
	}

	DEBUG_MSG(fmt::format("{}::onUpdateGhostPropertys: property({}), entityID({})\n", 
		scriptName(), pPropertyDescription->getName(), id()));

	PyObject* pyVal = pPropertyDescription->createFromStream(&s);
	if(pyVal == NULL)
	{
		ERROR_MSG(fmt::format("{}::onUpdateGhostPropertys: entityID={}, create({}) error!\n", 
			scriptName(), id(), pPropertyDescription->getName()));

		s.done();
		return;
	}

	PyObject_SetAttrString(static_cast<PyObject*>(this),
				pPropertyDescription->getName(), pyVal);

	Py_DECREF(pyVal);
}

//-------------------------------------------------------------------------------------
void Entity::onRemoteRealMethodCall(KBEngine::MemoryStream& s)
{
	ENTITY_PROPERTY_UID componentPropertyUID = 0;
	s >> componentPropertyUID;

	ENTITY_METHOD_UID utype = 0;
	s >> utype;

	ScriptDefModule* pCurrScriptModule = pScriptModule();

	PropertyDescription* pComponentPropertyDescription = NULL;
	if (componentPropertyUID > 0)
	{
		pComponentPropertyDescription = pCurrScriptModule->findCellPropertyDescription(componentPropertyUID);
	}

	if (pComponentPropertyDescription)
	{
		DataType* pDataType = pComponentPropertyDescription->getDataType();
		KBE_ASSERT(pDataType->type() == DATA_TYPE_ENTITY_COMPONENT);

		pCurrScriptModule = static_cast<EntityComponentType*>(pDataType)->pScriptDefModule();
	}

	MethodDescription* pMethodDescription = pCurrScriptModule->findCellMethodDescription(utype);
	if(pMethodDescription == NULL)
	{
		ERROR_MSG(fmt::format("{}::onRemoteRealMethodCall: not found {} method({}), entityID({})\n", 
			scriptName(), (pComponentPropertyDescription ? (std::string("component[") + 
				std::string(pCurrScriptModule->getName()) + "] ") : ""), utype, id()));

		s.done();
		return;
	}

	onRemoteMethodCall_(pComponentPropertyDescription, pMethodDescription, id(), s);
}

//-------------------------------------------------------------------------------------
void Entity::onUpdateGhostVolatileData(KBEngine::MemoryStream& s)
{
	DEBUG_MSG(fmt::format("{}::onUpdateGhostVolatileData: entityID({})\n", 
		scriptName(), id()));
}

//-------------------------------------------------------------------------------------
void Entity::changeToGhost(COMPONENT_ID realCell, KBEngine::MemoryStream& s)
{
	// Convert Entity to a ghost
	// First, need to set our realCell
	// Add all def data into the stream
	// Serialize the controller and stop all controllers (timer, navigate, trap,...)
	// Uninstall Witness and Serialize
	KBE_ASSERT(isReal() == true && "Entity::changeToGhost(): not is real.\n");
	KBE_ASSERT(realCell_ != g_componentID);

	realCell_ = realCell;
	ghostCell_ = 0;
	
	GhostManager* gm = Cellapp::getSingleton().pGhostManager();
	if(gm)
	{
		gm->addRoute(id(), realCell_);
	}

	DEBUG_MSG(fmt::format("{}::changeToGhost(): {}, realCell={}, spaceID={}, position=({},{},{}).\n", 
		scriptName(), id(), realCell_, spaceID_, position().x, position().y, position().z));
	
	// Must be done first
	addToStream(s);

	//witnesses_.clear();
	//witnesses_count_ = 0;

	if(pControllers_)
	{
		pControllers_->clear();
		SAFE_RELEASE(pControllers_);
	}

	if(pWitness())
	{
		pWitness()->clear(this);
		Witness::reclaimPoolObject(pWitness_);
		pWitness_ = NULL;
	}

	scriptTimers_.cancelAll();
	pyCallbackMgr_.finalise();
}

//-------------------------------------------------------------------------------------
void Entity::changeToReal(COMPONENT_ID ghostCell, KBEngine::MemoryStream& s)
{
	// Convert Ghost to a real Entity
	// First, need to set our ghostCell
	// Add all def data into the stream
	// Deserialize the controller and stop all controllers (timer, navigate, trap,...)
	// Deserialize and install Witness
	KBE_ASSERT(isReal() == false && "Entity::changeToReal(): not is ghost.\n");

	ghostCell_ = ghostCell;
	realCell_ = 0;

	DEBUG_MSG(fmt::format("{}::changeToReal(): {}, ghostCell={}, spaceID={}, position=({},{},{}).\n",
		scriptName(), id(), ghostCell_, spaceID_, position().x, position().y, position().z));

	createFromStream(s);
}

//-------------------------------------------------------------------------------------
void Entity::addToStream(KBEngine::MemoryStream& s)
{
	COMPONENT_ID baseEntityCallComponentID = 0;
	if(baseEntityCall_)
	{
		baseEntityCallComponentID = baseEntityCall_->componentID();
	}

	bool hasCustomVolatileinfo = (pCustomVolatileinfo_ != NULL);
	ENTITY_ID controlledByID = (controlledBy_ != NULL ? controlledBy_->id() : 0);
		
	s << pScriptModule_->getUType() << spaceID_ << isDestroyed_ << 
		isOnGround_ << topSpeed_ << topSpeedY_ << 
		layer_ << baseEntityCallComponentID << hasCustomVolatileinfo << controlledByID;

	if (pCustomVolatileinfo_)
		pCustomVolatileinfo_->addToStream(s);

	addCellDataToStream(CELLAPP_TYPE, ENTITY_CELL_DATA_FLAGS, &s);
	
	addMovementHandlerToStream(s);
	addControllersToStream(s);
	addWitnessToStream(s);
	addTimersToStream(s);
	addEventsToStream(s);

	pyCallbackMgr_.addToStream(s);
}

//-------------------------------------------------------------------------------------
void Entity::createFromStream(KBEngine::MemoryStream& s)
{
	ENTITY_SCRIPT_UID scriptUType;
	COMPONENT_ID baseEntityCallComponentID;
	bool hasCustomVolatileinfo;
	ENTITY_ID controlledByID;

	s >> scriptUType >> spaceID_ >> isDestroyed_ >> isOnGround_ >> topSpeed_ >> 
		topSpeedY_ >> layer_ >> baseEntityCallComponentID >> hasCustomVolatileinfo >> controlledByID;

	if (hasCustomVolatileinfo)
	{
		if (!pCustomVolatileinfo_)
			pCustomVolatileinfo_ = new VolatileInfo();

		pCustomVolatileinfo_->createFromStream(s);
	}

	// At this time, it is forcibly set to not be on the ground, it cannot be judged whether it is on the ground,
	//  and the character needs the client to report on the ground.
	// The server's NPC is then judged whether it is on the ground after the move.
	isOnGround_ = false;

	this->pScriptModule_ = EntityDef::findScriptModule(scriptUType);

	KBE_ASSERT(this->pScriptModule_);

	// Set entity's baseEntityCall
	if(baseEntityCallComponentID > 0)
		baseEntityCall(new EntityCall(pScriptModule(), NULL, baseEntityCallComponentID, id_, ENTITYCALL_TYPE_BASE));

	// If the controller before the transfer is the system or its own client, keep it
	// If another client is in control, try to restore the control relationship and reset it if it cannot be restored
	if (controlledByID == id())
		controlledBy(baseEntityCall());
	else if (controlledByID == 0)
		controlledBy(NULL);
	else
	{
		Entity* controllerEntity = Cellapp::getSingleton().findEntity(controlledByID);
		if (controllerEntity && spaceID() == controllerEntity->spaceID() && \
			controllerEntity->clientEntityCall())
			controlledBy(controllerEntity->baseEntityCall());
		else
			setControlledBy(baseEntityCall());
	}

	PyObject* cellData = createCellDataFromStream(&s);
	createNamespace(cellData);
	Py_XDECREF(cellData);

	removeFlags(ENTITY_FLAGS_INITING);
	
	createEventsFromStream(s);
	createMovementHandlerFromStream(s);
	createControllersFromStream(s);
	createWitnessFromStream(s);
	createTimersFromStream(s);

	pyCallbackMgr_.createFromStream(s);
	setDirty();
}

//-------------------------------------------------------------------------------------
void Entity::addControllersToStream(KBEngine::MemoryStream& s)
{
	if(pControllers_)
	{
		s << true;

		// Must clean up move Controllers
		stopMove();

		pControllers_->addToStream(s);
	}
	else
	{
		s << false;
	}
}

//-------------------------------------------------------------------------------------
void Entity::createControllersFromStream(KBEngine::MemoryStream& s)
{
	bool hasControllers;
	s >> hasControllers;

	if(hasControllers)
	{
		if(pControllers_ == NULL)
			pControllers_ = new Controllers(id());

		pControllers_->createFromStream(s);
	}
}

//-------------------------------------------------------------------------------------
void Entity::addWitnessToStream(KBEngine::MemoryStream& s)
{
	uint32 size = witnesses_count_;
	s << size;

	std::list<ENTITY_ID>::iterator iter = witnesses_.begin();
	for(; iter != witnesses_.end(); ++iter)
	{
		s << (*iter);
	}

	if(pWitness())
	{
		s << true;
		pWitness()->addToStream(s);
	}
	else
	{
		s << false;
	}
}

//-------------------------------------------------------------------------------------
void Entity::createWitnessFromStream(KBEngine::MemoryStream& s)
{
	uint32 size;
	s >> size;

	if (witnesses_count_ > 0)
	{
		WARNING_MSG(fmt::format("{}::createWitnessFromStream: witnesses_count({}/{}) != 0! entityID={}, isReal={}\n",
			scriptName(), witnesses_.size(), witnesses_count_, id(), isReal()));

		/*
		std::list<ENTITY_ID>::iterator it = witnesses_.begin();
		for (; it != witnesses_.end(); ++it)
		{
			Entity *ent = Cellapp::getSingleton().findEntity((*it));

			if (ent)
			{
				bool inTargetView = false;

				if (ent->pWitness())
				{
					Witness::VIEW_ENTITIES::iterator view_iter = ent->pWitness()->viewEntities().begin();
					for (; view_iter != ent->pWitness()->viewEntities().end(); ++view_iter)
					{
						if ((*view_iter)->pEntity() == this)
						{
							inTargetView = true;
							break;
						}
					}
				}

				ERROR_MSG(fmt::format("\t=>witnessed={}({}), isDestroyed={}, isReal={}, inTargetView={}, spaceID={}, position=({},{},{})\n",
					ent->scriptName(), (*it), ent->isDestroyed(), ent->isReal(), inTargetView, ent->spaceID(), ent->position().x, ent->position().y, ent->position().z));
			}
			else
			{
				ERROR_MSG(fmt::format("\t=> witnessed={}, not found entity!\n", (*it)));
			}
		}

		KBE_ASSERT(witnesses_count_ == 0);
		*/
		
		for (uint32 i = 0; i < size; ++i)
		{
			ENTITY_ID entityID;
			s >> entityID;
		}
	}
	else
	{
		for (uint32 i = 0; i < size; ++i)
		{
			ENTITY_ID entityID;
			s >> entityID;

			Entity* pEntity = Cellapp::getSingleton().findEntity(entityID);
			if (pEntity == NULL || pEntity->spaceID() != spaceID())
				continue;

			witnesses_.push_back(entityID);
			++witnesses_count_;
		}
	}

	bool hasWitness;
	s >> hasWitness;

	if(hasWitness)
	{
		PyObject* clientMB = PyObject_GetAttrString(baseEntityCall(), "client");
		KBE_ASSERT(clientMB != Py_None);

		EntityCall* client = static_cast<EntityCall*>(clientMB);	
		clientEntityCall(client);

		// Do not use setWitness, because you do not need to go onAttach process, the client does not need to enterWorld again.
		// setWitness(Witness::createPoolObject());
		pWitness_ = Witness::createPoolObject();
		pWitness_->pEntity(this);
		pWitness_->createFromStream(s);
	}
}

//-------------------------------------------------------------------------------------
void Entity::addMovementHandlerToStream(KBEngine::MemoryStream& s)
{
	if(pMoveController_)
	{
		s << true;
		pMoveController_->addToStream(s);
	}
	else
	{
		s << false;
	}
	
	if(pTurnController_)
	{
		s << true;
		pTurnController_->addToStream(s);
	}
	else
	{
		s << false;
	}	
}

//-------------------------------------------------------------------------------------
void Entity::createMovementHandlerFromStream(KBEngine::MemoryStream& s)
{
	bool hasMoveHandler;
	s >> hasMoveHandler;

	if(hasMoveHandler)
	{
		stopMove();

		pMoveController_ = KBEShared_ptr<Controller>(new MoveController(this));
		pMoveController_->createFromStream(s);
		pControllers_->add(pMoveController_);
	}
	
	bool hasTurnHandler;
	s >> hasTurnHandler;

	if(hasTurnHandler)
	{
		if(!hasMoveHandler)
			stopMove();
		
		pTurnController_ = KBEShared_ptr<Controller>(new TurnController(this));
		pTurnController_->createFromStream(s);
		pControllers_->add(pTurnController_);
	}	
}

//-------------------------------------------------------------------------------------
void Entity::onTimer(ScriptID timerID, int useraAgs)
{
	SCOPED_PROFILE(ONTIMER_PROFILE);

	bufferOrExeCallback(const_cast<char*>("onTimer"),
		Py_BuildValue(const_cast<char*>("(Ii)"), timerID, useraAgs));
}

//-------------------------------------------------------------------------------------
void Entity::addTimersToStream(KBEngine::MemoryStream& s)
{
	ScriptTimers::Map& map = scriptTimers_.map();
	uint32 size = map.size();
	s << size;

	ScriptTimers::Map::const_iterator iter = map.begin();
	while (iter != map.end())
	{
		// timerID
		s << iter->first;

		uint32 time;
		uint32 interval;
		void* pUser;

		Cellapp::getSingleton().timers().getTimerInfo(iter->second, time, interval, pUser);
		int32 userData = int32(uintptr(pUser));
		s << time << interval << userData;
		++iter;
	}
}

//-------------------------------------------------------------------------------------
void Entity::createTimersFromStream(KBEngine::MemoryStream& s)
{
	uint32 size;
	s >> size;

	for(uint32 i=0; i<size; ++i)
	{
		ScriptID tid;
		uint32 time;
		uint32 interval;
		int32 userData = 0;

		s >> tid >> time >> interval >> userData;

		EntityScriptTimerHandler* pEntityScriptTimerHandler = new EntityScriptTimerHandler(this);

		TimerHandle timerHandle = Cellapp::getSingleton().timers().add(
				time, interval,
				pEntityScriptTimerHandler, (void *)(intptr_t)userData);
		
		scriptTimers_.directAddTimer(tid, timerHandle);
	}
}

//-------------------------------------------------------------------------------------
void Entity::addEventsToStream(KBEngine::MemoryStream& s)
{
	ENTITY_EVENTS& eventsMap = events();

	int eventNameSize = eventsMap.size();
	s << eventNameSize;

	ENTITY_EVENTS::const_iterator mapiter = eventsMap.begin();
	for (; mapiter != eventsMap.end(); ++mapiter)
	{
		int eventSize = mapiter->second.size();
		s << mapiter->first << eventSize;

		std::vector<PyObjectPtr>::const_iterator vecIter = mapiter->second.begin();
		for (; vecIter != mapiter->second.end(); ++vecIter)
		{
			PyObject* pyObj = PyObject_GetAttrString((*vecIter).get(), "__qualname__");

			if (!pyObj)
			{
				SCRIPT_ERROR_CHECK();

				ERROR_MSG(fmt::format("{}::addEventsToStream: get info error! eventName={}\n",
					scriptName(), mapiter->first));

				continue;
			}

			wchar_t* PyUnicode_AsWideCharStringRet0 = PyUnicode_AsWideCharString(pyObj, NULL);
			char* ccattr = strutil::wchar2char(PyUnicode_AsWideCharStringRet0);
			PyMem_Free(PyUnicode_AsWideCharStringRet0);

			char *pClass;
			char *pMethod;

			pClass = strtok(ccattr, ".");
			pMethod = strtok(NULL, ".");

			ScriptDefModule* pScriptDefModule = EntityDef::findScriptModule(pClass);

			if (!pMethod || !pScriptDefModule || !PyObject_HasAttrString((PyObject*)pScriptDefModule->getScriptType(), pMethod))
			{
				ERROR_MSG(fmt::format("{}::addEventsToStream: not found [{}] in Entity or EntityComponent, will ignore packing it! eventName={}\n",
					scriptName(), ccattr, mapiter->first));

				s << "None";
			}
			else
			{
				if (pScriptDefModule->isComponentModule())
				{
					PyObject* objSelf = PyMethod_Self((*vecIter).get());
					if (!objSelf)
					{
						ERROR_MSG(fmt::format("{}::addEventsToStream: not found Self[{}], will ignore packing it! eventName={}\n",
							scriptName(), ccattr, mapiter->first));

						s << "None";
					}
					else
					{
						PyObject* pyObj1 = PyObject_GetAttrString(objSelf, "name");

						if (!pyObj1)
						{
							SCRIPT_ERROR_CHECK();

							ERROR_MSG(fmt::format("{}::addEventsToStream: get EntityComponent name error! eventName={}\n",
								scriptName(), mapiter->first));

							s << "None";
						}
						else
						{
							wchar_t* PyUnicode_AsWideCharStringRet1 = PyUnicode_AsWideCharString(pyObj1, NULL);
							char* ccattr1 = strutil::wchar2char(PyUnicode_AsWideCharStringRet1);
							PyMem_Free(PyUnicode_AsWideCharStringRet1);

							s << fmt::format("{}.{}", ccattr1, pMethod);

							free(ccattr1);
							S_RELEASE(pyObj1);
						}
					}
				}
				else
				{
					s << pMethod;
				}
			}

			free(ccattr);
			S_RELEASE(pyObj);
		}
	}
}

//-------------------------------------------------------------------------------------
void Entity::createEventsFromStream(KBEngine::MemoryStream& s)
{
	ENTITY_EVENTS& eventsMap = events();
	eventsMap.clear();

	int eventNameSize;
	s >> eventNameSize;

	while(eventNameSize-- > 0)
	{
		std::string eventName;
		s >> eventName;

		int eventSize;
		s >> eventSize;

		while (eventSize-- > 0)
		{
			std::string callbackName;
			s >> callbackName;

			if (eventName == "None")
				continue;

			std::vector<std::string> callBackNameVec;

			KBEngine::strutil::kbe_split(callbackName, '.', callBackNameVec);

			PyObject* pyCallback = NULL;

			if (callBackNameVec.size() >= 2)
			{
				PyObject* pyObj = PyObject_GetAttrString(this, callBackNameVec[0].c_str());
				KBE_ASSERT(pyObj);

				pyCallback = PyObject_GetAttrString(pyObj, callBackNameVec[1].c_str());
				Py_DECREF(pyObj);
			}
			else
			{
				pyCallback = PyObject_GetAttrString(this, callBackNameVec[0].c_str());
			}

			KBE_ASSERT(pyCallback);
			registerEvent(eventName, pyCallback);
			Py_DECREF(pyCallback);
		}
	}
}

//-------------------------------------------------------------------------------------
}
