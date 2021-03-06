#include "precompiled.h"

/*
* Hook API function that are available into enum
* Look at the enum's for parameter lists.
*
* @param function	The function to hook.
* @param callback	The forward to call.
* @param post		Whether or not to forward this in post.
* @return 		Returns a handle to the hook. Use EnableHookChain/DisableHookChain to toggle the forward on or off.
*
* native RegisterHookChain(any:function_id, const callback[], post = 0);
*/
cell AMX_NATIVE_CALL RegisterHookChain(AMX *amx, cell *params)
{
	enum args_e { arg_count, arg_func, arg_handler, arg_post };

	int func = params[arg_func];
	int post = params[arg_post];
	auto hook = g_hookManager.getHook(func);

	if (unlikely(hook == nullptr))
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s: function with id (%d) doesn't exist in current API version.", __FUNCTION__, func);
		return INVALID_HOOKCHAIN;
	}

	if (unlikely(!hook->checkRequirements()))
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s: function (%s) is not available, %s required.", __FUNCTION__, hook->func_name, hook->depend_name);
		return INVALID_HOOKCHAIN;
	}

	int funcid;
	const char *funcname = getAmxString(amx, params[arg_handler]);
	if (unlikely(g_amxxapi.amx_FindPublic(amx, funcname, &funcid) != AMX_ERR_NONE))
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s: public function \"%s\" not found.", __FUNCTION__, funcname);
		return INVALID_HOOKCHAIN;
	}

	int fwid = hook->registerForward(amx, funcname);
	if (unlikely(fwid == -1))
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s: register forward failed.", __FUNCTION__);
		return INVALID_HOOKCHAIN;
	}

	return g_hookManager.addHandler(amx, func, fwid, post != 0);
}

/*
* Enable hook by handle.
* Use the return value from RegisterHookChain as the parameter here!
*
* @param fwd		The hook to re-enable.
* @return		Returns if the function is successful executed true otherwise false
*
* native bool:EnableHookChain(any:fwd);
*/
cell AMX_NATIVE_CALL EnableHookChain(AMX *amx, cell *params)
{
	enum args_e { arg_count, arg_handle_hook };

	auto hook = g_hookManager.getAmxxHook(params[arg_handle_hook]);

	if (unlikely(hook == nullptr))
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s: invalid HookChain handle.", __FUNCTION__);
		return FALSE;
	}

	hook->SetState(FSTATE_ENABLED);
	return TRUE;
}

/*
* Disable hook by handle.
* Use the return value from RegisterHookChain as the parameter here!
*
* @param fwd		The hook to stop.
*
* native bool:DisableHookChain(any:fwd);
*/
cell AMX_NATIVE_CALL DisableHookChain(AMX *amx, cell *params)
{
	enum args_e { arg_count, arg_handle_hook };

	auto hook = g_hookManager.getAmxxHook(params[arg_handle_hook]);

	if (unlikely(hook == nullptr))
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s: invalid HookChain handle.", __FUNCTION__);
		return FALSE;
	}

	hook->SetState(FSTATE_STOPPED);
	return TRUE;
}

/*
* Sets the return value of a hookchain.
* This needs to be used in conjunction with RH_OVERRIDE or RH_SUPERCEDE.
*
* @param type		To specify the type ATYPE_*, look at the enum AType
* @param value		The value to set the return to.
*
* native SetHookChainReturn(AType:type, any:...);
*/
cell AMX_NATIVE_CALL SetHookChainReturn(AMX *amx, cell *params)
{
	if (unlikely(!g_hookCtx))
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s: trying to set return value without active hook.", __FUNCTION__);
		return FALSE;
	}

	enum args_e { arg_count, arg_type, arg_value };
	auto& retVal = g_hookCtx->retVal;

	if (unlikely(params[arg_type] != retVal.type))
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s: trying to set return value with incompatible type.", __FUNCTION__);
		return FALSE;
	}

	cell* srcAddr = getAmxAddr(amx, params[arg_value]);

	switch (retVal.type)
	{
	case ATYPE_INTEGER:
	case ATYPE_FLOAT:
		retVal._integer = *srcAddr;
		break;

	case ATYPE_STRING:
	{
		if (retVal._string != nullptr)
			delete[] retVal._string;

		size_t len;
		const char *dest = getAmxString(srcAddr, &len);
		retVal._string = strcpy(new char[len + 1], dest);
		break;
	}
	case ATYPE_CLASSPTR:
		retVal._classptr = getPrivate<CBaseEntity>(*srcAddr);
		break;
	case ATYPE_EDICT:
		retVal._edict = edictByIndexAmx(*srcAddr);
		break;
	case ATYPE_EVARS:
		retVal._pev = PEV(*srcAddr);
		break;
	default:
		return FALSE;
	}

	retVal.set = true;
	return TRUE;
}

/*
* Gets the return value of the current hookchain.
* This has no effect in pre hookchain.
*
* @param [maxlen]	Max length of string (optional)
* @return		If an integer or boolean or one byte or float, array or everything else is passed via 1rd argument and more
*
* native any:GetHookChainReturn(any:...);
*/
cell AMX_NATIVE_CALL GetHookChainReturn(AMX *amx, cell *params)
{
	if (unlikely(!g_hookCtx))
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s: trying to get return value without active hook.", __FUNCTION__);
		return FALSE;
	}

	enum args_e { arg_count, arg_value, arg_maxlen };
	auto& retVal = g_hookCtx->retVal;

	if (unlikely(!retVal.set))
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s: return value isn't set.", __FUNCTION__);
		return FALSE;
	}

	cell* dstAddr = getAmxAddr(amx, params[arg_value]);

	switch (retVal.type)
	{
	case ATYPE_INTEGER:
	case ATYPE_FLOAT:
		return retVal._integer;
	case ATYPE_STRING:
	{
		if (PARAMS_COUNT != 2)
			return FALSE;

		setAmxString(dstAddr, retVal._string, params[arg_maxlen]);
		break;
	}
	case ATYPE_CLASSPTR:
		return indexOfEdict(retVal._classptr->pev);
	case ATYPE_EDICT:
		return indexOfEdict(retVal._edict);
	case ATYPE_EVARS:
		return indexOfEdict(retVal._pev);
	default:
		return FALSE;
	}

	return TRUE;
}

/*
* Set hookchain argument.
* This has no effect in post hookchain.
*
* @param number		Number of argument
* @param value		New value
* @param [maxlen]	Max length of string (optional)
* @return		Returns if the function is successful executed true otherwise false
*
* native SetHookChainArg(number, AType:type, any:...);
*/
cell AMX_NATIVE_CALL SetHookChainArg(AMX *amx, cell *params)
{
	if (unlikely(!g_hookCtx))
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s: trying to get return value without active hook.", __FUNCTION__);
		return FALSE;
	}

	enum args_e { arg_count, arg_number, arg_type, arg_value };
	size_t number = params[arg_number] - 1;

	if (unlikely(number >= g_hookCtx->args_count))
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s: can't set argument %i of hookchain with %i args.", __FUNCTION__, params[arg_number], g_hookCtx->args_count);
		return FALSE;
	}

	AType type = g_hookCtx->args_type[number];

	if (unlikely(params[arg_type] != type))
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s: invalid argument type provided.", __FUNCTION__);
		return FALSE;
	}

	cell* srcAddr = getAmxAddr(amx, params[arg_value]);
	size_t destAddr = g_hookCtx->args_ptr + number * sizeof(int);

	switch (type)
	{
	case ATYPE_INTEGER:
	case ATYPE_FLOAT:
		*(cell *)destAddr = *srcAddr;
		break;
	case ATYPE_STRING:
		*(char **)destAddr = getAmxStringTemp(srcAddr, g_hookCtx->get_temp_string(amx), CTempStrings::STRING_LEN);
		break;
	case ATYPE_CLASSPTR:
		*(CBaseEntity **)destAddr = getPrivate<CBaseEntity>(*srcAddr);
		break;
	case ATYPE_EDICT:
		*(edict_t **)destAddr = edictByIndexAmx(*srcAddr);
		break;
	case ATYPE_EVARS:
		*(entvars_t **)destAddr = PEV(*srcAddr);
		break;
	}

	return TRUE;
}

AMX_NATIVE_INFO HookChain_Natives[] =
{
	{ "RegisterHookChain", RegisterHookChain },

	{ "EnableHookChain", EnableHookChain },
	{ "DisableHookChain", DisableHookChain },

	{ "SetHookChainReturn", SetHookChainReturn },
	{ "GetHookChainReturn", GetHookChainReturn },

	{ "SetHookChainArg", SetHookChainArg },

	{ nullptr, nullptr }
};

void RegisterNatives_HookChains()
{
	if (!api_cfg.hasReHLDS() && !api_cfg.hasReGameDLL())
		fillNatives(HookChain_Natives, [](AMX *amx, cell *params) -> cell { MF_LogError(amx, AMX_ERR_NATIVE, "You need ReHlds or ReGameDll for use hookchains"); return FALSE; });
	
	g_amxxapi.AddNatives(HookChain_Natives);
}
